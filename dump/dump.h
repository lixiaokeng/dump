/*
 *	Ported to Linux's Second Extended File System as part of the
 *	dump and restore backup suit
 *	Remy Card <card@Linux.EU.Org>, 1994-1997
 *	Stelian Pop <pop@noos.fr>, 1999-2000
 *	Stelian Pop <pop@noos.fr> - Alc�ve <www.alcove.fr>, 2000
 *
 *	$Id: dump.h,v 1.19 2000/12/21 11:14:53 stelian Exp $
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

#define MAXINOPB	(MAXBSIZE / sizeof(struct dinode))
#define MAXNINDIR	(MAXBSIZE / sizeof(daddr_t))

/*
 * Dump maps used to describe what is to be dumped.
 */
int	mapsize;	/* size of the state maps */
char	*usedinomap;	/* map of allocated inodes */
char	*dumpdirmap;	/* map of directories to be dumped */
char	*dumpinomap;	/* map of files to be dumped */
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
char	*disk;		/* name of the disk file */
char	tape[MAXPATHLEN];	/* name of the tape file */
char	*tapeprefix;	/* prefix of the tape file */
char	*dumpdates;	/* name of the file containing dump date information*/
char	lastlevel;	/* dump level of previous dump */
char	level;		/* dump level of this dump */
int	uflag;		/* update flag */
int	Mflag;		/* multi-volume flag */
char	*eot_script;	/* end of volume script fiag */
int	diskfd;		/* disk file descriptor */
int	tapefd;		/* tape file descriptor */
int	pipeout;	/* true => output to standard output */
ino_t	curino;		/* current inumber; used globally */
int	newtape;	/* new tape flag */
int	density;	/* density in 0.1" units */
long	tapesize;	/* estimated tape size, blocks */
long	tsize;		/* tape size in 0.1" units */
long	asize;		/* number of 0.1" units written on current tape */
int	etapes;		/* estimated number of tapes */
int	nonodump;	/* if set, do not honor UF_NODUMP user flags */
int	unlimited;	/* if set, write to end of medium */

int	notify;		/* notify operator flag */
int	blockswritten;	/* number of blocks written on current tape */
int	tapeno;		/* current tape number */
time_t	tstart_writing;	/* when started writing the first tape block */
time_t	tend_writing;	/* after writing the last tape block */
#ifdef __linux__
ext2_filsys fs;
#else
struct	fs *sblock;	/* the file system super block */
char	sblock_buf[MAXBSIZE];
#endif
long	xferrate;       /* averaged transfer rate of all volumes */
long	dev_bsize;	/* block size of underlying disk device */
int	dev_bshift;	/* log2(dev_bsize) */
int	tp_bshift;	/* log2(TP_BSIZE) */

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
int	mapfiles __P((ino_t maxino, long *tapesize));
#ifdef	__linux__
int	mapfilesfromdir __P((ino_t maxino, long *tapesize, char *directory));
#endif
int	mapdirs __P((ino_t maxino, long *tapesize));

/* file dumping routines */
void	blksout __P((daddr_t *blkp, int frags, ino_t ino));
void	bread __P((daddr_t blkno, char *buf, int size));
void	dumpino __P((struct dinode *dp, ino_t ino));
#ifdef	__linux__
void	dumpdirino __P((struct dinode *dp, ino_t ino));
#endif
void	dumpmap __P((char *map, int type, ino_t ino));
void	writeheader __P((ino_t ino));

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

char	*rawname __P((char *cp));
struct	dinode *getino __P((ino_t inum));

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
struct	dumptime *dthead;	/* head of the list version */
int	nddates;		/* number of records (might be zero) */
int	ddates_in;		/* we have read the increment file */
struct	dumpdates **ddatev;	/* the arrayfied version */
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

