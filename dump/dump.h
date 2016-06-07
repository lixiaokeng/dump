/*
 *	Ported to Linux's Second Extended File System as part of the
 *	dump and restore backup suit
 *	Remy Card <card@Linux.EU.Org>, 1994-1997
 *	Stelian Pop <stelian@popies.net>, 1999-2000
 *	Stelian Pop <stelian@popies.net> - Alcôve <www.alcove.com>, 2000-2002
 */

/*-
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <config.h>
#include <protocols/dumprestore.h>
#include "transformation.h"

#define MAXINOPB	(MAXBSIZE / sizeof(struct dinode))
#define MAXNINDIR	(MAXBSIZE / sizeof(blk_t))
#define NUM_STR_SIZE	32	/* a generic number buffer size */

/*
 * Dump maps used to describe what is to be dumped.
 */
extern int	mapsize;	/* size of the state maps */
extern char	*usedinomap;	/* map of allocated inodes */
extern char	*dumpdirmap;	/* map of directories to be dumped */
extern char	*dumpinomap;	/* map of files to be dumped */
extern char	*metainomap;	/* which of the inodes in dumpinomap
				   will get only their metadata dumped */
/*
 * Map manipulation macros.
 */
#define	SETINO(ino, map) \
	map[(u_int)((ino) - 1) / NBBY] |=  1 << ((u_int)((ino) - 1) % NBBY)
#define	CLRINO(ino, map) \
	map[(u_int)((ino) - 1) / NBBY] &=  ~(1 << ((u_int)((ino) - 1) % NBBY))
#define	TSTINO(ino, map) \
	(map[(u_int)((ino) - 1) / NBBY] &  (1 << ((u_int)((ino) - 1) % NBBY)))

/*
 *	All calculations done in 0.1" units!
 */
extern char	*host;		/* name of the remote host */
extern const char *disk;	/* name of the disk file */
extern char	tape[MAXPATHLEN];/* name of the tape file */
extern char	*tapeprefix;	/* prefix of the tape file */
extern char	*dumpdates;	/* name of the file containing dump date information*/
extern char	lastlevel[NUM_STR_SIZE];/* dump level of previous dump */
extern char	level[NUM_STR_SIZE];/* dump level of this dump */
extern int	zipflag;	/* which compression method */
extern int	uflag;		/* update flag */
extern int	mflag;		/* dump metadata only if possible flag */
extern int	Mflag;		/* multi-volume flag */
extern int	qflag;		/* quit on errors flag */
extern int	vflag;		/* verbose flag */
extern int      breademax;      /* maximum number of bread errors before we quit */
extern char	*eot_script;	/* end of volume script fiag */
extern int	diskfd;		/* disk file descriptor */
extern int	tapefd;		/* tape file descriptor */
extern int	pipeout;	/* true => output to standard output */
extern int	fifoout;	/* true => output to fifo */
extern dump_ino_t curino;	/* current inumber; used globally */
extern int	newtape;	/* new tape flag */
extern int	density;	/* density in 0.1" units */
extern long long tapesize;	/* estimated tape size, blocks */
extern long	tsize;		/* tape size in 0.1" units */
extern long	asize;		/* number of 0.1" units written on current tape */
extern int	etapes;		/* estimated number of tapes */
extern int	nonodump;	/* if set, do not honor UF_NODUMP user flags */
extern int	unlimited;	/* if set, write to end of medium */
extern int	compressed;	/* if set, dump is to be compressed */
extern long long bytes_written;/* total bytes written to tape */
extern long	uncomprblks;	/* uncompressed blocks written to tape */
extern int	notify;		/* notify operator flag */
extern int	blockswritten;	/* number of blocks written on current tape */
extern int	tapeno;		/* current tape number */
extern time_t	tstart_writing;	/* when started writing the first tape block */
extern time_t	tend_writing;	/* after writing the last tape block */
#ifdef __linux__
extern ext2_filsys fs;
#else
extern struct	fs *sblock;	/* the file system super block */
extern char	sblock_buf[MAXBSIZE];
#endif
extern long	xferrate;       /* averaged transfer rate of all volumes */
extern long	dev_bsize;	/* block size of underlying disk device */
extern int	dev_bshift;	/* log2(dev_bsize) */
extern int	tp_bshift;	/* log2(TP_BSIZE) */
extern dump_ino_t volinfo[];	/* which inode on which volume archive info */
extern Transformation *transformation;

/* operator interface functions */
void	broadcast (const char *message);
time_t	do_stats (void);
void	lastdump (char arg);
void	msg (const char *fmt, ...);
void	msgtail (const char *fmt, ...);
int	query (const char *question);
void	quit (const char *fmt, ...);
void	set_operators (void);
#if defined(SIGINFO)
void	statussig (int signo);
#endif
void	timeest (void);
time_t	unctime (const char *str);

/* mapping rouintes */
struct	dinode;
long	blockest (struct dinode const *dp);
int	mapfiles (dump_ino_t maxino, long long *tapesize);
#ifdef	__linux__
int	mapfilesfromdir (dump_ino_t maxino, long long *tapesize, char *directory);
int	maponefile (dump_ino_t maxino, long long *tapesize, char *directory);
#endif
int	mapdirs (dump_ino_t maxino, long long *tapesize);

/* file dumping routines */
void	blksout (blk_t *blkp, int frags, dump_ino_t ino);
void	bread (ext2_loff_t blkno, char *buf, int size);
void	dumpino (struct dinode *dp, dump_ino_t ino, int metaonly);
#ifdef	__linux__
void	dumpdirino (struct dinode *dp, dump_ino_t ino);
#endif
void	dumpmap (char *map, int type, dump_ino_t ino);
void	writeheader (dump_ino_t ino);
void	mkchecksum (union u_spcl *tmpspcl);

/* tape writing routines */
int	alloctape (void);
void	close_rewind (void);
void	dumpblock (blk_t blkno, int size);
void	startnewtape (int top);
time_t	trewind (void);
void	writerec (const void *dp, int isspcl);
char	*mktimeest (time_t tnow);

void 	Exit (int status);
void	dumpabort (int signo);
void	getfstab (void);

const char *rawname (const char *cp);
struct	dinode *getino (dump_ino_t inum);

/* rdump routines */
#ifdef RDUMP
int	rmthost (const char *host);
int	rmtopen (const char *tape, const int mode);
void	rmtclose (void);
int	rmtread (char *buf, size_t count);
int	rmtwrite (const char *buf, size_t count);
off_t	rmtseek (off_t offset, int pos);
struct mtget * rmtstatus (void);
int	rmtioctl (int cmd, int count);
#endif /* RDUMP */

void	interrupt (int signo);	/* in case operator bangs on console */
int	exclude_ino (dump_ino_t ino);
void	do_exclude_ino (dump_ino_t ino, const char *);

/*
 *	Exit status codes
 */
#define	X_FINOK		0	/* normal exit */
#define	X_STARTUP	1	/* startup error */
#define	X_REWRITE	2	/* restart writing from the check point */
#define	X_ABORT		3	/* abort dump; don't attempt checkpointing */

#define	OPGRENT	"operator"		/* group entry to notify */
#ifdef	__linux__
#define DIALUP	"ttyS"			/* prefix for dialups */
#else
#define DIALUP	"ttyd"			/* prefix for dialups */
#endif

#include <mntent.h>

struct	mntent *fstabsearch (const char *key);	/* search fs_file and fs_spec */
#ifdef	__linux__
struct	mntent *fstabsearchdir (const char *key, char *dir);	/* search fs_file and fs_spec */
#endif

/*
 *	The contents of the file _PATH_DUMPDATES is maintained both on
 *	a linked list, and then (eventually) arrayified.
 */
struct dumpdates {
	char	dd_name[MAXPATHLEN+3];
	struct mntent *dd_fstab;
	int	dd_level;
	time_t	dd_ddate;
};
struct dumptime {
	struct	dumpdates dt_value;
	struct	dumptime *dt_next;
};
extern struct	dumptime *dthead;	/* head of the list version */
extern int	nddates;		/* number of records (might be zero) */
extern int	ddates_in;		/* we have read the increment file */
extern struct	dumpdates **ddatev;	/* the arrayfied version */
void	initdumptimes (int);
void	getdumptime (int);
void	putdumptime (void);
#define	ITITERATE(i, ddp) \
	for (ddp = ddatev[i = 0]; i < nddates; ddp = ddatev[++i])

void	sig (int signo);

/*
 * Compatibility with old systems.
 */
#ifdef COMPAT
#include <sys/file.h>
#define	strchr(a,b)	index(a,b)
#define	strrchr(a,b)	rindex(a,b)
extern char *strdup(), *ctime();
extern int read(), write();
extern int errno;
#endif

#ifdef	__linux__
#define	DUMP_CURRENT_REV	1

int dump_fs_open(const char *disk, ext2_filsys *fs);
#endif

#ifndef	__linux__
#ifndef	_PATH_UTMP
#define	_PATH_UTMP	"/etc/utmp"
#endif
#ifndef	_PATH_FSTAB
#define	_PATH_FSTAB	"/etc/fstab"
#endif
#endif

#ifdef sunos
extern char *calloc();
extern char *malloc();
extern long atol();
extern char *strcpy();
extern char *strncpy();
extern char *strcat();
extern time_t time();
extern void endgrent();
extern void exit();
extern off_t lseek();
extern const char *strerror();

#endif
