#include <config.h>
#include <ctype.h>
#include <compaterr.h>
#include <fcntl.h>
#include <fstab.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <mntent.h>
#include <stdarg.h>

#include <sys/param.h>
#include <sys/time.h>
#include <time.h>
#ifdef __linux__
#include <linux/types.h>
#ifdef HAVE_EXT2FS_EXT2_FS_H
#include <ext2fs/ext2_fs.h>
#else
#include <linux/ext2_fs.h>
#endif
#include <ext2fs/ext2fs.h>
#include <sys/stat.h>
#include <bsdcompat.h>
#include <linux/fs.h>	/* for definition of BLKFLSBUF */
#elif defined sunos
#include <sys/vnode.h>

#include <ufs/inode.h>
#include <ufs/fs.h>
#else
#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>
#endif

#include <protocols/dumprestore.h>

//#include "dump.h"
#include "indexer.h"
#include "pathnames.h"
#include "bylabel.h"

extern int tapeno;
extern dump_ino_t volinfo[]; // TP_NINOS

extern void msg __P((const char *fmt,...));

#ifdef __linux__
extern ext2_filsys fs;
#else /* __linux__ */
extern struct fs *sblock;
#endif /* __linux__ */

#ifdef HAVE_SQLITE3
#include <sqlite3.h>
#include <uuid/uuid.h>

static sqlite3 *db;

// convenience macro that handles a basic sql statement.
#define EXEC(db, buffer) \
	rc = sqlite3_exec(db, buffer, NULL, 0, &errMsg); \
	if (rc != SQLITE_OK) { \
		msg("(%s:%d) SQL error: %s\n", __FILE__, __LINE__, sqlite3_errmsg(db)); \
		msg(buffer); \
		sqlite3_free(errMsg); \
		sqlite3_close(db); \
		db = NULL; \
		return -1; \
	}

/* prepared statement that allows us to use placeholder values. */
/* it also acts as sentinel to indicate if this is read or write mode. */
sqlite3_stmt *stmt = NULL;

/*
 * Ppopulate the 'backup' table.
 */
static int
sqlite_populate_backup_table()
{
	int rc;
	char *errMsg = NULL;
	char buffer[2000];
	char vol_uuid[40];
	char ts[40];
	char nts[40];
	char fs_uuid[40];
	time_t now;
	const char *tail;
	uuid_t uuid;

	time(&now);

	// populate 'backup table'. There will only be one entry.
	uuid_generate_time(uuid);
	uuid_unparse_lower(uuid, vol_uuid);
	uuid_unparse_lower(fs->super->s_uuid, fs_uuid);
	strftime(ts, sizeof ts, "%FT%T", gmtime((const time_t *) &spcl.c_date));
	strftime(nts, sizeof nts, "%FT%T", gmtime((const time_t *) &now));
	snprintf(buffer, sizeof buffer,
		"insert into backup(backup_id, vol_uuid, dt, start_dt, fs_uuid, dev, label, host, level) values (1, '%s', '%s', '%s', '%s', ?, ?, ?, %d)",
		vol_uuid, ts, nts, fs_uuid, spcl.c_level);

//		spcl.c_dev, spcl.c_label, spcl.c_host,

	// prepare statement.
	rc = sqlite3_prepare_v2(db, buffer, strlen(buffer), &stmt, &tail);
	if (rc != SQLITE_OK) {
		msg("(%s:%d) SQL error: %s\n", __FILE__, __LINE__, sqlite3_errmsg(db));
		msg(buffer);
		sqlite3_free(errMsg);
		sqlite3_close(db);
		db = NULL;
		return -1;
	}

	sqlite3_bind_text(stmt, 1, spcl.c_dev, strlen(spcl.c_dev), SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, spcl.c_label, strlen(spcl.c_label), SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, spcl.c_host, strlen(spcl.c_host), SQLITE_TRANSIENT);

	while ((rc = sqlite3_step(stmt)) == SQLITE_BUSY) {
		continue;
	}

	if (rc != SQLITE_DONE) {
		msg("(%s:%d) SQL error: %s\n", __FILE__, __LINE__, sqlite3_errmsg(db));
		msg("(%s:%d) %s\n", __FILE__, __LINE__, sqlite3_sql(stmt));
		sqlite3_free(errMsg);
		sqlite3_close(db);
		db = NULL;
		return -1;
	}

	rc = sqlite3_finalize(stmt);
	if (rc != SQLITE_OK) {
		msg("(%s:%d) SQL error: %s\n", __FILE__, __LINE__, sqlite3_errmsg(db));
		msg("(%s:%d) %s\n", __FILE__, __LINE__, sqlite3_sql(stmt));
		sqlite3_free(errMsg);
		sqlite3_close(db);
		db = NULL;
		return -1;
	}

	return 0;
}

/*
 * Create database schema
 */
static int
sqlite_create_schema(void)
{
	int rc;
	char *errMsg = NULL;
	char buffer[2000];
	const char *tail;

	// turn on foreign keys
	snprintf(buffer, sizeof buffer, "pragma foreign_keys = ON");

	EXEC(db, buffer);

	// turn off synchronous access. we have bigger problems
	// if we lose power in the middle of a backup.
	snprintf(buffer, sizeof buffer, "pragma synchronous = OFF\n");

	EXEC(db, buffer);

	// create 'backup' table.
	snprintf(buffer, sizeof buffer,
		"create table backup(backup_id int primary key not null, vol_uuid char[36] not null, dt timestamp not null, start_dt timestamp not null, end_dt timestamp, fs_uuid char[36] not null, dev varchar[%1$d] not null, label varchar[%1$d] not null, host varchar[%1$d] not null, level int not null, blocks int, volumes int)\n", EXT2_NAME_LEN);

	EXEC(db, buffer);

	snprintf(buffer, sizeof buffer,
		"create unique index i1 on backup(backup_id)\n");
	EXEC(db, buffer);

	// create 'entry' table. This contains information about
	// each directory entry
	snprintf(buffer, sizeof buffer,
		"create table entry(backup_id int not null, ino int not null, parent_ino int not null, type int not null, depth int, name varchar[%d] not null, path varchar[%d], foreign key(backup_id) references backup(backup_id))\n", EXT2_NAME_LEN, 1024);

	//	"create table entry(backup_id int not null, ino int not null, parent_ino int not null, type int not null, depth int, name varchar[%d] not null, path varchar[%d], foreign key(backup_id) references backup(backup_id), foreign key(parent_ino) references entry(ino))\n", EXT2_NAME_LEN, 1024);

	EXEC(db, buffer);

	// snprintf(buffer, sizeof buffer,
	//	"create unique index i2 on entry(ino)\n");

	// EXEC(db, buffer);

	snprintf(buffer, sizeof buffer,
		"create trigger t1 after insert on entry begin update entry set path = coalesce((select '.' where name = '.'), (select A.path || '/' || new.name from entry A where A.ino = new.parent_ino)) where rowid = new.rowid; end;\n");

	EXEC(db, buffer);

	// create 'inode' table. This contains information about each inode.
	snprintf(buffer, sizeof buffer,
		"create table inode(backup_id int not null, ino int not null, is_deleted char[1] not null default 'N', mode int not null, nlink int not null, uid int not null, gid int not null, rdev int not null, size int not null, atime timestamp not null, mtime timestamp not null, ctime timestamp not null, has_xattr char[1] not null default 'N', has_acl char[1] not null default 'N', volume int not null, recno int not null, foreign key(backup_id) references backup(backup_id))\n");

	//	"create table inode(backup_id int not null, ino int not null, is_deleted char[1] not null default 'N', mode int not null, nlink int not null, uid int not null, gid int not null, rdev int not null, size int not null, atime timestamp not null, mtime timestamp not null, ctime timestamp not null, has_xattr char[1] not null default 'N', has_acl char[1] not null default 'N', volume int not null, recno int not null, foreign key(backup_id) references backup(backup_id), foreign key(ino) references entry(ino))\n");

	EXEC(db, buffer);

	sqlite_populate_backup_table();

	// prepare statement for entry inserts.
	snprintf(buffer, sizeof buffer,
		"insert into entry(backup_id, ino, parent_ino, type, name) values(1, ?, ?, ?, ?)\n");

	rc = sqlite3_prepare_v2(db, buffer, strlen(buffer), &stmt, &tail);
	if (rc != SQLITE_OK) {
		msg("(%s:%d) SQL error: %s\n", __FILE__, __LINE__, sqlite3_errmsg(db));
		msg(buffer);
		sqlite3_free(errMsg);
		sqlite3_close(db);
		db = NULL;
		return -1;
	}

	return 0;
}

/*
 * Open database.
 */
static int
sqlite_open(const char *filename, int mode)
{
	if (mode == 0) {
		if (sqlite3_open(filename, &db)) {
			msg("Unable to open database: %s\n", sqlite3_errmsg(db));
			db = NULL;
			return -1;
		}
		stmt = NULL;
	} else if (mode == 1) {
		unlink(filename);

		/* sqlite will always create DB if it doesn't already exist. */
		if (sqlite3_open(filename, &db)) {
			msg("Unable to open database: %s\n", sqlite3_errmsg(db));
			db = NULL;
			return -1;
		}

		sqlite_create_schema();
	}

	return 0;
}

/*
 * Close database.
 */
static int
sqlite_close()
{
	int rc;
	char *errMsg = NULL;
	char buffer[2000];
	char uuid[40];
	char ts[40];
	time_t now;

	if (db == NULL) {
		msg("database is already closed!");
		return -1;
	}

	time(&now);

	/* if stmt is not null we're creating database */
	if (stmt != NULL) {
		// close prepared statement.
		rc = sqlite3_finalize(stmt);
		if (rc != SQLITE_OK) {
			msg("(%s:%d) SQL error: %s\n", __FILE__, __LINE__, sqlite3_errmsg(db));
			msg("(%s:%d) %s\n", __FILE__, __LINE__, sqlite3_sql(stmt));
			sqlite3_free(errMsg);
			sqlite3_close(db);
			db = NULL;
			return -1;
		}

		// update 'tape header' with number of volumes and blocks.
		uuid_unparse_lower(fs->super->s_uuid, uuid);
		strftime(ts, sizeof ts, "%FT%T", gmtime((const time_t *) &now));
		snprintf(buffer, sizeof buffer,
				"update backup set end_dt = '%s', blocks=%d, volumes=%d where fs_uuid='%s'",
				ts, spcl.c_tapea, spcl.c_volume, uuid);

		EXEC(db, buffer);
	}

	// close database.
	rc = sqlite3_close(db);
	if (rc != SQLITE_OK) {
		msg("(%s:%d) SQL error: %s\n", __FILE__, __LINE__, sqlite3_errmsg(db));
		msg(buffer);
		sqlite3_free(errMsg);
		db = NULL;
		return -1;
	}
	db = NULL;

	return 0;
}

/*
 * Add directory entry.
 */
static int
sqlite_addDirEntry(struct direct *dp, dump_ino_t parent_ino)
{
	int rc;
	char *errMsg = NULL;
	char buffer[2000];
	const char *tail;

	if (db == NULL) {
		return -1;
	}

	// don't include backlink.
	if (!strcmp(dp->d_name, "..")) {
		return -1;
	}

	sqlite3_bind_int(stmt, 1, dp->d_ino);
	sqlite3_bind_int(stmt, 2, parent_ino);
	sqlite3_bind_int(stmt, 3, dp->d_type);
	sqlite3_bind_text(stmt, 4, dp->d_name, strlen(dp->d_name), SQLITE_TRANSIENT);

	while ((rc = sqlite3_step(stmt)) == SQLITE_BUSY) {
		continue;
	}

	if (rc != SQLITE_DONE) {
		int i;
		msg("(%s:%d) SQL error: %s\n", __FILE__, __LINE__, sqlite3_errmsg(db));
		msg("(%s:%d) %s\n", __FILE__, __LINE__, sqlite3_sql(stmt));
		sqlite3_free(errMsg);
		sqlite3_close(db);
		db = NULL;
		return -1;
	}

	rc = sqlite3_reset(stmt);
	if (rc != SQLITE_OK) {
		msg("(%s:%d) SQL error: %s\n", __FILE__, __LINE__, sqlite3_errmsg(db));
		msg(buffer);
		sqlite3_free(errMsg);
		sqlite3_close(db);
		db = NULL;
		return -1;
	}

	rc = sqlite3_clear_bindings(stmt);
	if (rc != SQLITE_OK) {
		msg("(%s:%d) SQL error: %s\n", __FILE__, __LINE__, sqlite3_errmsg(db));
		msg(buffer);
		sqlite3_free(errMsg);
		sqlite3_close(db);
		db = NULL;
		return -1;
	}

	return 0;
}

/*
 * Add inode entry.
 */
static int
sqlite_addInode(struct dinode *dp, dump_ino_t ino, int metadata_only)
{
	int rc;
	char *errMsg = NULL;
	char buffer[2000];
	char ats[40];
	char mts[40];
	char cts[40];
	time_t t;

	if (db == NULL) {
		return -1;
	}

	ats[0] = 0;
	mts[0] = 0;
	cts[0] = 0;

	t = dp->di_atime;
	strftime(ats, sizeof ats, "%FT%T", gmtime(&t));
	t = dp->di_mtime;
	strftime(mts, sizeof ats, "%FT%T", gmtime(&t));
	t = dp->di_ctime;
	strftime(cts, sizeof ats, "%FT%T", gmtime(&t));

	// xattr: dp->di_extraisize != 0
	// acl: dp->di_file_acl (inode?)
	// se linux?...

	snprintf(buffer, sizeof buffer,
		"insert into inode(backup_id, ino, is_deleted, mode, nlink, uid, gid, rdev, size, atime, mtime, ctime, has_xattr, has_acl, volume, recno) values(1, %d, '%s', %d, %d, %d, %d, %d, %d, '%s', '%s', '%s', '%s', '%s', %d, %d)\n",
		ino, "N", dp->di_mode, dp->di_nlink, dp->di_uid, dp->di_gid, dp->di_rdev, dp->di_size, ats, mts, cts, (dp->di_extraisize == 0) ? "N" : "Y", (dp->di_file_acl != 0) ? "Y" : "N", spcl.c_volume, spcl.c_tapea);

	EXEC(db, buffer);

	return 0;
}

/*
 *
 */
static int
foo()
{
	return 0;
}

/*
 *
 */
static int
sqlite_foo()
{
	return 0;
}

/*
 *
 */
static int
sqlite_writerec(const void *dp, int isspcl)
{
	return 0;
}

/*
 *
 */
static int
sqlite_openQfa()
{
	return 0;
}

/*
 *
 */
static int
sqlite_closeQfa()
{
	return 0;
}

/*
 *
 */
static int
sqlite_openQfaState(QFA_State *state)
{
	return 0;
}

/*
 *
 */
static int
sqlite_updateQfaState(QFA_State *state)
{
	return 0;
}

/*
 *
 */
static int
sqlite_updateQfa(QFA_State *state)
{
	return 0;
}


/*
 *
 */
Indexer indexer_sqlite =
{
		NULL,
		&sqlite_open,
		&sqlite_close,
		&sqlite_foo,
		&sqlite_writerec,
		&sqlite_addInode,
		&sqlite_addDirEntry,

		&sqlite_openQfa,
		&sqlite_closeQfa,
		&sqlite_openQfaState,
		&sqlite_updateQfa,
		&sqlite_updateQfaState
};

#endif
