
/*
 * Structures pulled from dump/tape.c. We need it here so the 'archive' code can
 * determine where we are in the tape.
 */
#ifndef _SLAVE_H
#define _SLAVE_H 1

/*
 * Concurrent dump mods (Caltech) - disk block reading and tape writing
 * are exported to several slave processes.  While one slave writes the
 * tape, the others read disk blocks; they pass control of the tape in
 * a ring via signals. The parent process traverses the filesystem and
 * sends writeheader()'s and lists of daddr's to the slaves via pipes.
 * The following structure defines the instruction packets sent to slaves.
 */
struct req {
	ext2_loff_t dblk;
	int count;
};

#define SLAVES 3		/* 1 slave writing, 1 reading, 1 for slack */

struct slave {
	int tapea;		/* header number at start of this chunk */
	int count;		/* count to next header (used for TS_TAPE */
				/* after EOT) */
	int inode;		/* inode that we are currently dealing with */
	int fd;			/* FD for this slave */
	int pid;		/* PID for this slave */
	int sent;		/* 1 == we've sent this slave requests */
	int firstrec;		/* record number of this block */
	char (*tblock)[TP_BSIZE]; /* buffer for data blocks */
	struct req *req;	/* buffer for requests */
};

extern struct slave *slp;

#endif
