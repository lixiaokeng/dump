/*
 *	Ported to Linux's Second Extended File System as part of the
 *	dump and restore backup suit
 *	Remy Card <card@Linux.EU.Org>, 1994-1997
 *	Stelian Pop <stelian@popies.net>, 1999-2000
 *	Stelian Pop <stelian@popies.net> - Alcôve <www.alcove.com>, 2000-2002
 */

/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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

#ifndef lint
static const char rcsid[] =
	"$Id: tape.c,v 1.67 2003/01/10 14:42:51 stelian Exp $";
#endif /* not lint */

#include <config.h>
#include <compatlfs.h>
#include <errno.h>
#include <compaterr.h>
#include <system.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/param.h>
#include <sys/file.h>
#include <sys/mtio.h>
#include <sys/stat.h>

#ifdef	__linux__
#include <sys/time.h>
#include <time.h>
#ifdef HAVE_EXT2FS_EXT2_FS_H
#include <ext2fs/ext2_fs.h>
#else
#include <linux/ext2_fs.h>
#endif
#include <ext2fs/ext2fs.h>
#include <bsdcompat.h>
#else	/* __linux__ */
#include <ufs/ufs/dinode.h>
#endif	/* __linux__ */
#include <protocols/dumprestore.h>

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif /* HAVE_ZLIB */

#ifdef HAVE_BZLIB
#include <bzlib.h>
#endif /* HAVE_BZLIB */

#include "restore.h"
#include "extern.h"
#include "pathnames.h"

#ifdef USE_QFA
int		noresyncmesg = 0;
#endif /* USE_QFA */
static long	fssize = MAXBSIZE;
static int	mt = -1;
int		pipein = 0;
static int	magtapein = 0;		/* input is from magtape */
static char	magtape[MAXPATHLEN];
static char	magtapeprefix[MAXPATHLEN];
static int	blkcnt;
static int	numtrec;
static char	*tapebuf;		/* input buffer for read */
static int	bufsize;		/* buffer size without prefix */
static char	*tbufptr = NULL;	/* active tape buffer */
#if defined(HAVE_ZLIB) || defined(HAVE_BZLIB)
static char	*comprbuf;		/* uncompress work buf */
static size_t	comprlen;		/* size including prefix */
#endif
static union	u_spcl endoftapemark;
static long	blksread;		/* blocks read since last header */
static long	tpblksread = 0;		/* TP_BSIZE blocks read */
static long	tapesread;
static sigjmp_buf	restart;
static int	gettingfile = 0;	/* restart has a valid frame */
char		*host = NULL;

static int	ofile;
static char	*map;
static char	lnkbuf[MAXPATHLEN + 1];
static int	pathlen;

int		oldinofmt;	/* old inode format conversion required */
int		Bcvt;		/* Swap Bytes (for CCI or sun) */
static int	Qcvt;		/* Swap quads (for sun) */

#define	FLUSHTAPEBUF()	blkcnt = ntrec + 1

static void	 accthdr __P((struct s_spcl *));
static int	 checksum __P((int *));
static void	 findinode __P((struct s_spcl *));
static void	 findtapeblksize __P((void));
static int	 gethead __P((struct s_spcl *));
static int	 converthead __P((struct s_spcl *));
static void	 converttapebuf __P((struct tapebuf *));
static void	 readtape __P((char *));
static void	 setdumpnum __P((void));
static u_int	 swabi __P((u_int));
#if 0
static u_long	 swabl __P((u_long));
#endif
static u_char	*swab64 __P((u_char *, int));
static u_char	*swab32 __P((u_char *, int));
static u_char	*swab16 __P((u_char *, int));
static void	 terminateinput __P((void));
static void	 xtrfile __P((char *, size_t));
static void	 xtrlnkfile __P((char *, size_t));
static void	 xtrlnkskip __P((char *, size_t));
static void	 xtrmap __P((char *, size_t));
static void	 xtrmapskip __P((char *, size_t));
static void	 xtrskip __P((char *, size_t));
static void	 setmagtapein __P((void));

#if defined(HAVE_ZLIB) || defined(HAVE_BZLIB)
static void	newcomprbuf __P((int));
static void	(*readtape_func) __P((char *));
static void	readtape_set __P((char *));
static void	readtape_uncompr __P((char *));
static void	readtape_comprfile __P((char *));
static void	readtape_comprtape __P((char *));
static char	*decompress_tapebuf __P((struct tapebuf *, int));
static void	msg_read_error __P((char *));
#endif
static int	read_a_block __P((int, char *, size_t, long *));
#define PREFIXSIZE	sizeof(struct tapebuf)

#define COMPARE_ONTHEFLY 1

#if COMPARE_ONTHEFLY
static int	ifile;		/* input file for compare */
static int	cmperror;	/* compare error */
static void	xtrcmpfile __P((char *, size_t));
static void	xtrcmpskip __P((char *, size_t));
#endif

static int readmapflag;
static int readingmaps;		/* set to 1 while reading the maps */

/*
 * Set up an input source. This is called from main.c before setup() is.
 */
void
setinput(char *source)
{
	FLUSHTAPEBUF();
	if (bflag)
		newtapebuf(ntrec);
	else
		newtapebuf(NTREC > HIGHDENSITYTREC ? NTREC : HIGHDENSITYTREC);
	terminal = stdin;

#ifdef RRESTORE
	if (strchr(source, ':')) {
		host = source;
		source = strchr(host, ':');
		*source++ = '\0';
		if (rmthost(host) == 0)
			exit(1);
	} else
#endif
	if (strcmp(source, "-") == 0) {
		/*
		 * Since input is coming from a pipe we must establish
		 * our own connection to the terminal.
		 */
		terminal = fopen(_PATH_TTY, "r");
		if (terminal == NULL) {
			warn("cannot open %s", _PATH_TTY);
			terminal = fopen(_PATH_DEVNULL, "r");
			if (terminal == NULL)
				err(1, "cannot open %s", _PATH_DEVNULL);
		}
		pipein++;
	}
	setuid(getuid());	/* no longer need or want root privileges */
	if (Mflag) {
		strncpy(magtapeprefix, source, MAXPATHLEN);
		magtapeprefix[MAXPATHLEN-1] = '\0';
		snprintf(magtape, MAXPATHLEN, "%s%03d", source, 1);
	}
	else
		strncpy(magtape, source, MAXPATHLEN);
	magtape[MAXPATHLEN - 1] = '\0';
}

void
newtapebuf(long size)
{
	static int tapebufsize = -1;

	ntrec = size;
	bufsize = ntrec * TP_BSIZE;
	if (size <= tapebufsize)
		return;
	if (tapebuf != NULL)
		free(tapebuf);
	tapebuf = malloc(size * TP_BSIZE + sizeof(struct tapebuf));
	if (tapebuf == NULL)
		errx(1, "Cannot allocate space for tape buffer");
	tapebufsize = size;
}

#if defined(HAVE_ZLIB) || defined(HAVE_BZLIB)
static void
newcomprbuf(int size)
{
	size_t buf_size = (size+1) * TP_BSIZE + sizeof(struct tapebuf);
	if (buf_size <= comprlen)
		return;
	comprlen = buf_size;
	if (comprbuf != NULL)
		free(comprbuf);
	comprbuf = malloc(comprlen);
	if (comprbuf == NULL)
		errx(1, "Cannot allocate space for decompress buffer");
}
#endif /* HAVE_ZLIB || HAVE_BZLIB */

/*
 * Verify that the tape drive can be accessed and
 * that it actually is a dump tape.
 */
void
setup(void)
{
	int i, j, *ip, bot_code;
	struct STAT stbuf;
	char *temptape;

	Vprintf(stdout, "Verify tape and initialize maps\n");
	if (Afile == NULL && bot_script) {
		msg("Launching %s\n", bot_script);
		bot_code = system_command(bot_script, magtape, 1);
		if (bot_code != 0 && bot_code != 1) {
			msg("Restore aborted by the beginning of tape script\n");
			exit(1);
		}
	}

	if (Afile)
		temptape = Afile;
	else
		temptape = magtape;

#ifdef RRESTORE
	if (host)
		mt = rmtopen(temptape, O_RDONLY);
	else
#endif
	if (pipein)
		mt = 0;
	else
		mt = OPEN(temptape, O_RDONLY, 0);
	if (mt < 0)
		err(1, "%s", magtape);
	if (!Afile) {
		volno = 1;
		setmagtapein();
		setdumpnum();
	}
#if defined(HAVE_ZLIB) || defined(HAVE_BZLIB)
	readtape_func = readtape_set;
#endif
	FLUSHTAPEBUF();
	findtapeblksize();
	if (gethead(&spcl) == FAIL) {
		blkcnt--; /* push back this block */
		blksread--;
		tpblksread--;
		cvtflag++;
		if (gethead(&spcl) == FAIL)
			errx(1, "Tape is not a dump tape");
		fprintf(stderr, "Converting to new file system format.\n");
	}

	if (zflag) {
		fprintf(stderr, "Dump tape is compressed.\n");
#if !defined(HAVE_ZLIB) && !defined(HAVE_BZLIB)
		errx(1,"This restore version doesn't support decompression");
#endif /* !HAVE_ZLIB && !HAVE_BZLIB */
	}
	if (pipein) {
		endoftapemark.s_spcl.c_magic = cvtflag ? OFS_MAGIC : NFS_MAGIC;
		endoftapemark.s_spcl.c_type = TS_END;
		ip = (int *)&endoftapemark;
		j = sizeof(union u_spcl) / sizeof(int);
		i = 0;
		do
			i += *ip++;
		while (--j);
		endoftapemark.s_spcl.c_checksum = CHECKSUM - i;
	}
	if (vflag || command == 't' || command == 'C')
		printdumpinfo();
#ifdef USE_QFA
	if (tapeposflag && (unsigned long)spcl.c_date != qfadumpdate)
		errx(1, "different QFA/dumpdates detected\n");
#endif
	if (filesys[0] == '\0') {
		char *dirptr;
		strncpy(filesys, spcl.c_filesys, NAMELEN);
		filesys[NAMELEN - 1] = '\0';
		dirptr = strstr(filesys, " (dir");
		if (dirptr != NULL)
			*dirptr = '\0';
	}
	dumptime = spcl.c_ddate;
	dumpdate = spcl.c_date;
	if (STAT(".", &stbuf) < 0)
		err(1, "cannot stat .");
	if (stbuf.st_blksize > 0 && stbuf.st_blksize < TP_BSIZE )
		fssize = TP_BSIZE;
	if (stbuf.st_blksize >= TP_BSIZE && stbuf.st_blksize <= MAXBSIZE)
		fssize = stbuf.st_blksize;
	if (((fssize - 1) & fssize) != 0)
		errx(1, "bad block size %ld", fssize);
	if (spcl.c_volume != 1)
		errx(1, "Tape is not volume 1 of the dump");
	if (gethead(&spcl) == FAIL) {
		Dprintf(stdout, "header read failed at %ld blocks\n", (long)blksread);
		panic("no header after volume mark!\n");
	}
	readingmaps = 1;
	findinode(&spcl);
	if (spcl.c_type != TS_CLRI)
		errx(1, "Cannot find file removal list");
	maxino = (spcl.c_count * TP_BSIZE * NBBY) + 1;
	map = calloc((unsigned)1, (unsigned)howmany(maxino, NBBY));
	if (map == NULL)
		errx(1, "no memory for active inode map");
	usedinomap = map;
	curfile.action = USING;
	getfile(xtrmap, xtrmapskip);
	while (spcl.c_type == TS_ADDR) {
		/* Recompute maxino and the map */
		dump_ino_t oldmaxino = maxino;
		maxino += (spcl.c_count * TP_BSIZE * NBBY) + 1;
		resizemaps(oldmaxino, maxino);

		spcl.c_dinode.di_size = spcl.c_count * TP_BSIZE;
		getfile(xtrmap, xtrmapskip);
	}
	Dprintf(stdout, "maxino = %lu\n", (unsigned long)maxino);
	if (spcl.c_type != TS_BITS)
		errx(1, "Cannot find file dump list");
	map = calloc((unsigned)1, (unsigned)howmany(maxino, NBBY));
	if (map == (char *)NULL)
		errx(1, "no memory for file dump list");
	dumpmap = map;
	curfile.action = USING;
	getfile(xtrmap, xtrmapskip);
	while (spcl.c_type == TS_ADDR) {
		spcl.c_dinode.di_size = spcl.c_count * TP_BSIZE;
		getfile(xtrmap, xtrmapskip);
	}
	/*
	 * If there may be whiteout entries on the tape, pretend that the
	 * whiteout inode exists, so that the whiteout entries can be
	 * extracted.
	 */
	if (oldinofmt == 0)
		SETINO(WINO, dumpmap);
	readingmaps = 0;
	findinode(&spcl);
}

/*
 * Prompt user to load a new dump volume.
 * "Nextvol" is the next suggested volume to use.
 * This suggested volume is enforced when doing full
 * or incremental restores, but can be overridden by
 * the user when only extracting a subset of the files.
 */
void
getvol(long nextvol)
{
	long newvol = 0, wantnext = 0, i;
	long saved_blksread = 0, saved_tpblksread = 0;
	union u_spcl tmpspcl;
#	define tmpbuf tmpspcl.s_spcl
	char buf[TP_BSIZE];
	int haderror = 0, bot_code = 1;

	if (nextvol == 1) {
		tapesread = 0;
		gettingfile = 0;
		tpblksread = 0;
		blksread = 0;
	}
	if (pipein) {
		if (nextvol != 1)
			panic("Changing volumes on pipe input?\n");
		if (volno == 1)
			return;
		goto gethdr;
	}
	saved_blksread = blksread;
	saved_tpblksread = tpblksread;
#if defined(USE_QFA) && defined(sunos)
	if (createtapeposflag || tapeposflag) 
		close(fdsmtc);
#endif
again:
	if (pipein)
		exit(1); /* pipes do not get a second chance */
	if (aflag || curfile.action != SKIP) {
		newvol = nextvol;
		wantnext = 1;
	} else {
		newvol = 0;
		wantnext = 0;
	}
	while (newvol <= 0) {
		if (tapesread == 0) {
			fprintf(stderr, "%s%s%s%s%s",
			    "You have not read any volumes yet.\n",
			    "Unless you know which volume your",
			    " file(s) are on you should start\n",
			    "with the last volume and work",
			    " towards the first.\n");
		} else {
			fprintf(stderr, "You have read volumes");
			strcpy(buf, ": ");
			for (i = 1; i < 32; i++)
				if (tapesread & (1 << i)) {
					fprintf(stderr, "%s%ld", buf, (long)i);
					strcpy(buf, ", ");
				}
			fprintf(stderr, "\n");
		}
		do	{
			fprintf(stderr, "Specify next volume # (none if no more volumes): ");
			(void) fflush(stderr);
			(void) fgets(buf, TP_BSIZE, terminal);
		} while (!feof(terminal) && buf[0] == '\n');
		if (feof(terminal))
			exit(1);
		if (!strcmp(buf, "none\n")) {
			terminateinput();
			return;
		}
		newvol = atoi(buf);
		if (newvol <= 0) {
			fprintf(stderr,
			    "Volume numbers are positive numerics\n");
		}
	}
	if (newvol == volno) {
		tapesread |= 1 << volno;
#if defined(USE_QFA) && defined(sunos)
		if (createtapeposflag || tapeposflag) {
			if (OpenSMTCmt(magtape) < 0) {
				volno = -1;
				haderror = 1;
				goto again;
			}
		}
#endif
		return;
	}
	closemt();
	if (Mflag) {
		snprintf(magtape, MAXPATHLEN, "%s%03ld", magtapeprefix, newvol);
		magtape[MAXPATHLEN - 1] = '\0';
	}
	if (bot_script && !haderror) {
		msg("Launching %s\n", bot_script);
		bot_code = system_command(bot_script, magtape, newvol);
		if (bot_code != 0 && bot_code != 1) {
			msg("Restore aborted by the beginning of tape script\n");
			exit(1);
		}
	}
	if (haderror || (bot_code && !Mflag)) {
		haderror = 0;
		fprintf(stderr, "Mount volume %ld\n", (long)newvol);
		fprintf(stderr, "Enter ``none'' if there are no more volumes\n");
		fprintf(stderr, "otherwise enter volume name (default: %s) ", magtape);
		(void) fflush(stderr);
		(void) fgets(buf, TP_BSIZE, terminal);
		if (feof(terminal))
			exit(1);
		if (!strcmp(buf, "none\n")) {
			terminateinput();
			return;
		}
		if (buf[0] != '\n') {
			char *pos;
			(void) strncpy(magtape, buf, sizeof(magtape));
			magtape[sizeof(magtape) - 1] = '\0';
			if ((pos = strchr(magtape, '\n'))) 
				magtape[pos - magtape] = '\0';
		}
	}
#ifdef RRESTORE
	if (host)
		mt = rmtopen(magtape, O_RDONLY);
	else
#endif
		mt = OPEN(magtape, O_RDONLY, 0);

	if (mt == -1) {
		fprintf(stderr, "Cannot open %s\n", magtape);
		volno = -1;
		haderror = 1;
		goto again;
	}
gethdr:
	setmagtapein();
#if defined(HAVE_ZLIB) || defined(HAVE_BZLIB)
	readtape_func = readtape_set;
#endif
	volno = newvol;
	setdumpnum();
	FLUSHTAPEBUF();
	findtapeblksize();
	if (gethead(&tmpbuf) == FAIL) {
		Dprintf(stdout, "header read failed at %ld blocks\n", (long)blksread);
		fprintf(stderr, "tape is not dump tape\n");
		volno = 0;
		haderror = 1;
		blksread = saved_blksread;
		tpblksread = saved_tpblksread;
		goto again;
	}
	if (tmpbuf.c_volume != volno) {
		fprintf(stderr, "Wrong volume (%d)\n", tmpbuf.c_volume);
		volno = 0;
		haderror = 1;
		blksread = saved_blksread;
		tpblksread = saved_tpblksread;
		goto again;
	}
	if (tmpbuf.c_date != dumpdate || tmpbuf.c_ddate != dumptime) {
		fprintf(stderr, "Wrong dump date\n\tgot: %s",
			ctime4(&tmpbuf.c_date));
		fprintf(stderr, "\twanted: %s", ctime(&dumpdate));
		volno = 0;
		haderror = 1;
		blksread = saved_blksread;
		tpblksread = saved_tpblksread;
		goto again;
	}
	tapesread |= 1 << volno;
 	/*
 	 * If continuing from the previous volume, skip over any
 	 * blocks read already at the end of the previous volume.
 	 *
 	 * If coming to this volume at random, skip to the beginning
 	 * of the next record.
 	 */
	if (zflag) {
		fprintf(stderr, "Dump tape is compressed.\n");
#if !defined(HAVE_ZLIB) && !defined(HAVE_BZLIB)
		errx(1,"This restore version doesn't support decompression");
#endif /* !HAVE_ZLIB && !HAVE_BZLIB */
	}
	Dprintf(stdout, "read %ld recs, tape starts with %ld\n",
		tpblksread - 1, (long)tmpbuf.c_firstrec);
 	if (tmpbuf.c_type == TS_TAPE && (tmpbuf.c_flags & DR_NEWHEADER)) {
 		if (!wantnext) {
 			tpblksread = tmpbuf.c_firstrec + 1;
 			for (i = tmpbuf.c_count; i > 0; i--)
 				readtape(buf);
 		} else if (tmpbuf.c_firstrec > 0 &&
			   tmpbuf.c_firstrec < tpblksread - 1) {
			/*
			 * -1 since we've read the volume header
			 */
 			i = tpblksread - tmpbuf.c_firstrec - 1;
			Dprintf(stderr, "Skipping %ld duplicate record%s.\n",
				(long)i, i > 1 ? "s" : "");
 			while (--i >= 0)
 				readtape(buf);
 		}
 	}
	if (curfile.action == USING) {
		if (volno == 1)
			panic("active file into volume 1\n");
		return;
	}
	/*
	 * Skip up to the beginning of the next record
	 */
	if (tmpbuf.c_type == TS_TAPE && (tmpbuf.c_flags & DR_NEWHEADER))
		for (i = tmpbuf.c_count; i > 0; i--)
			readtape(buf);
	(void) gethead(&spcl);
	findinode(&spcl);
	if (gettingfile) {
		gettingfile = 0;
		siglongjmp(restart, 1);
	}
}

/*
 * Handle unexpected EOF.
 */
static void
terminateinput(void)
{

	if (gettingfile && curfile.action == USING) {
		printf("Warning: %s %s\n",
		    "End-of-input encountered while extracting", curfile.name);
	}
	curfile.name = "<name unknown>";
	curfile.action = UNKNOWN;
	curfile.dip = NULL;
	curfile.ino = maxino;
	if (gettingfile) {
		gettingfile = 0;
		siglongjmp(restart, 1);
	}
}

/*
 * handle multiple dumps per tape by skipping forward to the
 * appropriate one.
 */
static void
setdumpnum(void)
{
	struct mtop tcom;

	if (dumpnum == 1 || volno != 1)
		return;
	if (pipein)
		errx(1, "Cannot have multiple dumps on pipe input");
	tcom.mt_op = MTFSF;
	tcom.mt_count = dumpnum - 1;
#ifdef RRESTORE
	if (host)
		rmtioctl(MTFSF, dumpnum - 1);
	else
#endif
		if (ioctl(mt, (int)MTIOCTOP, (char *)&tcom) < 0)
			warn("ioctl MTFSF");
}

void
printdumpinfo(void)
{
	fprintf(stdout, "Dump   date: %s", ctime4(&spcl.c_date));
	fprintf(stdout, "Dumped from: %s",
	    (spcl.c_ddate == 0) ? "the epoch\n" : ctime4(&spcl.c_ddate));
	if (spcl.c_host[0] == '\0')
		return;
	fprintf(stdout, "Level %d dump of %s on %s:%s\n",
		spcl.c_level, spcl.c_filesys, spcl.c_host, spcl.c_dev);
	fprintf(stdout, "Label: %s\n", spcl.c_label);
}

void 
printvolinfo(void)
{
	int i;

	if (volinfo[1] == ROOTINO) {
		printf("Starting inode numbers by volume:\n");
		for (i = 1; i < (int)TP_NINOS && volinfo[i] != 0; ++i)
			printf("\tVolume %d: %lu\n", i, (unsigned long)volinfo[i]);
	}
}

#ifdef sunos
struct timeval
	time_t		tv_sec;		/* seconds */
	suseconds_t	tv_usec;	/* and microseconds */
};
#endif

int
extractfile(struct entry *ep, int doremove)
{
	unsigned int flags;
	mode_t mode;
	struct timeval timep[2];
	char *name = myname(ep);

	/* If removal is requested (-r mode) do remove it unless
	 * we are extracting a metadata only inode */
	if (spcl.c_flags & DR_METAONLY) {
		Vprintf(stdout, "file %s is metadata only\n", name);
	}
	else {
		if (doremove) {
			removeleaf(ep);
			ep->e_flags &= ~REMOVED;
		}
	}

	curfile.name = name;
	curfile.action = USING;
#if defined(__linux__) || defined(sunos)
	timep[0].tv_sec = curfile.dip->di_atime.tv_sec;
	timep[0].tv_usec = curfile.dip->di_atime.tv_usec;
	timep[1].tv_sec = curfile.dip->di_mtime.tv_sec;
	timep[1].tv_usec = curfile.dip->di_mtime.tv_usec;
#else	/* __linux__ || sunos */
	timep[0].tv_sec = curfile.dip->di_atime;
	timep[0].tv_usec = curfile.dip->di_atimensec / 1000;
	timep[1].tv_sec = curfile.dip->di_mtime;
	timep[1].tv_usec = curfile.dip->di_mtimensec / 1000;
#endif	/* __linux__ */
	mode = curfile.dip->di_mode;
	flags = curfile.dip->di_flags;
	switch (mode & IFMT) {

	default:
		fprintf(stderr, "%s: unknown file mode 0%o\n", name, mode);
		skipfile();
		return (FAIL);

	case IFSOCK:
		Vprintf(stdout, "skipped socket %s\n", name);
		skipfile();
		return (GOOD);

	case IFDIR:
		if (mflag) {
			if (ep == NULL || ep->e_flags & EXTRACT)
				panic("unextracted directory %s\n", name);
			skipfile();
			return (GOOD);
		}
		Vprintf(stdout, "extract file %s\n", name);
		return (genliteraldir(name, curfile.ino));

	case IFLNK:
	{
#ifdef HAVE_LCHOWN
		uid_t luid = curfile.dip->di_uid;
		gid_t lgid = curfile.dip->di_gid;
#endif
		if (! (spcl.c_flags & DR_METAONLY)) {
			lnkbuf[0] = '\0';
			pathlen = 0;
			getfile(xtrlnkfile, xtrlnkskip);
			if (pathlen == 0) {
				Vprintf(stdout,
				    "%s: zero length symbolic link (ignored)\n", name);
				return (GOOD);
			}
			if (linkit(lnkbuf, name, SYMLINK) == FAIL)
				return (FAIL);
		}
		else
			skipfile();

#ifdef HAVE_LCHOWN
		(void) lchown(name, luid, lgid);
#endif
		return (GOOD);
	}

	case IFIFO:
		Vprintf(stdout, "extract fifo %s\n", name);
		if (Nflag) {
			skipfile();
			return (GOOD);
		}
		if (! (spcl.c_flags & DR_METAONLY)) {
			if (uflag && !Nflag)
				(void)unlink(name);
			if (mkfifo(name, mode) < 0) {
				warn("%s: cannot create fifo", name);
				skipfile();
				return (FAIL);
			}
		}
		(void) chown(name, curfile.dip->di_uid, curfile.dip->di_gid);
		(void) chmod(name, mode);
		if (flags)
#ifdef  __linux__
			(void) fsetflags(name, flags);
#else
			(void) chflags(name, flags);
#endif
		skipfile();
		utimes(name, timep);
		return (GOOD);

	case IFCHR:
	case IFBLK:
		Vprintf(stdout, "extract special file %s\n", name);
		if (Nflag) {
			skipfile();
			return (GOOD);
		}
		if (! (spcl.c_flags & DR_METAONLY)) {
			if (uflag)
				(void)unlink(name);
			if (mknod(name, mode, (int)curfile.dip->di_rdev) < 0) {
				warn("%s: cannot create special file", name);
				skipfile();
				return (FAIL);
			}
		}
		(void) chown(name, curfile.dip->di_uid, curfile.dip->di_gid);
		(void) chmod(name, mode);
		if (flags)
#ifdef	__linux__
			{
			warn("%s: fsetflags called on a special file", name);
			(void) fsetflags(name, flags);
			}
#else
			(void) chflags(name, flags);
#endif
		skipfile();
		utimes(name, timep);
		return (GOOD);

	case IFREG:
	{
		uid_t luid = curfile.dip->di_uid;
		gid_t lgid = curfile.dip->di_gid;

		Vprintf(stdout, "extract file %s\n", name);
		if (Nflag) {
			skipfile();
			return (GOOD);
		}
		if (! (spcl.c_flags & DR_METAONLY)) {
			if (uflag)
				(void)unlink(name);
			if ((ofile = OPEN(name, O_WRONLY | O_CREAT | O_TRUNC,
			    0666)) < 0) {
				warn("%s: cannot create file", name);
				skipfile();
				return (FAIL);
			}
			getfile(xtrfile, xtrskip);
			(void) close(ofile);
		}
		else
			skipfile();
		(void) chown(name, luid, lgid);
		(void) chmod(name, mode);
		if (flags)
#ifdef	__linux__
			(void) fsetflags(name, flags);
#else
			(void) chflags(name, flags);
#endif
		utimes(name, timep);
		return (GOOD);
	}
	}
	/* NOTREACHED */
}

/*
 * skip over bit maps on the tape
 */
void
skipmaps(void)
{

	while (spcl.c_type == TS_BITS || spcl.c_type == TS_CLRI)
		skipfile();
}

/*
 * skip over a file on the tape
 */
void
skipfile(void)
{

	curfile.action = SKIP;
	getfile(xtrnull, xtrnull);
}

/*
 * Extract a file from the tape.
 * When an allocated block is found it is passed to the fill function;
 * when an unallocated block (hole) is found, a zeroed buffer is passed
 * to the skip function.
 */
void
getfile(void (*fill) __P((char *, size_t)), void (*skip) __P((char *, size_t)))
{
	int i;
	volatile int curblk = 0;
	volatile quad_t size = spcl.c_dinode.di_size;
	volatile int last_write_was_hole = 0;
	quad_t origsize = size;
	static char clearedbuf[MAXBSIZE];
	char buf[MAXBSIZE / TP_BSIZE][TP_BSIZE];
	char junk[TP_BSIZE];

	if (spcl.c_type == TS_END)
		panic("ran off end of tape\n");
	if (spcl.c_magic != NFS_MAGIC)
		panic("not at beginning of a file\n");
	if (!gettingfile && setjmp(restart) != 0)
		return;
	gettingfile++;
loop:
	for (i = 0; i < spcl.c_count; i++) {
		if (readmapflag || spcl.c_addr[i]) {
			readtape(&buf[curblk++][0]);
			if (curblk == fssize / TP_BSIZE) {
				(*fill)((char *)buf, (size_t)(size > TP_BSIZE ?
				     fssize : (curblk - 1) * TP_BSIZE + size));
				curblk = 0;
				last_write_was_hole = 0;
			}
		} else {
			if (curblk > 0) {
				(*fill)((char *)buf, (size_t)(size > TP_BSIZE ?
				     curblk * TP_BSIZE :
				     (curblk - 1) * TP_BSIZE + size));
				curblk = 0;
			}
			(*skip)(clearedbuf, (long)(size > TP_BSIZE ?
				TP_BSIZE : size));
			last_write_was_hole = 1;
		}
		if ((size -= TP_BSIZE) <= 0) {
			for (i++; i < spcl.c_count; i++)
				if (readmapflag || spcl.c_addr[i])
					readtape(junk);
			break;
		}
	}
	if (gethead(&spcl) == GOOD && size > 0) {
		if (spcl.c_type == TS_ADDR)
			goto loop;
		Dprintf(stdout,
			"Missing address (header) block for %s at %ld blocks\n",
			curfile.name, (long)blksread);
	}
	if (curblk > 0) {
		(*fill)((char *)buf, (size_t)((curblk * TP_BSIZE) + size));
		last_write_was_hole = 0;
	}
	if (size > 0) {
		fprintf(stderr, "Missing blocks at the end of %s, assuming hole\n", curfile.name);
		while (size > 0) {
			size_t skp = size > TP_BSIZE ? TP_BSIZE : size;
			(*skip)(clearedbuf, skp);
			size -= skp;
		}
		last_write_was_hole = 1;
	}
	if (last_write_was_hole) {
		FTRUNCATE(ofile, origsize);
	}
	if (!readingmaps) 
		findinode(&spcl);
	gettingfile = 0;
}

/*
 * Write out the next block of a file.
 */
static void
xtrfile(char *buf, size_t size)
{

	if (Nflag)
		return;
	if (write(ofile, buf, (int) size) == -1)
		err(1, "write error extracting inode %lu, name %s\nwrite",
			(unsigned long)curfile.ino, curfile.name);
}

/*
 * Skip over a hole in a file.
 */
/* ARGSUSED */
static void
xtrskip(UNUSED(char *buf), size_t size)
{

	if (LSEEK(ofile, (OFF_T)size, SEEK_CUR) == -1)
		err(1, "seek error extracting inode %lu, name %s\nlseek",
			(unsigned long)curfile.ino, curfile.name);
}

/*
 * Collect the next block of a symbolic link.
 */
static void
xtrlnkfile(char *buf, size_t size)
{

	pathlen += size;
	if (pathlen > MAXPATHLEN)
		errx(1, "symbolic link name: %s->%s%s; too long %d",
		    curfile.name, lnkbuf, buf, pathlen);
	(void) strcat(lnkbuf, buf);
}

/*
 * Skip over a hole in a symbolic link (should never happen).
 */
/* ARGSUSED */
static void
xtrlnkskip(UNUSED(char *buf), UNUSED(size_t size))
{

	errx(1, "unallocated block in symbolic link %s", curfile.name);
}

/*
 * Collect the next block of a bit map.
 */
static void
xtrmap(char *buf, size_t size)
{

	memmove(map, buf, size);
	map += size;
}

/*
 * Skip over a hole in a bit map (should never happen).
 */
/* ARGSUSED */
static void
xtrmapskip(UNUSED(char *buf), size_t size)
{

	panic("hole in map\n");
	map += size;
}

/*
 * Noop, when an extraction function is not needed.
 */
/* ARGSUSED */
void
xtrnull(UNUSED(char *buf), UNUSED(size_t size))
{

	return;
}

#if COMPARE_ONTHEFLY
/*
 * Compare the next block of a file.
 */
static void
xtrcmpfile(char *buf, size_t size)
{
	static char cmpbuf[MAXBSIZE];

	if (cmperror)
		return;
	
	if (read(ifile, cmpbuf, size) != (ssize_t)size) {
		fprintf(stderr, "%s: size has changed.\n", 
			curfile.name);
		cmperror = 1;
		return;
	}
	
	if (memcmp(buf, cmpbuf, size) != 0) {
		fprintf(stderr, "%s: tape and disk copies are different\n",
			curfile.name);
		cmperror = 1;
		return;
	}
}

/*
 * Skip over a hole in a file.
 */
static void
xtrcmpskip(UNUSED(char *buf), size_t size)
{
	static char cmpbuf[MAXBSIZE];
	int i;

	if (cmperror)
		return;
	
	if (read(ifile, cmpbuf, size) != (ssize_t)size) {
		fprintf(stderr, "%s: size has changed.\n", 
			curfile.name);
		cmperror = 1;
		return;
	}

	for (i = 0; i < (int)size; ++i)
		if (cmpbuf[i] != '\0') {
			fprintf(stderr, "%s: tape and disk copies are different\n",
				curfile.name);
			cmperror = 1;
			return;
		}
}
#endif /* COMPARE_ONTHEFLY */

#if !COMPARE_ONTHEFLY
static int
do_cmpfiles(int fd_tape, int fd_disk, long size)
{
	static char buf_tape[BUFSIZ];
	static char buf_disk[BUFSIZ];
	ssize_t n_tape;
	ssize_t n_disk;

	while (size > 0) {
		if ((n_tape = read(fd_tape, buf_tape, sizeof(buf_tape))) < 1) {
			close(fd_tape), close(fd_disk);
			panic("do_cmpfiles: unexpected EOF[1]");
		}
		if ((n_disk = read(fd_disk, buf_disk, sizeof(buf_tape))) < 1) {
			close(fd_tape), close(fd_disk);
			panic("do_cmpfiles: unexpected EOF[2]");
		}
		if (n_tape != n_disk) {
			close(fd_tape), close(fd_disk);
			panic("do_cmpfiles: sizes different!");
		}
		if (memcmp(buf_tape, buf_disk, (size_t)n_tape) != 0) return (1);
		size -= n_tape;
	}
	return (0);
}

/* for debugging compare problems */
#undef COMPARE_FAIL_KEEP_FILE

static
#ifdef COMPARE_FAIL_KEEP_FILE
/* return true if tapefile should be unlinked after compare */
int
#else
void
#endif
cmpfiles(char *tapefile, char *diskfile, struct STAT *sbuf_disk)
{
	struct STAT sbuf_tape;
	int fd_tape, fd_disk;

	if (STAT(tapefile, &sbuf_tape) != 0) {
		panic("Can't lstat tmp file %s: %s\n", tapefile,
		      strerror(errno));
		do_compare_error;
	}

	if (sbuf_disk->st_size != sbuf_tape.st_size) {
		fprintf(stderr,
			"%s: size changed from %ld to %ld.\n",
			diskfile, (long)sbuf_tape.st_size, (long)sbuf_disk->st_size);
		do_compare_error;
#ifdef COMPARE_FAIL_KEEP_FILE
		return (0);
#else
		return;
#endif
	}

	if ((fd_tape = OPEN(tapefile, O_RDONLY)) < 0) {
		panic("Can't open %s: %s\n", tapefile, strerror(errno));
		do_compare_error;
	}
	if ((fd_disk = OPEN(diskfile, O_RDONLY)) < 0) {
		close(fd_tape);
		panic("Can't open %s: %s\n", diskfile, strerror(errno));
		do_compare_error;
	}

	if (do_cmpfiles(fd_tape, fd_disk, sbuf_tape.st_size)) {
		fprintf(stderr, "%s: tape and disk copies are different\n",
			diskfile);
		close(fd_tape);
		close(fd_disk);
		do_compare_error;
#ifdef COMPARE_FAIL_KEEP_FILE
		/* rename the file to live in /tmp */
		/* rename `tapefile' to /tmp/<basename of diskfile> */
		{
			char *p = strrchr(diskfile, '/');
			char newname[MAXPATHLEN];
			if (!p) {
				panic("can't find / in %s\n", diskfile);
			}
			snprintf(newname, sizeof(newname), "%s/debug/%s", tmpdir, p + 1);
			if (rename(tapefile, newname)) {
				panic("rename from %s to %s failed: %s\n",
				      tapefile, newname,
				      strerror(errno));
			} else {
				fprintf(stderr, "*** %s saved to %s\n",
					tapefile, newname);
			}
		}
		
		/* don't unlink the file (it's not there anymore */
		/* anyway) */
		return (0);
#else
		return;
#endif
	}
	close(fd_tape);
	close(fd_disk);
#ifdef COMPARE_FAIL_KEEP_FILE
	return (1);
#endif
}
#endif /* !COMPARE_ONTHEFLY */

#if !COMPARE_ONTHEFLY
static char tmpfilename[MAXPATHLEN];
#endif

void
comparefile(char *name)
{
	unsigned int mode;
	struct STAT sb;
	int r;
#if !COMPARE_ONTHEFLY
	static char *tmpfile = NULL;
	struct STAT stemp;
#endif

	if ((r = LSTAT(name, &sb)) != 0) {
		warn("%s: does not exist (%d)", name, r);
		do_compare_error;
		skipfile();
		return;
	}

	curfile.name = name;
	curfile.action = USING;
	mode = curfile.dip->di_mode;

	Vprintf(stdout, "comparing %s (size: %ld, mode: 0%o)\n", name,
		(long)sb.st_size, mode);

	if (sb.st_mode != mode) {
		fprintf(stderr, "%s: mode changed from 0%o to 0%o.\n",
			name, mode & 07777, sb.st_mode & 07777);
		do_compare_error;
	}
	if (spcl.c_flags & DR_METAONLY) {
		skipfile();
		return;
	}
	switch (mode & IFMT) {
	default:
		skipfile();
		return;

	case IFSOCK:
		skipfile();
		return;

	case IFDIR:
		skipfile();
		return;

	case IFLNK: {
		char lbuf[MAXPATHLEN + 1];
		int lsize;

		if (!(sb.st_mode & S_IFLNK)) {
			fprintf(stderr, "%s: is no longer a symbolic link\n",
				name);
			do_compare_error;
			return;
		}
		lnkbuf[0] = '\0';
		pathlen = 0;
		getfile(xtrlnkfile, xtrlnkskip);
		if (pathlen == 0) {
			fprintf(stderr,
				"%s: zero length symbolic link (ignored)\n",
				name);
			do_compare_error;
			return;
		}
		if ((lsize = readlink(name, lbuf, MAXPATHLEN)) < 0) {
			panic("readlink of %s failed: %s", name,
			      strerror(errno));
			do_compare_error;
		}
		lbuf[lsize] = 0;
		if (strcmp(lbuf, lnkbuf) != 0) {
			fprintf(stderr,
				"%s: symbolic link changed from %s to %s.\n",
				name, lnkbuf, lbuf);
			do_compare_error;
			return;
		}
		return;
	}

	case IFCHR:
	case IFBLK:
		if (!(sb.st_mode & (S_IFCHR|S_IFBLK))) {
			fprintf(stderr, "%s: no longer a special file\n",
				name);
			do_compare_error;
			skipfile();
			return;
		}

		if (sb.st_rdev != (dev_t)curfile.dip->di_rdev) {
			fprintf(stderr,
				"%s: device changed from %d,%d to %d,%d.\n",
				name,
				((int)curfile.dip->di_rdev >> 8) & 0xff,
				(int)curfile.dip->di_rdev & 0xff,
				((int)sb.st_rdev >> 8) & 0xff,
				(int)sb.st_rdev & 0xff);
			do_compare_error;
		}
		skipfile();
		return;

	case IFREG:
#if COMPARE_ONTHEFLY
		if ((ifile = OPEN(name, O_RDONLY)) < 0) {
			panic("Can't open %s: %s\n", name, strerror(errno));
			skipfile();
			do_compare_error;
		}
		else {
			cmperror = 0;
			getfile(xtrcmpfile, xtrcmpskip);
			if (!cmperror) {
				char c;
				if (read(ifile, &c, 1) != 0) {
					fprintf(stderr, "%s: size has changed.\n", 
						name);
					cmperror = 1;
				}
			}
			if (cmperror)
				do_compare_error;
			close(ifile);
		}
#else
		if (tmpfile == NULL) {
			/* argument to mktemp() must not be in RO space: */
			snprintf(tmpfilename, sizeof(tmpfilename), "%s/restoreCXXXXXX", tmpdir);
			tmpfile = mktemp(&tmpfilename[0]);
		}
		if ((STAT(tmpfile, &stemp) == 0) && (unlink(tmpfile) != 0)) {
			panic("cannot delete tmp file %s: %s\n",
			      tmpfile, strerror(errno));
		}
		if ((ofile = OPEN(tmpfile, O_WRONLY | O_CREAT | O_TRUNC, 0600)) < 0) {
			panic("cannot create file temp file %s: %s\n",
			      name, strerror(errno));
		}
		getfile(xtrfile, xtrskip);
		(void) close(ofile);
#ifdef COMPARE_FAIL_KEEP_FILE
		if (cmpfiles(tmpfile, name, &sb))
			unlink(tmpfile);
#else
		cmpfiles(tmpfile, name, &sb);
		unlink(tmpfile);
#endif
#endif /* COMPARE_ONTHEFLY */
		return;
	}
	/* NOTREACHED */
}

#if defined(HAVE_ZLIB) || defined(HAVE_BZLIB)
static void (*readtape_func)(char *) = readtape_set;

/*
 * Read TP_BSIZE blocks from the input.
 * Handle read errors, and end of media.
 * Decompress compressed blocks.
 */
static void
readtape(char *buf)
{
	(*readtape_func)(buf);	/* call the actual processing routine */
}

/*
 * Set function pointer for readtape() routine. zflag and magtapein must
 * be correctly set before the first call to readtape().
 */
static void
readtape_set(char *buf)
{
	if (!zflag) 
		readtape_func = readtape_uncompr;
	else {
		newcomprbuf(ntrec);
		if (magtapein)
			readtape_func = readtape_comprtape;
		else
			readtape_func = readtape_comprfile;
	}
	readtape(buf);
}

#endif /* HAVE_ZLIB || HAVE_BZLIB */

/*
 * This is the original readtape(), it's used for reading uncompressed input.
 * Read TP_BSIZE blocks from the input.
 * Handle read errors, and end of media.
 */
static void
#if defined(HAVE_ZLIB) || defined(HAVE_BZLIB)
readtape_uncompr(char *buf)
#else
readtape(char *buf)
#endif
{
	ssize_t rd, newvol, i;
	int cnt, seek_failed;

	if (blkcnt < numtrec) {
		memmove(buf, &tbufptr[(blkcnt++ * TP_BSIZE)], TP_BSIZE);
		blksread++;
		tpblksread++;
		return;
	}
	tbufptr = tapebuf;
	for (i = 0; i < ntrec; i++)
		((struct s_spcl *)&tapebuf[i * TP_BSIZE])->c_magic = 0;
	if (numtrec == 0)
		numtrec = ntrec;
	cnt = ntrec * TP_BSIZE;
	rd = 0;
#ifdef USE_QFA
	if (createtapeposflag)
		(void)GetTapePos(&curtapepos);
#endif
getmore:
#ifdef RRESTORE
	if (host)
		i = rmtread(&tapebuf[rd], cnt);
	else
#endif
		i = read(mt, &tapebuf[rd], cnt);

	/*
	 * Check for mid-tape short read error.
	 * If found, skip rest of buffer and start with the next.
	 */
	if (!pipein && numtrec < ntrec && i > 0) {
		Dprintf(stdout, "mid-media short read error.\n");
		numtrec = ntrec;
	}
	/*
	 * Handle partial block read.
	 */
	if (pipein && i == 0 && rd > 0)
		i = rd;
	else if (i > 0 && i != ntrec * TP_BSIZE) {
		if (pipein) {
			rd += i;
			cnt -= i;
			if (cnt > 0)
				goto getmore;
			i = rd;
		} else {
			/*
			 * Short read. Process the blocks read.
			 */
			if (i % TP_BSIZE != 0)
				Vprintf(stdout,
				    "partial block read: %ld should be %ld\n",
				    (long)i, ntrec * TP_BSIZE);
			numtrec = i / TP_BSIZE;
		}
	}
	/*
	 * Handle read error.
	 */
	if (i < 0) {
		fprintf(stderr, "Tape read error while ");
		switch (curfile.action) {
		default:
			fprintf(stderr, "trying to set up tape\n");
			break;
		case UNKNOWN:
			fprintf(stderr, "trying to resynchronize\n");
			break;
		case USING:
			fprintf(stderr, "restoring %s\n", curfile.name);
			break;
		case SKIP:
			fprintf(stderr, "skipping over inode %lu\n",
				(unsigned long)curfile.ino);
			break;
		}
		if (!yflag && !reply("continue"))
			exit(1);
		i = ntrec * TP_BSIZE;
		memset(tapebuf, 0, (size_t)i);
#ifdef RRESTORE
		if (host)
			seek_failed = (rmtseek(i, 1) < 0);
		else
#endif
			seek_failed = (LSEEK(mt, i, SEEK_CUR) == (OFF_T)-1);

		if (seek_failed) {
			warn("continuation failed");
			if (!yflag && !reply("assume end-of-tape and continue"))
				exit(1);
			i = 0;
		}
	}
	/*
	 * Handle end of tape.
	 */
	if (i == 0) {
		Vprintf(stdout, "End-of-tape encountered\n");
		if (!pipein) {
			newvol = volno + 1;
			volno = 0;
			numtrec = 0;
			getvol(newvol);
			readtape(buf);
			return;
		}
		if (rd % TP_BSIZE != 0)
			panic("partial block read: %d should be %d\n",
				rd, ntrec * TP_BSIZE);
		terminateinput();
		memmove(&tapebuf[rd], &endoftapemark, TP_BSIZE);
	}
	blkcnt = 0;
	memmove(buf, &tbufptr[(blkcnt++ * TP_BSIZE)], TP_BSIZE);
	blksread++;
	tpblksread++;
}

#if defined(HAVE_ZLIB) || defined(HAVE_BZLIB)

/*
 * Read a compressed format block from a file or pipe and uncompress it.
 * Attempt to handle read errors, and end of file. 
 */
static void
readtape_comprfile(char *buf)
{
	long rl, size, i, ret;
	int newvol; 
	struct tapebuf *tpb;

	if (blkcnt < numtrec) {
		memmove(buf, &tbufptr[(blkcnt++ * TP_BSIZE)], TP_BSIZE);
		blksread++;
		tpblksread++;
		return;
	}
	/* need to read the next block */
	tbufptr = tapebuf;
	for (i = 0; i < ntrec; i++)
		((struct s_spcl *)&tapebuf[i * TP_BSIZE])->c_magic = 0;
	numtrec = ntrec;
	tpb = (struct tapebuf *) tapebuf;

	/* read the block prefix */
	ret = read_a_block(mt, tapebuf, PREFIXSIZE, &rl);
	converttapebuf(tpb);

	if (Vflag && (ret == 0 || rl < (int)PREFIXSIZE  ||  tpb->length == 0))
		ret = 0;
	if (ret <= 0)
		goto readerr;

	/* read the data */
	size = tpb->length;
	if (size > bufsize)  {
		/* something's wrong */
		Vprintf(stdout, "Prefix size error, max size %d, got %ld\n",
			bufsize, size);
		size = bufsize;
		tpb->length = bufsize;
	}
	ret = read_a_block(mt, tpb->buf, size, &rl);
	if (ret <= 0)
		goto readerr;

	tbufptr = decompress_tapebuf(tpb, rl + PREFIXSIZE);
	if (tbufptr == NULL) {
		msg_read_error("File decompression error while");
		if (!yflag && !reply("continue"))
			exit(1);
		memset(tapebuf, 0, bufsize);
		tbufptr = tapebuf;
	}

	blkcnt = 0;
	memmove(buf, &tbufptr[(blkcnt++ * TP_BSIZE)], TP_BSIZE);
	blksread++;
	tpblksread++;
	return;

readerr:
	/* Errors while reading from a file or pipe are catastrophic. Since
	 * there are no block boundaries, it's impossible to bypass the
	 * block in error and find the start of the next block.
	 */
	if (ret == 0) {
		/* It's possible to have multiple input files using -M
		 * and -f file1,file2...
		 */
		Vprintf(stdout, "End-of-File encountered\n");
		if (!pipein) {
			newvol = volno + 1;
			volno = 0;
			numtrec = 0;
			getvol(newvol);
			readtape(buf);
			return;
		}
	}
	msg_read_error("Read error while");
	/* if (!yflag && !reply("continue")) */
		exit(1);
}

/*
 * Read compressed data from a tape and uncompress it.
 * Handle read errors, and end of media.
 * Since a tape consists of separate physical blocks, we try
 * to recover from errors by repositioning the tape to the next
 * block.
 */
static void
readtape_comprtape(char *buf)
{
	long rl, size, i;
	int ret, newvol;
	struct tapebuf *tpb;
	struct mtop tcom;

	if (blkcnt < numtrec) {
		memmove(buf, &tbufptr[(blkcnt++ * TP_BSIZE)], TP_BSIZE);
		blksread++;
		tpblksread++;
		return;
	}
	/* need to read the next block */
	tbufptr = tapebuf;
	for (i = 0; i < ntrec; i++)
		((struct s_spcl *)&tapebuf[i * TP_BSIZE])->c_magic = 0;
	numtrec = ntrec;
	tpb = (struct tapebuf *) tapebuf;

	/* read the block */
	size = bufsize + PREFIXSIZE;
	ret = read_a_block(mt, tapebuf, size, &rl);
	if (ret <= 0)
		goto readerr;

	converttapebuf(tpb);
	tbufptr = decompress_tapebuf(tpb, rl);
	if (tbufptr == NULL) {
		msg_read_error("Tape decompression error while");
		if (!yflag && !reply("continue"))
			exit(1);
		memset(tapebuf, 0, PREFIXSIZE + bufsize);
		tbufptr = tapebuf;
	}
	goto moverecord;

readerr:
	/* Handle errors: EOT switches to the next volume, other errors
	 * attempt to position the tape to the next block.
	 */
	if (ret == 0) {
		Vprintf(stdout, "End-of-tape encountered\n");
		newvol = volno + 1;
		volno = 0;
		numtrec = 0;
		getvol(newvol);
		readtape(buf);
		return;
	}

	msg_read_error("Tape read error while");
	if (!yflag && !reply("continue"))
		exit(1);
	memset(tapebuf, 0, PREFIXSIZE + bufsize);
	tbufptr = tapebuf;

#ifdef RRESTORE
	if (host)
		rl = rmtioctl(MTFSR, 1);
	else
#endif
	{
		tcom.mt_op = MTFSR;
		tcom.mt_count = 1;
		rl = ioctl(mt, MTIOCTOP, &tcom);
	}

	if (rl < 0) {
		warn("continuation failed");
		if (!yflag && !reply("assume end-of-tape and continue"))
			exit(1);
		ret = 0;         /* end of tape */
		goto readerr;
	}

moverecord:
	blkcnt = 0;
	memmove(buf, &tbufptr[(blkcnt++ * TP_BSIZE)], TP_BSIZE);
	blksread++;
	tpblksread++;
}

/*
 *  Decompress a struct tapebuf into a buffer. readsize is the size read
 *  from the tape/file and is used for error messages. Returns a pointer
 *  to the location of the uncompressed buffer or NULL on errors.
 *  Adjust numtrec and complain for a short block.
 */
static char *
decompress_tapebuf(struct tapebuf *tpbin, int readsize)
{
	/* If zflag is on, all blocks have a struct tapebuf prefix */
	/* zflag gets set in setup() from the dump header          */
	int cresult, blocklen;        
	unsigned long worklen;
#ifdef HAVE_BZLIB
	unsigned int worklen2;
#endif
	char *output = NULL,*reason = NULL, *lengtherr = NULL;              
       
	/* build a length error message */
	blocklen = tpbin->length;
	if (readsize < blocklen + (int)PREFIXSIZE)
		lengtherr = "short";
	else
		if (readsize > blocklen + (int)PREFIXSIZE)
			lengtherr = "long";

	worklen = comprlen;
	cresult = 1;
	if (tpbin->compressed) {
		/* uncompress whatever we read, if it fails, complain later */
		if (tpbin->flags == COMPRESS_ZLIB) {
#ifndef HAVE_ZLIB
			errx(1,"This restore version doesn't support zlib decompression");
#else
			cresult = uncompress(comprbuf, &worklen, 
					     tpbin->buf, blocklen);
			output = comprbuf;
			switch (cresult) {
				case Z_OK:
					break;
				case Z_MEM_ERROR:
					reason = "not enough memory";
					break;
				case Z_BUF_ERROR:
					reason = "buffer too small";
					break;
				case Z_DATA_ERROR:
					reason = "data error";
					break;
				default:
					reason = "unknown";
			}
			if (cresult == Z_OK)
				cresult = 1;
			else
				cresult = 0;
#endif /* HAVE_ZLIB */
		}
		if (tpbin->flags == COMPRESS_BZLIB) {
#ifndef HAVE_BZLIB
			errx(1,"This restore version doesn't support bzlib decompression");
#else
			worklen2 = worklen;
			cresult = BZ2_bzBuffToBuffDecompress(
					comprbuf, &worklen2, 
					tpbin->buf, blocklen, 0, 0);
			worklen = worklen2;
			output = comprbuf;
			switch (cresult) {
				case BZ_OK:
					break;
				case BZ_MEM_ERROR:
					reason = "not enough memory";
					break;
				case BZ_OUTBUFF_FULL:
					reason = "buffer too small";
					break;
				case BZ_DATA_ERROR:
				case BZ_DATA_ERROR_MAGIC:
				case BZ_UNEXPECTED_EOF:
					reason = "data error";
					break;
				default:
					reason = "unknown";
			}
			if (cresult == BZ_OK)
				cresult = 1;
			else
				cresult = 0;
#endif /* HAVE_BZLIB */
		}
	}
	else {
		output = tpbin->buf;
		worklen = blocklen;
	}
	if (cresult) {
		numtrec = worklen / TP_BSIZE;
		if (worklen % TP_BSIZE != 0)
			reason = "length mismatch";
	}
	if (reason) {
		if (lengtherr)
			fprintf(stderr, "%s compressed block: %d expected: %d\n",
				lengtherr, readsize, tpbin->length + PREFIXSIZE);
		fprintf(stderr, "decompression error, block %ld: %s\n",
			tpblksread+1, reason);
		if (!cresult)
			output = NULL;
	}
	return output;
}

/*
 * Print an error message for a read error.
 * This was exteracted from the original readtape().
 */
static void
msg_read_error(char *m)
{
	switch (curfile.action) {
		default:
			fprintf(stderr, "%s trying to set up tape\n", m);
			break;
		case UNKNOWN:
			fprintf(stderr, "%s trying to resynchronize\n", m);
			break;
		case USING:
			fprintf(stderr, "%s restoring %s\n", m, curfile.name);
			break;
		case SKIP:
			fprintf(stderr, "%s skipping over inode %lu\n", m,
				(unsigned long)curfile.ino);
			break;
	}
}
#endif /* HAVE_ZLIB || HAVE_BZLIB */

/*
 * Read the first block and get the blocksize from it. Test
 * for a compressed dump tape/file. setup() will make the final
 * determination by checking the compressed flag if gethead()
 * finds a valid header. The test here is necessary to offset the buffer
 * by the size of the compressed prefix. zflag is set here so that
 * readtape_set can set the correct function pointer for readtape().
 * Note that the first block of each tape/file is not compressed
 * and does not have a prefix.
 */ 
static void
findtapeblksize(void)
{
	long i;
	size_t len;
	struct tapebuf *tpb = (struct tapebuf *) tapebuf;
	struct s_spcl spclpt;

	for (i = 0; i < ntrec; i++)
		((struct s_spcl *)&tapebuf[i * TP_BSIZE])->c_magic = 0;
	blkcnt = 0;
	tbufptr = tapebuf;
	/*
	 * For a pipe or file, read in the first record. For a tape, read
	 * the first block.
	 */
	len = magtapein ? ntrec * TP_BSIZE : TP_BSIZE;

	if (read_a_block(mt, tapebuf, len, &i) <= 0)
		errx(1, "Tape read error on first record");

	memcpy(&spclpt, tapebuf, TP_BSIZE);
	if (converthead(&spclpt) == FAIL) {
		cvtflag++;
		if (converthead(&spclpt) == FAIL) {
			/* Special case for old compressed tapes with prefix */
			if (magtapein && (i % TP_BSIZE != 0)) 
				goto oldformat;
			errx(1, "Tape is not a dump tape");
		}
		fprintf(stderr, "Converting to new file system format.\n");
	}
	/*
	 * If the input is from a file or a pipe, we read TP_BSIZE
	 * bytes looking for a dump header. If the dump is compressed
	 * we need to read in the rest of the block, as determined
	 * by c_ntrec in the dump header. The first block of the
	 * dump is not compressed and does not have a prefix.
	 */
	if (!magtapein) {
		if (spclpt.c_type == TS_TAPE
		    && spclpt.c_flags & DR_COMPRESSED) {
			/* It's a compressed dump file, read in the */
			/* rest of the block based on spclpt.c_ntrec. */
			if (spclpt.c_ntrec > ntrec)
				errx(1, "Tape blocksize is too large, use "
				     "\'-b %d\' ", spclpt.c_ntrec);
			ntrec = spclpt.c_ntrec;
			len = (ntrec - 1) * TP_BSIZE;
			zflag = 1;   
		}
		else {
			/* read in the rest of the block based on bufsize */
			len = bufsize - TP_BSIZE;
		}
		if (read_a_block(mt, tapebuf+TP_BSIZE, len, &i) < 0
		    || (i != (long)len && i % TP_BSIZE != 0))
			errx(1,"Error reading dump file header");
		tbufptr = tapebuf;
		numtrec = ntrec;
		Vprintf(stdout, "Input block size is %ld\n", ntrec);
		return;
	} /* if (!magtapein) */

	/*
	 * If the input is a tape, we tried to read ntrec * TP_BSIZE bytes.
	 * If the value of ntrec is too large, we read less than
	 * what we asked for; adjust the value of ntrec and test for 
	 * a compressed dump tape.
	 */
	if (i % TP_BSIZE != 0) {
oldformat:
		/* may be old format compressed dump tape with a prefix */
		memcpy(&spclpt, tpb->buf, TP_BSIZE);
		cvtflag = 0;
		if (converthead(&spclpt) == FAIL) {
			cvtflag++;
			if (converthead(&spclpt) == FAIL)
				errx(1, "Tape is not a dump tape");
			fprintf(stderr, "Converting to new file system format.\n");
		}
		if (i % TP_BSIZE == PREFIXSIZE
		    && tpb->compressed == 0
		    && spclpt.c_type == TS_TAPE
		    && spclpt.c_flags & DR_COMPRESSED) {
			zflag = 1;
			tbufptr = tpb->buf;
			if (tpb->length > bufsize)
				errx(1, "Tape blocksize is too large, use "
					"\'-b %d\' ", tpb->length / TP_BSIZE);
		}
		else
			errx(1, "Tape block size (%ld) is not a multiple of dump block size (%d)",
				i, TP_BSIZE);
	}
	ntrec = i / TP_BSIZE;
	if (spclpt.c_type == TS_TAPE) {
		if (spclpt.c_flags & DR_COMPRESSED)
			zflag = 1;
		if (spclpt.c_ntrec > ntrec)
			errx(1, "Tape blocksize is too large, use "
				"\'-b %d\' ", spclpt.c_ntrec);
	}
	numtrec = ntrec;
	Vprintf(stdout, "Tape block size is %ld\n", ntrec);
}

/*
 * Read a block of data handling all of the messy details.
 */
static int read_a_block(int fd, char *buf, size_t len, long *lengthread)
{
	long i = 1, size;

	size = len;
	while (size > 0) {
#ifdef RRESTORE
		if (host)
			i = rmtread(buf, size);
		else
#endif
			i = read(fd, buf, size);                 

		if (i <= 0)
			break; /* EOD or error */
		size -= i;
		if (magtapein)
			break; /* block at a time for mt */
		buf += i;
	}
	*lengthread = len - size;
	return i;
}

void
closemt(void)
{

	if (mt < 0)
		return;
#ifdef RRESTORE
	if (host)
		rmtclose();
	else
#endif
		(void) close(mt);
}

static void
setmagtapein(void) {
	struct mtget mt_stat;
	static int done = 0;
	if (done)
		return;
	done = 1;
	if (!pipein) {
		/* need to know if input is really from a tape */
#ifdef RRESTORE
		if (host)
			magtapein = !lflag;
		else
#endif
			magtapein = ioctl(mt, MTIOCGET, (char *)&mt_stat) == 0;
	}

	Vprintf(stdout,"Input is from %s\n", 
			magtapein ? "tape" :
			Vflag ? "multi-volume (no tape)" : "file/pipe");
}

/*
 * Read the next block from the tape.
 * Check to see if it is one of several vintage headers.
 * If it is an old style header, convert it to a new style header.
 * If it is not any valid header, return an error.
 */
static int
gethead(struct s_spcl *buf)
{
	readtape((char *)buf);
	return converthead(buf);
}

static int
converthead(struct s_spcl *buf)
{
	int32_t i;
	union {
		quad_t	qval;
		int32_t	val[2];
	} qcvt;
	union u_ospcl {
		char dummy[TP_BSIZE];
		struct	s_ospcl {
			int32_t	c_type;
			int32_t	c_date;
			int32_t	c_ddate;
			int32_t	c_volume;
			int32_t	c_tapea;
			u_int16_t c_inumber;
			int32_t	c_magic;
			int32_t	c_checksum;
			struct odinode {
				u_int16_t odi_mode;
				u_int16_t odi_nlink;
				u_int16_t odi_uid;
				u_int16_t odi_gid;
				int32_t	odi_size;
				int32_t	odi_rdev;
				char	odi_addr[36];
				int32_t	odi_atime;
				int32_t	odi_mtime;
				int32_t	odi_ctime;
			} c_dinode;
			int32_t	c_count;
			char	c_fill[256];
		} s_ospcl;
	} u_ospcl;

	if (!cvtflag) {
		if (buf->c_magic != NFS_MAGIC) {
			if (swabi(buf->c_magic) != NFS_MAGIC)
				return (FAIL);
			if (!Bcvt) {
				Vprintf(stdout, "Note: Doing Byte swapping\n");
				Bcvt = 1;
			}
		}
		if (checksum((int *)buf) == FAIL)
			return (FAIL);
		if (Bcvt)
			swabst((u_char *)"8i4s31i528bi192b3i", (u_char *)buf);
		goto good;
	}
	memcpy(&u_ospcl.s_ospcl, buf, TP_BSIZE);
	memset((char *)buf, 0, (long)TP_BSIZE);
	buf->c_type = u_ospcl.s_ospcl.c_type;
	buf->c_date = u_ospcl.s_ospcl.c_date;
	buf->c_ddate = u_ospcl.s_ospcl.c_ddate;
	buf->c_volume = u_ospcl.s_ospcl.c_volume;
	buf->c_tapea = u_ospcl.s_ospcl.c_tapea;
	buf->c_inumber = u_ospcl.s_ospcl.c_inumber;
	buf->c_checksum = u_ospcl.s_ospcl.c_checksum;
	buf->c_magic = u_ospcl.s_ospcl.c_magic;
	buf->c_dinode.di_mode = u_ospcl.s_ospcl.c_dinode.odi_mode;
	buf->c_dinode.di_nlink = u_ospcl.s_ospcl.c_dinode.odi_nlink;
	buf->c_dinode.di_uid = u_ospcl.s_ospcl.c_dinode.odi_uid;
	buf->c_dinode.di_gid = u_ospcl.s_ospcl.c_dinode.odi_gid;
	buf->c_dinode.di_size = u_ospcl.s_ospcl.c_dinode.odi_size;
	buf->c_dinode.di_rdev = u_ospcl.s_ospcl.c_dinode.odi_rdev;
#if defined(__linux__) || defined(sunos)
	buf->c_dinode.di_atime.tv_sec = u_ospcl.s_ospcl.c_dinode.odi_atime;
	buf->c_dinode.di_mtime.tv_sec = u_ospcl.s_ospcl.c_dinode.odi_mtime;
	buf->c_dinode.di_ctime.tv_sec = u_ospcl.s_ospcl.c_dinode.odi_ctime;
#else	/* __linux__ || sunos */
	buf->c_dinode.di_atime = u_ospcl.s_ospcl.c_dinode.odi_atime;
	buf->c_dinode.di_mtime = u_ospcl.s_ospcl.c_dinode.odi_mtime;
	buf->c_dinode.di_ctime = u_ospcl.s_ospcl.c_dinode.odi_ctime;
#endif	/* __linux__ || sunos */
	buf->c_count = u_ospcl.s_ospcl.c_count;
	memmove(buf->c_addr, u_ospcl.s_ospcl.c_fill, (long)256);
	if (u_ospcl.s_ospcl.c_magic != OFS_MAGIC ||
	    checksum((int *)(&u_ospcl.s_ospcl)) == FAIL)
		return(FAIL);
	buf->c_magic = NFS_MAGIC;

good:
	if ((buf->c_dinode.di_size == 0 || buf->c_dinode.di_size > 0xfffffff) &&
	    (buf->c_dinode.di_mode & IFMT) == IFDIR && Qcvt == 0) {
		qcvt.qval = buf->c_dinode.di_size;
		if (qcvt.val[0] || qcvt.val[1]) {
			Vprintf(stdout, "Note: Doing Quad swapping\n");
			Qcvt = 1;
		}
	}
	if (Qcvt) {
		qcvt.qval = buf->c_dinode.di_size;
		i = qcvt.val[1];
		qcvt.val[1] = qcvt.val[0];
		qcvt.val[0] = i;
		buf->c_dinode.di_size = qcvt.qval;
	}
	readmapflag = 0;

	switch (buf->c_type) {

	case TS_CLRI:
	case TS_BITS:
		/*
		 * Have to patch up missing information in bit map headers
		 */
		buf->c_inumber = 0;
		buf->c_dinode.di_size = buf->c_count * TP_BSIZE;
		if (buf->c_count > TP_NINDIR)
			readmapflag = 1;
		else 
			for (i = 0; i < buf->c_count; i++)
				buf->c_addr[i]++;
		break;

	case TS_TAPE:
		if ((buf->c_flags & DR_NEWINODEFMT) == 0)
			oldinofmt = 1;
		/* fall through */
	case TS_END:
		buf->c_inumber = 0;
		if (buf->c_flags & DR_INODEINFO) {
			memcpy(volinfo, buf->c_inos, TP_NINOS * sizeof(dump_ino_t));
			if (Bcvt)
				swabst((u_char *)"128i", (u_char *)volinfo);
		}
		break;

	case TS_INODE:
	case TS_ADDR:
		break;

	default:
		panic("gethead: unknown inode type %d\n", buf->c_type);
		break;
	}
	/*
	 * If we are restoring a filesystem with old format inodes,
	 * copy the uid/gid to the new location.
	 */
	if (oldinofmt) {
		buf->c_dinode.di_uid = buf->c_dinode.di_ouid;
		buf->c_dinode.di_gid = buf->c_dinode.di_ogid;
	}
	if (dflag)
		accthdr(buf);
	return(GOOD);
}

static void
converttapebuf(struct tapebuf *tpb)
{
	if (Bcvt) {
		struct tb {
			unsigned int    length:28;
			unsigned int    flags:3;
			unsigned int    compressed:1;
		} tb;
		swabst((u_char *)"i", (u_char *)tpb);
		memcpy(&tb, tpb, 4);	
		tpb->length = tb.length;
		tpb->flags = tb.flags;
		tpb->compressed = tb.compressed;
	}
}

/*
 * Check that a header is where it belongs and predict the next header
 */
static void
accthdr(struct s_spcl *header)
{
	static dump_ino_t previno = 0x7fffffff;
	static int prevtype;
	static long predict;
	long blks, i;

	if (header->c_type == TS_TAPE) {
		fprintf(stderr, "Volume header (%s inode format) ",
		    oldinofmt ? "old" : "new");
 		if (header->c_firstrec)
 			fprintf(stderr, "begins with record %d",
 				header->c_firstrec);
 		fprintf(stderr, "\n");
		previno = 0x7fffffff;
		return;
	}
	if (previno == 0x7fffffff)
		goto newcalc;
	switch (prevtype) {
	case TS_BITS:
		fprintf(stderr, "Dumped inodes map header");
		break;
	case TS_CLRI:
		fprintf(stderr, "Used inodes map header");
		break;
	case TS_INODE:
		fprintf(stderr, "File header, ino %lu", (unsigned long)previno);
		break;
	case TS_ADDR:
		fprintf(stderr, "File continuation header, ino %ld", (long)previno);
		break;
	case TS_END:
		fprintf(stderr, "End of tape header");
		break;
	}
	if (predict != blksread - 1)
		fprintf(stderr, "; predicted %ld blocks, got %ld blocks",
			predict, blksread - 1);
	fprintf(stderr, "\n");
newcalc:
	blks = 0;
	if (header->c_type != TS_END)
		for (i = 0; i < header->c_count; i++)
			if (readmapflag || header->c_addr[i] != 0)
				blks++;
	predict = blks;
	blksread = 0;
	prevtype = header->c_type;
	previno = header->c_inumber;
}

/*
 * Find an inode header.
 * Complain if had to skip, and complain is set.
 */
static void
findinode(struct s_spcl *header)
{
	static long skipcnt = 0;
	long i;
	char buf[TP_BSIZE];

	curfile.name = "<name unknown>";
	curfile.action = UNKNOWN;
	curfile.dip = NULL;
	curfile.ino = 0;
	do {
		if (header->c_magic != NFS_MAGIC) {
			skipcnt++;
			while (gethead(header) == FAIL ||
			    header->c_date != dumpdate)
				skipcnt++;
		}
		switch (header->c_type) {

		case TS_ADDR:
			/*
			 * Skip up to the beginning of the next record
			 */
			for (i = 0; i < header->c_count; i++)
				if (header->c_addr[i])
					readtape(buf);
			while (gethead(header) == FAIL ||
			    header->c_date != dumpdate)
				skipcnt++;
			break;

		case TS_INODE:
			curfile.dip = &header->c_dinode;
			curfile.ino = header->c_inumber;
			break;

		case TS_END:
			curfile.ino = maxino;
			break;

		case TS_CLRI:
			curfile.name = "<file removal list>";
			break;

		case TS_BITS:
			curfile.name = "<file dump list>";
			break;

		case TS_TAPE:
			panic("unexpected tape header\n");
			/* NOTREACHED */

		default:
			panic("unknown tape header type %d\n", spcl.c_type);
			/* NOTREACHED */

		}
	} while (header->c_type == TS_ADDR);
	if (skipcnt > 0)
#ifdef USE_QFA
		if (!noresyncmesg)
#endif
			fprintf(stderr, "resync restore, skipped %ld blocks\n",
		    		skipcnt);
	skipcnt = 0;
}

static int
checksum(int *buf)
{
	int i, j;

	j = sizeof(union u_spcl) / sizeof(int);
	i = 0;
	if(!Bcvt) {
		do
			i += *buf++;
		while (--j);
	} else {
		/* What happens if we want to read restore tapes
			for a 16bit int machine??? */
		do
			i += swabi(*buf++);
		while (--j);
	}

	if (i != CHECKSUM) {
		fprintf(stderr, "Checksum error %o, inode %lu file %s\n", i,
			(unsigned long)curfile.ino, curfile.name);
		return(FAIL);
	}
	return(GOOD);
}

#ifdef RRESTORE
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

void
#ifdef __STDC__
msg(const char *fmt, ...)
#else
msg(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list ap;
#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
}
#endif /* RRESTORE */

static u_char *
swab16(u_char *sp, int n)
{
	char c;

	while (--n >= 0) {
		c = sp[0]; sp[0] = sp[1]; sp[1] = c;
		sp += 2;
	}
	return (sp);
}

static u_char *
swab32(u_char *sp, int n)
{
	char c;

	while (--n >= 0) {
		c = sp[0]; sp[0] = sp[3]; sp[3] = c;
		c = sp[1]; sp[1] = sp[2]; sp[2] = c;
		sp += 4;
	}
	return (sp);
}

static u_char *
swab64(u_char *sp, int n)
{
	char c;

	while (--n >= 0) {
		c = sp[0]; sp[0] = sp[7]; sp[7] = c;
		c = sp[1]; sp[1] = sp[6]; sp[6] = c;
		c = sp[2]; sp[2] = sp[5]; sp[5] = c;
		c = sp[3]; sp[3] = sp[4]; sp[4] = c;
		sp += 8;
	}
	return (sp);
}

void
swabst(u_char *cp, u_char *sp)
{
	int n = 0;

	while (*cp) {
		switch (*cp) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			n = (n * 10) + (*cp++ - '0');
			continue;

		case 's': case 'w': case 'h':
			if (n == 0)
				n = 1;
			sp = swab16(sp, n);
			break;

		case 'i':
			if (n == 0)
				n = 1;
			sp = swab32(sp, n);
			break;

		case 'l':
			if (n == 0)
				n = 1;
			sp = swab64(sp, n);
			break;

		default: /* Any other character, like 'b' counts as byte. */
			if (n == 0)
				n = 1;
			sp += n;
			break;
		}
		cp++;
		n = 0;
	}
}

static u_int
swabi(u_int x)
{
	swabst((u_char *)"i", (u_char *)&x);
	return (x);
}

#if 0
static u_long
swabl(u_long x)
{
	swabst((u_char *)"l", (u_char *)&x);
	return (x);
}
#endif

#ifdef USE_QFA
/*
 * get the current position of the tape
 */
int
GetTapePos(long long *pos)
{
	int err = 0;

#ifdef RDUMP
	if (host) {
		*pos = (long long) rmtseek(0, SEEK_CUR);
		err = *pos < 0;
	}
	else
#endif
	{
	if (magtapein) {
		long mtpos;
		*pos = 0;
		err = (ioctl(mt, MTIOCPOS, &mtpos) < 0);
		*pos = (long long)mtpos;
	}
	else {
		*pos = LSEEK(mt, 0, SEEK_CUR);
		err = (*pos < 0);
	}
	}
	if (err) {
		err = errno;
		fprintf(stdout, "[%ld] error: %d (getting tapepos: %lld)\n", 
			(unsigned long)getpid(), err, *pos);
		return err;
	}
	return err;
}

typedef struct mt_pos {
	short	 mt_op;
	int	 mt_count;
} MTPosRec, *MTPosPtr;

/*
 * go to specified position on tape
 */
int
GotoTapePos(long long pos)
{
	int err = 0;

#ifdef RDUMP
	if (host)
		err = (rmtseek(pos, SEEK_SET) < 0);
	else
#endif
	{
	if (magtapein) {
		struct mt_pos buf;
		buf.mt_op = MTSEEK;
		buf.mt_count = (int) pos;
		err = (ioctl(mt, MTIOCTOP, &buf) < 0);
	}
	else {
		pos = LSEEK(mt, pos, SEEK_SET);
		err = (pos < 0);
	}
	}
	if (err) {
		err = errno;
		fprintf(stdout, "[%ld] error: %d (setting tapepos: %lld)\n", 
			(unsigned long)getpid(), err, pos);
		return err;
	}
	return err;
}

/*
 * read next data from tape to re-sync
 */
void
ReReadFromTape(void)
{
	FLUSHTAPEBUF();
	noresyncmesg = 1;
	if (gethead(&spcl) == FAIL) {
#ifdef DEBUG_QFA
		fprintf(stdout, "DEBUG 1 gethead failed\n");
#endif
	}
	findinode(&spcl);
	noresyncmesg = 0;
}

void
ReReadInodeFromTape(dump_ino_t theino)
{
	long cntloop = 0;

	FLUSHTAPEBUF();
	noresyncmesg = 1;
	do {
		cntloop++;
		gethead(&spcl);
	} while (!(spcl.c_inumber == theino && spcl.c_type == TS_INODE && spcl.c_date == dumpdate) && (cntloop < 32));
#ifdef DEBUG_QFA
	fprintf(stderr, "%ld reads\n", cntloop);
	if (cntloop == 32) {
		fprintf(stderr, "DEBUG: bufsize %d\n", bufsize);
		fprintf(stderr, "DEBUG: ntrec %ld\n", ntrec);
		fprintf(stderr, "DEBUG: %ld reads\n", cntloop);
	}
#endif
	findinode(&spcl);
	noresyncmesg = 0;
}
#endif /* USE_QFA */

void
RequestVol(long tnum)
{
	FLUSHTAPEBUF();
	getvol(tnum);
}
