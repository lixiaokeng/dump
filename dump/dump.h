/*
 *	Ported to Linux's Second Extended File System as part of the
 *	dump and restore backup suit
 *	Remy Card <card@Linux.EU.Org>, 1994-1997
 *	Stelian Pop <stelian@popies.net>, 1999-2000
 *	Stelian Pop <stelian@popies.net> - Alcôve <www.alcove.com>, 2000-2002
 *
 *	$Id: dump.h,v 1.37 2002/04/04 08:20:23 stelian Exp $
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#define MAXINOPB	(MAXBSIZE / sizeof(struct dinode))
#define MAXNINDIR	(MAXBSIZE / sizeof(daddr_t))

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
extern char	lastlevel;	/* dump level of previous dump */
extern char	level;		/* dump level of this dump */
extern int	Afile;		/* archive file descriptor */
extern int	bzipflag;	/* compression is done using bzlib */
extern int	uflag;		/* update flag */
extern int	mflag;		/* dump metadata only if possible flag */
extern int	Mflag;		/* multi-volume flag */
extern int	qflag;		/* quit on errors flag */
extern int      breademax;      /* maximum number of bread errors before we quit */
extern char	*eot_script;	/* end of volume script fiag */
extern int	diskfd;		/* disk file descriptor */
extern int	tapefd;		/* tape file descriptor */
extern int	pipeout;	/* true => output to standard output */
extern int	fifoout;	/* true => output to fifo */
extern dump_ino_t curino;	/* current inumber; used globally */
extern int	newtape;	/* new tape flag */
extern int	density;	/* density in 0.1" units */
extern long	tapesize;	/* estimated tape size, blocks */
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

#ifdef USE_QFA
#define	QFA_MAGIC	"495115637697"
#define QFA_VERSION	"1.0"
extern int	gTapeposfd;
extern char	*gTapeposfile;
extern char	gTps[255];
extern int32_t	gThisDumpDate;
#endif /* USE_QFA */

#ifndef __P
#include <sys/cdefs.h>
#endif

/* operator interface functions */
void	broadcast __P((const char *message));
time_t	do_stats __P((void));
void	lastdump __P((char arg));
void	msg __P((const char *fmt, ...));
void	msgtail __P((const char *fmt, ...));
int	query __P((const char *question));
void	quit __P((const char *fmt, ...));
void	set_operators __P((void));
#if defined(SIGINFO)
void	statussig __P((int signo));
#endif
void	timeest __P((void));
time_t	unctime __P((const char *str));

/* mapping rouintes */
struct	dinode;
long	blockest __P((struct dinode const *dp));
int	mapfiles __P((dump_ino_t maxino, long *tapesize));
#ifdef	__linux__
int	mapfilesfromdir __P((dump_ino_t maxino, long *tapesize, char *directory));
int	maponefile __P((dump_ino_t maxino, long *tapesize, char *directory));
#endif
int	mapdirs __P((dump_ino_t maxino, long *tapesize));

/* file dumping routines */
void	blksout __P((daddr_t *blkp, int frags, dump_ino_t ino));
void	bread __P((daddr_t blkno, char *buf, int size));
void	dumpino __P((struct dinode *dp, dump_ino_t ino, int metaonly));
#ifdef	__linux__
void	dumpdirino __P((struct dinode *dp, dump_ino_t ino));
#endif
void	dumpmap __P((char *map, int type, dump_ino_t ino));
void	writeheader __P((dump_ino_t ino));
void	mkchecksum __P((union u_spcl *tmpspcl));

/* tape writing routines */
int	alloctape __P((void));
void	close_rewind __P((void));
void	dumpblock __P((daddr_t blkno, int size));
void	startnewtape __P((int top));
time_t	trewind __P((void));
void	writerec __P((const void *dp, int isspcl));
char	*mktimeest __P((time_t tnow));

void 	Exit __P((int status));
void	dumpabort __P((int signo));
void	getfstab __P((void));

const char *rawname __P((const char *cp));
struct	dinode *getino __P((dump_ino_t inum));

/* rdump routines */
#ifdef RDUMP
int	rmthost __P((const char *host));
int	rmtopen __P((const char *tape, int mode));
void	rmtclose __P((void));
int	rmtread __P((char *buf, size_t count));
int	rmtwrite __P((const char *buf, size_t count));
int	rmtseek __P((int offset, int pos));
struct mtget * rmtstatus __P((void));
int	rmtioctl __P((int cmd, int count));
#endif /* RDUMP */

void	interrupt __P((int signo));	/* in case operator bangs on console */
int	exclude_ino __P((dump_ino_t ino));
void	do_exclude_ino __P((dump_ino_t ino, const char *));

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

struct	fstab *fstabsearch __P((const char *key));	/* search fs_file and fs_spec */
#ifdef	__linux__
struct	fstab *fstabsearchdir __P((const char *key, char *dir));	/* search fs_file and fs_spec */
#endif

/*
 *	The contents of the file _PATH_DUMPDATES is maintained both on
 *	a linked list, and then (eventually) arrayified.
 */
struct dumpdates {
	char	dd_name[MAXPATHLEN+3];
	struct fstab *dd_fstab;
	char	dd_level;
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
void	initdumptimes __P((int));
void	getdumptime __P((int));
void	putdumptime __P((void));
#define	ITITERATE(i, ddp) \
	for (ddp = ddatev[i = 0]; i < nddates; ddp = ddatev[++i])

void	sig __P((int signo));

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

				/* 04-Feb-00 ILC */
#define IEXCLUDE_MAXNUM 256	/* max size of inode exclude list */

