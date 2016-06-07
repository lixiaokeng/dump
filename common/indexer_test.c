#include <config.h>
#include <stdio.h>
#include <stdarg.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <limits.h>

#include <bsdcompat.h>
#include <protocols/dumprestore.h>

#ifdef __linux__
#include <linux/types.h>
#ifdef HAVE_EXT2FS_EXT2_FS_H
#include <ext2fs/ext2_fs.h>
#else
#include <linux/ext2_fs.h>
#endif
#include <ext2fs/ext2fs.h>
#elif defined sunos
#include <sys/vnode.h>
#include <ufs/inode.h>
#include <ufs/fs.h>
#else
#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>
#endif
#include <uuid/uuid.h>

#include "indexer.h"
#include "slave.h"

int notify;
dump_ino_t volinfo[TP_NINOS];

int nddates;
struct dumpdates **ddatev;
char *host = NULL;
int tapefd;
int tapeno;

struct slave slaves[SLAVES+1];
struct slave *slp;

union u_spcl u_spcl;

#ifdef __linux__
struct struct_ext2_filsys test_fs;
struct ext2_super_block test_super;
ext2_filsys fs = &test_fs;
#else
struct fs *sblock; /* the file system super block */
char sblock_buf[MAXBSIZE];
#endif

/* for dumprmt.c */
int dokerberos = 0;
int ntrec;
int abortifconnerr = 0;

/*
 *
 */
void
msg(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

/*
 * print error message and quit.
 */
int
quit(const char *msg)
{
	printf("%s\n", msg);
	exit(1);
}

/*
 * Compute and fill in checksum information. (from dump/traverse.c)
 */
void
mkchecksum(union u_spcl *tmpspcl)
{
	int32_t sum, cnt, *lp;

	tmpspcl->s_spcl.c_checksum = 0;
	lp = (int32_t *)&tmpspcl->s_spcl;
	sum = 0;
	cnt = sizeof(union u_spcl) / (4 * sizeof(int32_t));
	while (--cnt >= 0) {
		sum += *lp++;
		sum += *lp++;
		sum += *lp++;
		sum += *lp++;
	}
	tmpspcl->s_spcl.c_checksum = CHECKSUM - sum;
}

/*
 *
 */
int
dump_inode(Indexer *indexer, struct stat *buf)
{
	struct dinode dinode;

	memset(&dinode, 0, sizeof dinode);
	dinode.di_mode = buf->st_mode;
	dinode.di_nlink = buf->st_nlink;
	dinode.di_uid = buf->st_uid;
	dinode.di_gid = buf->st_gid;
	dinode.di_rdev = buf->st_rdev;
	dinode.di_size = buf->st_size;
	dinode.di_atime = buf->st_atime;
	dinode.di_mtime = buf->st_mtime;
	dinode.di_ctime = buf->st_ctime;
	dinode.di_blocks = buf->st_blocks;

	spcl.c_tapea++;

	indexer->addInode(&dinode, buf->st_ino, 0);
}

/*
 *
 */
int
dump_walk(Indexer *indexer, const char *root, int first)
{
	DIR *dirp;
	struct dirent *dp;
	char pathname[MAXPATHLEN];
	struct stat bufroot, bufentry;
	struct direct direct;

	dirp = opendir(root);
	if (dirp == NULL) {
		printf("%s:%d: trying to open '%s'\n", __FILE__, __LINE__, root);
		perror(root);
		return -1;
	}

	stat(root, &bufroot);

	//if (first) {
	//	spcl.c_volume = 1;
	//	spcl.c_tapea = 1;
	//	indexer->addDirEntry(&direct, direct.d_ino);
	//	dump_inode(indexer, &bufroot);
	//}

	while ((dp = readdir(dirp)) != NULL) {
		direct.d_ino = dp->d_ino;
		direct.d_reclen = dp->d_reclen;
		direct.d_type = dp->d_type;
		direct.d_namlen = strlen(dp->d_name);
		memcpy(direct.d_name, dp->d_name, MAXNAMLEN + 1);
		direct.d_name[MAXNAMLEN] = '\0';

		snprintf(pathname, sizeof pathname, "%s/%s", root, dp->d_name);
		stat(pathname, &bufentry);
		if (strcmp(dp->d_name, "..")) {
			indexer->addDirEntry(&direct, bufentry.st_ino);
			dump_inode(indexer, &bufentry);
			if (S_ISDIR(bufentry.st_mode)) {
				if (strcmp(dp->d_name, ".")) {
					dump_walk(indexer, pathname, 0);
				}
			}
		}
	}
	closedir(dirp);
}

/*
 *
 */
int
test_indexer(Indexer *indexer, const char *filename, const char *path)
{
	struct direct dp;
	struct stat buf;

#ifdef HAVE_UUID
	test_fs.super = &test_super;
	uuid_generate_time((unsigned char *)&test_fs.super->s_uuid);
#endif

	indexer->open(filename, 1);

	dump_walk(indexer, path, 1);
	indexer->close();

	indexer->open(filename, 0);
	indexer->close();
}

/*
 *
 */
int
main(int argc, char **argv)
{
	if (argc == 1) {
//		printf("usage: %s partition\n", argv[0]);
//		exit(1);
		argv[1] = "/usr/include";
	}

//	test_indexer(&indexer_legacy, "/tmp/archive.data", argv[1]);

#ifdef HAVE_SQLITE3
	test_indexer(&indexer_sqlite, "/tmp/t.sqlite3", argv[1]);
#endif /* HAVE_SQLITE3 */
}
