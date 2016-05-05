#include <config.h>
#include <protocols/dumprestore.h>

#ifndef _ARCHIVE_H
#define _ARCHIVE_H 1

#ifdef USE_QFA
#define	QFA_MAGIC	"495115637697"
#define QFA_VERSION	"1.0"
#endif /* USE_QFA */

/* state information when writing QFA file. */
typedef struct qfa_state {
	long long curtapepos;
	/* long		maxntrecs = 300000000 / (ntrec * 1024);	 last tested: 50 000 000 */
	long		maxntrecs;
	long		cntntrecs;
} QFA_State;

/* methods used to write 'Indexer' file. */
typedef struct indexer {
	struct indexer *next;
	int (*open) (const char *filename, int mode);
	int (*close) (void);
	int (*foo) (void); // what does this function do?
	int (*writerec) (const void *dp, int isspcl);
	int (*addInode) (struct dinode *dp, dump_ino_t ino, int metaonly);  // dump entry
	//int (*addDirEntry) (struct ext2_dir_entry *dirent, UNUSED(int offset),  // dump inode
	//	    UNUSED(int blocksize), UNUSED(char *buf), void *private);
	int (*addDirEntry) (struct direct *dp, dump_ino_t parent_ino);

	int (*openQfa) ();
	int (*closeQfa) ();
	int (*openQfaState) (QFA_State *state);
	int (*updateQfa) (QFA_State *state);
	int (*updateQfaState) (QFA_State *state);
} Indexer;

extern Indexer *indexer;

extern Indexer indexer_legacy;
extern Indexer indexer_sqlite;

//extern Indexer *indexer_legacy_factory();

//#ifdef HAVE_SQLITE3
//extern Indexer *indexer_sqlite_factory();
//#endif /* HAVE_SQLITE3 */
#endif
