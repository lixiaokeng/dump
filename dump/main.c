/*
 *	Ported to Linux's Second Extended File System as part of the
 *	dump and restore backup suit
 *	Remy Card <card@Linux.EU.Org>, 1994-1997
 *	Stelian Pop <stelian@popies.net>, 1999-2000
 *	Stelian Pop <stelian@popies.net> - Alcôve <www.alcove.com>, 2000-2002
 */

/*-
 * Copyright (c) 1980, 1991, 1993, 1994
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

#ifndef lint
static const char rcsid[] =
	"$Id: main.c,v 1.95 2009/06/18 09:35:14 stelian Exp $";
#endif /* not lint */

#include <config.h>
#include <compatlfs.h>
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

#include "dump.h"
#include "pathnames.h"
#include "bylabel.h"

#ifndef SBOFF
#define SBOFF (SBLOCK * DEV_BSIZE)
#endif

int abortifconnerr = 1;		/* set to 1 if lib dumprmt.o should exit on connection errors
                                otherwise just print a message using msg */
/*
 * Dump maps used to describe what is to be dumped.
 */
int	mapsize;	/* size of the state maps */
char	*usedinomap;	/* map of allocated inodes */
char	*dumpdirmap;	/* map of directories to be dumped */
char	*dumpinomap;	/* map of files to be dumped */
char	*metainomap;	/* which of the inodes in dumpinomap will get
			   only their metadata dumped */

const char *disk;	/* name of the disk file */
char	tape[MAXPATHLEN];/* name of the tape file */
char	*tapeprefix;	/* prefix of the tape file */
char	*dumpdates;	/* name of the file containing dump date information*/
char	lastlevel[NUM_STR_SIZE];/* dump level of previous dump */
char	level[NUM_STR_SIZE];/* dump level of this dump */
int	zipflag;	/* which compression method */
int	Afile = -1;	/* archive file descriptor */
int	AfileActive = 1;/* Afile flag */
int	uflag;		/* update flag */
int	mflag;		/* dump metadata only if possible */
int	Mflag;		/* multi-volume flag */
int	qflag;		/* quit on errors flag */
int	vflag;		/* verbose flag */
int     breademax = 32; /* maximum number of bread errors before we quit */
char	*eot_script;	/* end of volume script fiag */
int	diskfd;		/* disk file descriptor */
int	tapefd;		/* tape file descriptor */
int	pipeout;	/* true => output to standard output */
int	fifoout;	/* true => output to fifo */
dump_ino_t curino;	/* current inumber; used globally */
int	newtape;	/* new tape flag */
int	density;	/* density in 0.1" units */
long	tapesize;	/* estimated tape size, blocks */
long	tsize;		/* tape size in 0.1" units */
long	asize;		/* number of 0.1" units written on current tape */
int	etapes;		/* estimated number of tapes */
int	nonodump;	/* if set, do not honor UF_NODUMP user flags */
int	unlimited;	/* if set, write to end of medium */
int	compressed;	/* if set, dump is to be compressed */
long long bytes_written;/* total bytes written to tape */
long	uncomprblks;	/* uncompressed blocks written to tape */
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
dump_ino_t volinfo[TP_NINOS];/* which inode on which volume archive info */

#ifdef USE_QFA
int	gTapeposfd;
char	*gTapeposfile;
char	gTps[255];
int32_t	gThisDumpDate;
#endif /* USE_QFA */

struct	dumptime *dthead;	/* head of the list version */
int	nddates;		/* number of records (might be zero) */
int	ddates_in;		/* we have read the increment file */
struct	dumpdates **ddatev;	/* the arrayfied version */

int	notify = 0;	/* notify operator flag */
int	blockswritten = 0;	/* number of blocks written on current tape */
int	tapeno = 0;	/* current tape number */
int	density = 0;	/* density in bytes/0.1" " <- this is for hilit19 */
int	ntrec = NTREC;	/* # blocks in each tape record */
int	cartridge = 0;	/* Assume non-cartridge tape */
#ifdef USE_QFA
int	tapepos = 0; 	/* assume no QFA tapeposition needed by user */
#endif /* USE_QFA */
int	dokerberos = 0;	/* Use Kerberos authentication */
long	dev_bsize = 1;	/* recalculated below */
long	*blocksperfiles = NULL; /* output blocks per file(s) */
char	*host = NULL;	/* remote host (if any) */
int	sizest = 0;	/* return size estimate only */
int	compressed = 0;	/* use zlib to compress the output, compress level 1-9 */
long long bytes_written = 0; /* total bytes written */
long	uncomprblks = 0;/* uncompressed blocks written */

long smtc_errno;

#ifdef	__linux__
char	*__progname;
#endif

int 	maxbsize = 1024*1024;     /* XXX MAXBSIZE from sys/param.h */
static long numarg __P((const char *, long, long));
static long *numlistarg __P((const char *, long, long));
static void obsolete __P((int *, char **[]));
static void usage __P((void));
static void do_exclude_from_file __P((char *));
static void do_exclude_ino_str __P((char *));
static void incompat_flags __P((int, char, char));

static char* iexclude_bitmap = NULL;		/* the inode exclude bitmap */
static unsigned int iexclude_bitmap_bytes = 0;	/* size of bitmap in bytes */

int
main(int argc, char *argv[])
{
	dump_ino_t ino;
	int dirty;
	struct dinode *dp;
	struct mntent *dt;
	char *map;
	int ch, pch = 0;
	int i, anydirskipped;
	int aflag = 0, bflag = 0, Tflag = 0, honorlevel = 1;
	dump_ino_t maxino;
	struct STAT statbuf;
	dev_t filedev = 0;
#ifdef	__linux__
	errcode_t retval;
	char directory[MAXPATHLEN];
	char pathname[MAXPATHLEN];
#endif
	time_t tnow;
	char *diskparam;
	char *Apath = NULL;

	spcl.c_label[0] = '\0';
	spcl.c_date = time(NULL);

#ifdef __linux__
	__progname = argv[0];
	directory[0] = 0;
	initialize_ext2_error_table();
#endif

	tsize = 0;	/* Default later, based on 'c' option for cart tapes */
	unlimited = 1;
	eot_script = NULL;
	if ((tapeprefix = getenv("TAPE")) == NULL)
		tapeprefix = _PATH_DEFTAPE;
	dumpdates = _PATH_DUMPDATES;
	if (TP_BSIZE / DEV_BSIZE == 0 || TP_BSIZE % DEV_BSIZE != 0)
		quit("TP_BSIZE must be a multiple of DEV_BSIZE\n");
	memset(&lastlevel, 0, NUM_STR_SIZE);
	memset(&level, 0, NUM_STR_SIZE);
	/* Default dump level is zero. */
	level[0] = '0';

	if (argc < 2)
		usage();

	obsolete(&argc, &argv);

#ifdef USE_QFA
	gTapeposfd = -1;
#endif /* USE_QFA */

	while ((ch = getopt(argc, argv,
			    "0123456789A:aB:b:cd:D:e:E:f:F:h:I:"
#ifdef HAVE_BZLIB
			    "j::"
#endif
			    "L:"
#ifdef KERBEROS
			    "k"
#endif
			    "mMnq"
#ifdef USE_QFA
			    "Q:"
#endif
			    "s:ST:uvWw"
#ifdef HAVE_LZO
			    "y"
#endif
#ifdef HAVE_ZLIB
			    "z::"
#endif
			    )) != -1)
		switch (ch) {
		/* dump level */
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			if ((pch >= '0') && (pch <= '9') && (strlen(level) < NUM_STR_SIZE))
				level[strlen(level)] = ch;
			else 
				level[0] = ch;
			pch = ch;
			break;

		case 'A':		/* archive file */
			Apath = optarg;
			break;

		case 'a':		/* `auto-size', Write to EOM. */
			unlimited = 1;
			aflag = 1;
			break;

		case 'B':		/* blocks per output file */
			unlimited = 0;
			blocksperfiles = numlistarg("number of blocks per file",
			    1L, 0L);
			break;

		case 'b':		/* blocks per tape write */
			ntrec = numarg("number of blocks per write",
			    1L, 1048576L);
			if (ntrec > maxbsize/1024) {
				msg("Please choose a blocksize <= %dkB\n",
					maxbsize/1024);
				msg("The ENTIRE dump is aborted.\n");
				exit(X_STARTUP);
			}
			bflag = 1;
			break;

		case 'c':		/* Tape is cart. not 9-track */
			unlimited = 0;
			cartridge = 1;
			break;

		case 'd':		/* density, in bits per inch */
			unlimited = 0;
			density = numarg("density", 10L, 327670L) / 10;
			if (density >= 625 && !bflag)
				ntrec = HIGHDENSITYTREC;
			break;

		case 'D':		/* path of dumpdates file */
			dumpdates = optarg;
			break;
			
			                /* 04-Feb-00 ILC */
		case 'e':		/* exclude an inode */
			{
			char *p = optarg, *q;
			while ((q = strchr(p, ','))) {
				*q = '\0';
				do_exclude_ino_str(p);
				p = q + 1;
			}
			do_exclude_ino_str(p);
			}
			break;

		case 'E':		/* exclude inodes read from file */
			do_exclude_from_file(optarg);
			break;

		case 'f':		/* output file */
			tapeprefix = optarg;
			break;

		case 'F':		/* end of tape script */
			eot_script = optarg;
			break;

		case 'h':
			honorlevel = numarg("honor level", 0L, 10L);
			break;

#ifdef HAVE_BZLIB
		case 'j':
			compressed = 2;
			zipflag = COMPRESS_BZLIB;
			if (optarg)
				compressed = numarg("compress level", 1L, 9L);
			break;
#endif /* HAVE_BZLIB */

	        case 'I':
		        breademax =
			  numarg ("number of errors to ignore", 0L, 0L);
			break;

#ifdef KERBEROS
		case 'k':
			dokerberos = 1;
			break;
#endif

		case 'L':
			/*
			 * Note that although there are LBLSIZE characters,
			 * the last must be '\0', so the limit on strlen()
			 * is really LBLSIZE-1.
			 */
			strncpy(spcl.c_label, optarg, LBLSIZE);
			spcl.c_label[LBLSIZE-1] = '\0';
			if (strlen(optarg) > LBLSIZE-1) {
				msg(
		"WARNING Label `%s' is larger than limit of %d characters.\n",
				    optarg, LBLSIZE-1);
				msg("WARNING: Using truncated label `%s'.\n",
				    spcl.c_label);
			}
			break;

		case 'm':		/* metadata only flag */
			mflag = 1;
			break;

		case 'M':		/* multi-volume flag */
			Mflag = 1;
			break;

		case 'n':		/* notify operators */
			notify = 1;
			break;

		case 'q':
			qflag = 1;
			break;

#ifdef USE_QFA
		case 'Q':		/* create tapeposfile */
			gTapeposfile = optarg;
			tapepos = 1;
			break;
#endif /* USE_QFA */
			
		case 's':		/* tape size, feet */
			unlimited = 0;
			tsize = numarg("tape size", 1L, 0L) * 12 * 10;
			break;

		case 'S':
			sizest = 1;	/* return size estimate only */
			break;

		case 'T':		/* time of last dump */
			spcl.c_ddate = unctime(optarg);
			if (spcl.c_ddate < 0) {
				msg("bad time \"%s\"\n", optarg);
				msg("The ENTIRE dump is aborted.\n");
				exit(X_STARTUP);
			}
			Tflag = 1;
			lastlevel[0] = '?'; lastlevel[1] = '\0'; 
			break;

		case 'u':		/* update dumpdates */
			uflag = 1;
			break;

		case 'v':		/* verbose */
			vflag = 1;
			break;

		case 'W':		/* what to do */
		case 'w':
			lastdump(ch);
			exit(X_FINOK);	/* do nothing else */
#ifdef HAVE_LZO
		case 'y':
                	compressed = 2;
			zipflag = COMPRESS_LZO;
			break;
#endif /* HAVE_LZO */

#ifdef HAVE_ZLIB
		case 'z':
			compressed = 2;
			zipflag = COMPRESS_ZLIB;
			if (optarg)
				compressed = numarg("compress level", 1L, 9L);
			break;
#endif /* HAVE_ZLIB */

		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc < 1) {
		msg("Must specify disk or filesystem\n");
		msg("The ENTIRE dump is aborted.\n");
		exit(X_STARTUP);
	}
	diskparam = *argv++;
	if (strlen(diskparam) >= MAXPATHLEN) {
		msg("Disk or filesystem name too long: %s\n", diskparam);
		msg("The ENTIRE dump is aborted.\n");
		exit(X_STARTUP);
	}
	argc--;
	incompat_flags(Tflag && uflag, 'T', 'u');
	incompat_flags(aflag && blocksperfiles, 'a', 'B');
	incompat_flags(aflag && cartridge, 'a', 'c');
	incompat_flags(aflag && density, 'a', 'd');
	incompat_flags(aflag && tsize, 'a', 's');

	if (strcmp(tapeprefix, "-") == 0) {
		pipeout++;
		tapeprefix = "standard output";
	}

	if (blocksperfiles && !compressed)
		for (i = 1; i <= *blocksperfiles; i++)
			blocksperfiles[i] = blocksperfiles[i] / ntrec * ntrec; /* round down */
	else if (!unlimited) {
		/*
		 * Determine how to default tape size and density
		 *
		 *         	density				tape size
		 * 9-track	1600 bpi (160 bytes/.1")	2300 ft.
		 * 9-track	6250 bpi (625 bytes/.1")	2300 ft.
		 * cartridge	8000 bpi (100 bytes/.1")	1700 ft.
		 *						(450*4 - slop)
		 * hilit19 hits again: "
		 */
		if (density == 0)
			density = cartridge ? 100 : 160;
		if (tsize == 0)
			tsize = cartridge ? 1700L*120L : 2300L*120L;
	}

	{
		int i;
		char *n;

		if ((n = strchr(tapeprefix, ':'))) {
			for (i = 0; i < (n - tapeprefix); i++) {
				if (tapeprefix[i] == '/')
					break;
			}
			if (tapeprefix[i] != '/') {
				host = tapeprefix;
				tapeprefix = strchr(host, ':');
				*tapeprefix++ = '\0';
#ifdef RDUMP
				if (index(tapeprefix, '\n')) {
					msg("invalid characters in tape\n");
					msg("The ENTIRE dump is aborted.\n");
					exit(X_STARTUP);
				}
				if (rmthost(host) == 0)
					exit(X_STARTUP);
#else
				msg("remote dump not enabled\n");
				msg("The ENTIRE dump is aborted.\n");
				exit(X_STARTUP);
#endif
			}
		}
	}

	(void)setuid(getuid()); /* rmthost() is the only reason to be setuid */
	if (Apath && (Afile = open(Apath, O_WRONLY|O_CREAT|O_TRUNC,
				   S_IRUSR | S_IWUSR | S_IRGRP |
				   S_IWGRP | S_IROTH | S_IWOTH)) < 0) {
		msg("Cannot open %s for writing: %s\n",
		    optarg, strerror(errno));
		msg("The ENTIRE dump is aborted.\n");
		exit(X_STARTUP);
	}

	if (signal(SIGHUP, SIG_IGN) != SIG_IGN)
		signal(SIGHUP, sig);
	if (signal(SIGTRAP, SIG_IGN) != SIG_IGN)
		signal(SIGTRAP, sig);
	if (signal(SIGFPE, SIG_IGN) != SIG_IGN)
		signal(SIGFPE, sig);
	if (signal(SIGBUS, SIG_IGN) != SIG_IGN)
		signal(SIGBUS, sig);
	if (signal(SIGSEGV, SIG_IGN) != SIG_IGN)
		signal(SIGSEGV, sig);
	if (signal(SIGTERM, SIG_IGN) != SIG_IGN)
		signal(SIGTERM, sig);
	if (signal(SIGINT, interrupt) == SIG_IGN)
		signal(SIGINT, SIG_IGN);
#ifdef SIGXCPU
	signal(SIGXCPU, SIG_IGN);
#endif /* SIGXCPU */
#ifdef SIGXFSZ
	signal(SIGXFSZ, SIG_IGN);
#endif /* SIGXFSZ */

	set_operators();	/* /etc/group snarfed */
	getfstab();		/* /etc/fstab snarfed */

	/*
	 *      disk may end in / and this can confuse
	 *      fstabsearch.
	 */
	i = strlen(diskparam) - 1;
	if (i > 1 && diskparam[i] == '/')
		if (!(i == 6 && !strcmp(diskparam, "LABEL=/")))
			diskparam[i] = '\0';

	disk = get_device_name(diskparam);
	if (!disk)
		disk = strdup(diskparam);

	/*
	 *	disk can be either the full special file name,
	 *	the suffix of the special file name,
	 *	the special name missing the leading '/',
	 *	the file system name with or without the leading '/'.
	 */
	if ((dt = fstabsearch(disk)) != NULL) {
		/* if found then only one parameter (i.e. partition)
		 * is allowed */
		if (argc >= 1) {
			(void)fprintf(stderr, "Unknown arguments to dump:");
			while (argc--)
				(void)fprintf(stderr, " %s", *argv++);
			(void)fprintf(stderr, "\n");
			msg("The ENTIRE dump is aborted.\n");
			exit(X_STARTUP);
		}
		disk = rawname(dt->mnt_fsname);
		(void)strncpy(spcl.c_dev, dt->mnt_fsname, NAMELEN);
		(void)strncpy(spcl.c_filesys, dt->mnt_dir, NAMELEN);
	} else {
#ifdef	__linux__
#ifdef	HAVE_REALPATH
		if (realpath(disk, pathname) == NULL)
#endif
			strcpy(pathname, disk);
		/*
		 * The argument could be now a mountpoint of
		 * a filesystem specified in fstab. Search for it.
		 */
		if ((dt = fstabsearch(pathname)) != NULL) {
			disk = rawname(dt->mnt_fsname);
			(void)strncpy(spcl.c_dev, dt->mnt_fsname, NAMELEN);
			(void)strncpy(spcl.c_filesys, dt->mnt_dir, NAMELEN);
		} else {
			/*
			 * The argument was not found in the fstab
			 * assume that this is a subtree and search for it
			 */
			dt = fstabsearchdir(pathname, directory);
			if (dt != NULL) {
				char name[MAXPATHLEN];
				(void)strncpy(spcl.c_dev, dt->mnt_fsname, NAMELEN);
				(void)snprintf(name, sizeof(name), "%s (dir %s)",
					      dt->mnt_dir, directory);
				(void)strncpy(spcl.c_filesys, name, NAMELEN);
				disk = rawname(dt->mnt_fsname);
			} else {
				(void)strncpy(spcl.c_dev, disk, NAMELEN);
				(void)strncpy(spcl.c_filesys, "an unlisted file system",
				    NAMELEN);
			}
		}
#else
		(void)strncpy(spcl.c_dev, disk, NAMELEN);
		(void)strncpy(spcl.c_filesys, "an unlisted file system",
		    NAMELEN);
#endif
	}

	if (directory[0] != 0) {
		if (atoi(level) != 0) {
			msg("Only level 0 dumps are allowed on a subdirectory\n");
			msg("The ENTIRE dump is aborted.\n");
			exit(X_STARTUP);
		}
		if (uflag) {
			msg("You can't update the dumpdates file when dumping a subdirectory\n");
			msg("The ENTIRE dump is aborted.\n");
			exit(X_STARTUP);
		}
	}
	spcl.c_dev[NAMELEN-1] = '\0';
	spcl.c_filesys[NAMELEN-1] = '\0';
	(void)gethostname(spcl.c_host, NAMELEN);
	spcl.c_host[NAMELEN-1] = '\0';
	spcl.c_level = atoi(level);
	spcl.c_type = TS_TAPE;
	if (!Tflag)
	        getdumptime(uflag);		/* dumpdates snarfed */

	if (spcl.c_ddate == 0 && spcl.c_level) {
		msg("WARNING: There is no inferior level dump on this filesystem\n"); 
		msg("WARNING: Assuming a level 0 dump by default\n");
		level[0] = '0'; level[1] = '\0';
		spcl.c_level = 0;
	}

	if (Mflag)
		snprintf(tape, MAXPATHLEN, "%s%03d", tapeprefix, tapeno + 1);
	else
		strncpy(tape, tapeprefix, MAXPATHLEN);
	tape[MAXPATHLEN - 1] = '\0';

	if (!pipeout) {
		if (STAT(tape, &statbuf) != -1)
			fifoout= statbuf.st_mode & S_IFIFO;
	}

	if (!sizest) {

		msg("Date of this level %s dump: %s", level,
		    ctime4(&spcl.c_date));
#ifdef USE_QFA
		gThisDumpDate = spcl.c_date;
#endif
		if (spcl.c_ddate)
	 		msg("Date of last level %s dump: %s", lastlevel,
			    ctime4(&spcl.c_ddate));
		msg("Dumping %s (%s) ", disk, spcl.c_filesys);
		if (host)
			msgtail("to %s on host %s\n", tape, host);
		else
			msgtail("to %s\n", tape);
	} /* end of size estimate */

#ifdef	__linux__
	if ((diskfd = OPEN(disk, O_RDONLY)) < 0) {
		msg("Cannot open %s\n", disk);
		msg("The ENTIRE dump is aborted.\n");
		exit(X_STARTUP);
	}
#ifdef BLKFLSBUF
	(void)ioctl(diskfd, BLKFLSBUF, 0);
#endif
	retval = dump_fs_open(disk, &fs);
	if (retval) {
		com_err(disk, retval, "while opening filesystem");
		if (retval == EXT2_ET_REV_TOO_HIGH)
			msg("Get a newer version of dump!\n");
		msg("The ENTIRE dump is aborted.\n");
		exit(X_STARTUP);
	}
	if (fs->super->s_rev_level > DUMP_CURRENT_REV) {
		com_err(disk, retval, "while opening filesystem");
		msg("Get a newer version of dump!\n");
		msg("The ENTIRE dump is aborted.\n");
		exit(X_STARTUP);
	}
	/* if no user label specified, use ext2 filesystem label if available */
	if (spcl.c_label[0] == '\0') {
		const char *lbl;
		if ( (lbl = get_device_label(disk)) != NULL) {
			strncpy(spcl.c_label, lbl, LBLSIZE);
			spcl.c_label[LBLSIZE-1] = '\0';
		}
		else
			strcpy(spcl.c_label, "none");   /* safe strcpy. */
	}
	sync();
	dev_bsize = DEV_BSIZE;
	dev_bshift = ffs(dev_bsize) - 1;
	if (dev_bsize != (1 << dev_bshift))
		quit("dev_bsize (%d) is not a power of 2", dev_bsize);
	tp_bshift = ffs(TP_BSIZE) - 1;
	if (TP_BSIZE != (1 << tp_bshift))
		quit("TP_BSIZE (%d) is not a power of 2", TP_BSIZE);
	maxino = fs->super->s_inodes_count + 1;
	spcl.c_flags |= DR_NEWINODEFMT;
#else	/* __linux __*/
	if ((diskfd = open(disk, O_RDONLY)) < 0) {
		msg("Cannot open %s\n", disk);
		msg("The ENTIRE dump is aborted.\n");
		exit(X_STARTUP);
	}
	sync();
	sblock = (struct fs *)sblock_buf;
	bread(SBOFF, (char *) sblock, SBSIZE);
	if (sblock->fs_magic != FS_MAGIC)
		quit("bad sblock magic number\n");
	dev_bsize = sblock->fs_fsize / fsbtodb(sblock, 1);
	dev_bshift = ffs(dev_bsize) - 1;
	if (dev_bsize != (1 << dev_bshift))
		quit("dev_bsize (%d) is not a power of 2", dev_bsize);
	tp_bshift = ffs(TP_BSIZE) - 1;
	if (TP_BSIZE != (1 << tp_bshift))
		quit("TP_BSIZE (%d) is not a power of 2", TP_BSIZE);
#ifdef FS_44INODEFMT
	if (sblock->fs_inodefmt >= FS_44INODEFMT)
		spcl.c_flags |= DR_NEWINODEFMT;
#endif
	maxino = sblock->fs_ipg * sblock->fs_ncg;
#endif	/* __linux__ */
	mapsize = roundup(howmany(maxino, NBBY), TP_BSIZE);
	usedinomap = (char *)calloc((unsigned) mapsize, sizeof(char));
	dumpdirmap = (char *)calloc((unsigned) mapsize, sizeof(char));
	dumpinomap = (char *)calloc((unsigned) mapsize, sizeof(char));
	metainomap = (char *)calloc((unsigned) mapsize, sizeof(char));
	if (usedinomap == NULL || dumpdirmap == NULL || 
	    dumpinomap == NULL || metainomap == NULL)
		quit("out of memory allocating inode maps\n");
	tapesize = 2 * (howmany(mapsize * sizeof(char), TP_BSIZE) + 1);

	nonodump = spcl.c_level < honorlevel;

	if (!sizest) {
		msg("Label: %s\n", spcl.c_label);
		
		msg("Writing %d Kilobyte records\n", ntrec);

		if (compressed) {
			if (zipflag == COMPRESS_LZO) 
				msg("Compressing output (lzo)\n");
			else
				msg("Compressing output at compression level %d (%s)\n", 
					compressed, zipflag == COMPRESS_ZLIB ? "zlib" : "bzlib");
		}
	}

#if defined(SIGINFO)
	(void)signal(SIGINFO, statussig);
#endif

	if (!sizest)
		msg("mapping (Pass I) [regular files]\n");
#ifdef	__linux__
	if (directory[0] == 0)
		anydirskipped = mapfiles(maxino, &tapesize);
	else {
		if (LSTAT(pathname, &statbuf) == -1) {
			msg("File cannot be accessed (%s).\n", pathname);
			msg("The ENTIRE dump is aborted.\n");
			exit(X_STARTUP);
		}
		filedev = statbuf.st_dev;
		if (!(statbuf.st_mode & S_IFDIR))	/* is a file */
			anydirskipped = maponefile(maxino, &tapesize, 
						   directory);
		else
			anydirskipped = mapfilesfromdir(maxino, &tapesize, 
							directory);
	}
	while (argc--) {
		int anydirskipped2;
		char *p = *argv;
		/* check if file is available */
		if (LSTAT(p, &statbuf) == -1) {
			msg("File cannot be accessed (%s).\n", p);
			msg("The ENTIRE dump is aborted.\n");
			exit(X_STARTUP);
		}
		/* check if file is on same unix partiton as the first 
		 * argument */
		if (statbuf.st_dev != filedev) {
			msg("Files are not on same file system (%s).\n", p);
			msg("The ENTIRE dump is aborted.\n");
			exit(X_STARTUP);
		}
		/* check if file is a directory */
		if (!(statbuf.st_mode & S_IFDIR))
			anydirskipped2 = maponefile(maxino, &tapesize, 
						    p+strlen(dt->mnt_dir));
		else
			/* read directory inodes.
			 * NOTE: nested directories are not recognized 
			 * so inodes may be umped twice!
			 */
			anydirskipped2 = mapfilesfromdir(maxino, &tapesize, 
							 p+strlen(dt->mnt_dir));
		if (!anydirskipped)
			anydirskipped = anydirskipped2;
		argv++;
	}		
#else
	anydirskipped = mapfiles(maxino, &tapesize);
#endif

	if (!sizest)
		msg("mapping (Pass II) [directories]\n");
	while (anydirskipped) {
		anydirskipped = mapdirs(maxino, &tapesize);
	}

	if (sizest) {
		printf("%.0f\n", ((double)tapesize + 1 + ntrec) * TP_BSIZE);
		exit(X_FINOK);
	} /* stop here for size estimate */

	if (pipeout || unlimited) {
		tapesize += 1 + ntrec;	/* 1 map header + trailer blocks */
		msg("estimated %ld blocks.\n", tapesize);
	} else {
		double fetapes;

		if (blocksperfiles) {
			long tapesize_left;

			tapesize_left = tapesize;
			fetapes = 0;
			for (i = 1; i < *blocksperfiles && tapesize_left; i++) {
				fetapes++;
				if (tapesize_left > blocksperfiles[i])
					tapesize_left -= blocksperfiles[i];
				else
					tapesize_left = 0;
			}
			if (tapesize_left)
				fetapes += (double)tapesize_left / blocksperfiles[*blocksperfiles];
		} else if (cartridge) {
			/* Estimate number of tapes, assuming streaming stops at
			   the end of each block written, and not in mid-block.
			   Assume no erroneous blocks; this can be compensated
			   for with an artificially low tape size. */
			fetapes =
			(	  (double) tapesize	/* blocks */
				* TP_BSIZE	/* bytes/block */
				* (1.0/density)	/* 0.1" / byte " */
			  +
				  (double) tapesize	/* blocks */
				* (1.0/ntrec)	/* streaming-stops per block */
				* 15.48		/* 0.1" / streaming-stop " */
			) * (1.0 / tsize );	/* tape / 0.1" " */
		} else {
			/* Estimate number of tapes, for old fashioned 9-track
			   tape */
			int tenthsperirg = (density == 625) ? 3 : 7;
			fetapes =
			(	  (double) tapesize	/* blocks */
				* TP_BSIZE	/* bytes / block */
				* (1.0/density)	/* 0.1" / byte " */
			  +
				  (double) tapesize	/* blocks */
				* (1.0/ntrec)	/* IRG's / block */
				* tenthsperirg	/* 0.1" / IRG " */
			) * (1.0 / tsize );	/* tape / 0.1" " */
		}
		etapes = fetapes;		/* truncating assignment */
		etapes++;
		/* count the dumped inodes map on each additional tape */
		tapesize += (etapes - 1) *
			(howmany(mapsize * sizeof(char), TP_BSIZE) + 1);
		tapesize += etapes + ntrec;	/* headers + trailer blks */
		msg("estimated %ld blocks on %3.2f tape(s).\n",
		    tapesize, fetapes);
	}

#ifdef USE_QFA
	if (tapepos) {
		msg("writing QFA positions to %s\n", gTapeposfile);
		if ((gTapeposfd = open(gTapeposfile,
				       O_WRONLY|O_CREAT|O_TRUNC,
				       S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP
				       | S_IROTH | S_IWOTH)) < 0)
			quit("can't open tapeposfile\n");
		/* print QFA-file header */
		snprintf(gTps, sizeof(gTps), "%s\n%s\n%ld\n\n", QFA_MAGIC, QFA_VERSION, (unsigned long)spcl.c_date);
		gTps[sizeof(gTps) - 1] = '\0';
		if (write(gTapeposfd, gTps, strlen(gTps)) != (ssize_t)strlen(gTps))
			quit("can't write tapeposfile\n");
		sprintf(gTps, "ino\ttapeno\ttapepos\n");
		if (write(gTapeposfd, gTps, strlen(gTps)) != (ssize_t)strlen(gTps))
			quit("can't write tapeposfile\n");
	}
#endif /* USE_QFA */

	/*
	 * Allocate tape buffer.
	 */
	if (!alloctape())
		quit(
	"can't allocate tape buffers - try a smaller blocking factor.\n");

	startnewtape(1);
	tstart_writing = time(NULL);
	dumpmap(usedinomap, TS_CLRI, maxino - 1);

	msg("dumping (Pass III) [directories]\n");
	dirty = 0;		/* XXX just to get gcc to shut up */
	for (map = dumpdirmap, ino = 1; ino < maxino; ino++) {
		if (((ino - 1) % NBBY) == 0)	/* map is offset by 1 */
			dirty = *map++;
		else
			dirty >>= 1;
		if ((dirty & 1) == 0)
			continue;
		/*
		 * Skip directory inodes deleted and maybe reallocated
		 */
		dp = getino(ino);
		if ((dp->di_mode & IFMT) != IFDIR)
			continue;
#ifdef	__linux__
		/*
		 * Skip directory inodes deleted and not yes reallocated...
		 */
		if (dp->di_nlink == 0 || dp->di_dtime != 0)
			continue;
		if (vflag)
			msg("dumping directory inode %lu\n", ino);
		(void)dumpdirino(dp, ino);
#else
		(void)dumpino(dp, ino);
#endif
	}

	msg("dumping (Pass IV) [regular files]\n");
	for (map = dumpinomap, ino = 1; ino < maxino; ino++) {
		if (((ino - 1) % NBBY) == 0)	/* map is offset by 1 */
			dirty = *map++;
		else
			dirty >>= 1;
		if ((dirty & 1) == 0)
			continue;
		/*
		 * Skip inodes deleted and reallocated as directories.
		 */
		dp = getino(ino);
		if ((dp->di_mode & IFMT) == IFDIR)
			continue;
#ifdef __linux__
		/*
		 * No need to check here for deleted and not yet reallocated
		 * inodes since this is done in dumpino().
		 */
#endif
		if (vflag) {
			if (mflag && TSTINO(ino, metainomap))
				msg("dumping regular inode %lu (meta only)\n", ino);
			else
				msg("dumping regular inode %lu\n", ino);
		}
		(void)dumpino(dp, ino, mflag && TSTINO(ino, metainomap));
	}

	tend_writing = time(NULL);
	spcl.c_type = TS_END;

	if (Afile >= 0) {
		volinfo[1] = ROOTINO;
		memcpy(spcl.c_inos, volinfo, TP_NINOS * sizeof(dump_ino_t));
		spcl.c_flags |= DR_INODEINFO;
	}

	/*
	 * Finish off the current tape record with trailer blocks, to ensure
	 * at least the data in the last partial record makes it to tape.
	 * Also make sure we write at least 1 trailer block.
	 */
	for (i = ntrec - (spcl.c_tapea % ntrec); i; --i)
		writeheader(maxino - 1);

	tnow = trewind();

	if (pipeout || fifoout)
		msg("%ld blocks (%.2fMB)\n", spcl.c_tapea,
			((double)spcl.c_tapea * TP_BSIZE / 1048576));
	else
		msg("%ld blocks (%.2fMB) on %d volume(s)\n",
		    spcl.c_tapea, 
		    ((double)spcl.c_tapea * TP_BSIZE / 1048576),
		    spcl.c_volume);

	/* report dump performance, avoid division by zero */
	if (tend_writing - tstart_writing == 0)
		msg("finished in less than a second\n");
	else
		msg("finished in %d seconds, throughput %d kBytes/sec\n",
		    tend_writing - tstart_writing,
		    spcl.c_tapea / (tend_writing - tstart_writing));

	putdumptime();
	msg("Date of this level %s dump: %s", level,
		spcl.c_date == 0 ? "the epoch\n" : ctime4(&spcl.c_date));
	msg("Date this dump completed:  %s", ctime(&tnow));

	msg("Average transfer rate: %ld kB/s\n", xferrate / tapeno);
	if (compressed) {
		long tapekb = bytes_written / 1024;
		double rate = .0005 + (double) spcl.c_tapea / tapekb;
		msg("Wrote %ldkB uncompressed, %ldkB compressed, %1.3f:1\n",
			spcl.c_tapea, tapekb, rate);
	}

	if (Afile >= 0)
		msg("Archiving dump to %s\n", Apath);

	broadcast("DUMP IS DONE!\7\7\n");
	msg("DUMP IS DONE\n");
	Exit(X_FINOK);
	/* NOTREACHED */
	return 0;	/* gcc - shut up */
}

static void
usage(void)
{
	char white[MAXPATHLEN];
	const char *ext2ver, *ext2date;

	memset(white, ' ', MAXPATHLEN);
	white[MIN(strlen(__progname), MAXPATHLEN - 1)] = '\0';

#ifdef __linux__
	ext2fs_get_library_version(&ext2ver, &ext2date);
	fprintf(stderr, "%s %s (using libext2fs %s of %s)\n",
		__progname, _DUMP_VERSION, ext2ver, ext2date);
#else
	fprintf(stderr, "%s %s\n", __progname, _DUMP_VERSION);
#endif
	fprintf(stderr,
		"usage:\t%s [-level#] [-ac"
#ifdef KERBEROS
		"k"
#endif
		"mMnqSuv"
		"] [-A file] [-B records] [-b blocksize]\n"
		"\t%s [-d density] [-D file] [-e inode#,inode#,...] [-E file]\n"
		"\t%s [-f file] [-h level] [-I nr errors] "
#ifdef HAVE_BZLIB
		"[-j zlevel] "
#endif
#ifdef USE_QFA
		"[-Q file]\n"
#endif
		"\t%s [-s feet] "
		"[-T date] "
#ifdef HAVE_LZO
		"[-y] "
#endif
#ifdef HAVE_ZLIB
		"[-z zlevel] "
#endif
		"filesystem\n"
		"\t%s [-W | -w]\n", 
		__progname, white, white, white, __progname);
	exit(X_STARTUP);
}

/*
 * Pick up a numeric argument.  It must be nonnegative and in the given
 * range (except that a vmax of 0 means unlimited).
 */
static long
numarg(const char *meaning, long vmin, long vmax)
{
	char *p;
	long val;

	val = strtol(optarg, &p, 10);
	if (*p)
		errx(X_STARTUP, "illegal %s -- %s", meaning, optarg);
	if (val < vmin || (vmax && val > vmax))
		errx(X_STARTUP, "%s must be between %ld and %ld", meaning, vmin, vmax);
	return (val);
}

/*
 * The same as numarg, just that it expects a comma separated list of numbers
 * and returns an array of longs with the first element containing the number
 * values in that array.
 */
static long *
numlistarg(const char *meaning, long vmin, long vmax)
{
	char *p;
	long *vals,*curval;
	long valnum;

	p = optarg;
	vals = NULL;
	valnum = 0;
	do {
		valnum++;
		if ( !(vals = realloc(vals, (valnum + 1) * sizeof(*vals))) )
			errx(X_STARTUP, "allocating memory failed");
		curval = vals + valnum;
		*curval = strtol(p, &p, 10);
		if (*p && *p!=',')
			errx(X_STARTUP, "illegal %s -- %s", meaning, optarg);
		if (*curval < vmin || (vmax && *curval > vmax))
			errx(X_STARTUP, "%s must be between %ld and %ld", meaning, vmin, vmax);
		*vals = valnum;
		if (*p) p++;
	} while(*p);
	return (vals);
}

void
sig(int signo)
{
	switch(signo) {
	case SIGALRM:
	case SIGBUS:
	case SIGFPE:
	case SIGHUP:
	case SIGTERM:
	case SIGTRAP:
		if (pipeout || fifoout)
			quit("Signal on pipe: cannot recover\n");
		msg("Rewriting attempted as response to unknown signal: %d.\n", signo);
		(void)fflush(stderr);
		(void)fflush(stdout);
		close_rewind();
		exit(X_REWRITE);
		/* NOTREACHED */
	case SIGSEGV:
		msg("SIGSEGV: ABORTING!\n");
		(void)signal(SIGSEGV, SIG_DFL);
		(void)kill(0, SIGSEGV);
		/* NOTREACHED */
	}
}

const char *
rawname(const char *cp)
{
#ifdef	__linux__
	return cp;
#else	/* __linux__ */
	static char rawbuf[MAXPATHLEN];
	char *dp = strrchr(cp, '/');

	if (dp == NULL)
		return (NULL);
	(void)strncpy(rawbuf, cp, min(dp-cp, MAXPATHLEN - 1));
	rawbuf[min(dp-cp, MAXPATHLEN-1)] = '\0';
	(void)strncat(rawbuf, "/r", MAXPATHLEN - 1 - strlen(rawbuf));
	(void)strncat(rawbuf, dp + 1, MAXPATHLEN - 1 - strlen(rawbuf));
	return (rawbuf);
#endif	/* __linux__ */
}

/*
 * obsolete --
 *	Change set of key letters and ordered arguments into something
 *	getopt(3) will like.
 */
static void
obsolete(int *argcp, char **argvp[])
{
	int argc, flags;
	char *ap, **argv, *flagsp=NULL, **nargv, *p=NULL;

	/* Setup. */
	argv = *argvp;
	argc = *argcp;

	/* Return if no arguments or first argument has leading dash. */
	ap = argv[1];
	if (argc == 1 || *ap == '-')
		return;

	/* Allocate space for new arguments. */
	if ((*argvp = nargv = malloc((argc + 1) * sizeof(char *))) == NULL ||
	    (p = flagsp = malloc(strlen(ap) + 2)) == NULL)
		err(X_STARTUP, "malloc new args");

	*nargv++ = *argv;
	argv += 2;

	for (flags = 0; *ap; ++ap) {
		switch (*ap) {
		case 'A':
		case 'B':
		case 'b':
		case 'd':
		case 'D':
		case 'e':
		case 'E':
		case 'f':
		case 'F':
		case 'h':
		case 'L':
		case 'Q':
		case 's':
		case 'T':
			if (*argv == NULL) {
				warnx("option requires an argument -- %c", *ap);
				usage();
			}
			if ((nargv[0] = malloc(strlen(*argv) + 2 + 1)) == NULL)
				err(X_STARTUP, "malloc arg");
			nargv[0][0] = '-';
			nargv[0][1] = *ap;
			(void)strcpy(&nargv[0][2], *argv);
			++argv;
			++nargv;
			break;
		default:
			if (!flags) {
				*p++ = '-';
				flags = 1;
			}
			*p++ = *ap;
			break;
		}
	}

	/* Terminate flags. */
	if (flags) {
		*p = '\0';
		*nargv++ = flagsp;
	}

	/* Copy remaining arguments. */
	while ((*nargv++ = *argv++));

	/* Update argument count. */
	*argcp = nargv - *argvp - 1;
}

/*
 * This tests whether an inode is in the exclude bitmap
 */
int
exclude_ino(dump_ino_t ino)
{
	/* if the inode exclude bitmap exists and covers given inode */
	if (iexclude_bitmap && iexclude_bitmap_bytes > ino / 8) {
		/* then check this inode against it */
		int idx = iexclude_bitmap[ino / 8];
		if (idx & (1 << (ino % 8)))
			return 1;
	}
	return 0;
}

/*
 * This adds an inode to the exclusion bitmap if it isn't already there
 */
void
do_exclude_ino(dump_ino_t ino, const char *reason)
{
	if (exclude_ino(ino))
		return;

	if (vflag) {
		if (reason)
			msg("Excluding inode %u (%s) from dump\n", ino, reason);
		else
			msg("Excluding inode %u from dump\n", ino);
	}

	/* check for enough mem; initialize */
	if ((ino/8 + 1) > iexclude_bitmap_bytes) {
		if (iexclude_bitmap_bytes == 0) {
			unsigned int j;
			iexclude_bitmap_bytes = 2 * (ino/8 + 1);
			iexclude_bitmap = (char*) malloc(iexclude_bitmap_bytes);
			if (iexclude_bitmap == NULL) {
				msg("allocating memory failed\n");
				exit(X_STARTUP);
			}
			for (j = 0; j < iexclude_bitmap_bytes; j++)
				iexclude_bitmap[j] = 0;
		}
		else {
			unsigned int oldsize = iexclude_bitmap_bytes;
			iexclude_bitmap_bytes *= 
				(ino / 8 + 1) / iexclude_bitmap_bytes + 1;
			iexclude_bitmap = (char*) realloc(iexclude_bitmap, 
				iexclude_bitmap_bytes);
			if (iexclude_bitmap == NULL) {
				msg("allocating memory failed\n");
				exit(X_STARTUP);
			}
			for( ; oldsize < iexclude_bitmap_bytes; oldsize++)
				iexclude_bitmap[oldsize] = 0;
		}
	}
		
	iexclude_bitmap[ino / 8] |= 1 << (ino % 8);
}

static void
do_exclude_ino_str(char * ino) {
	char *r;
	unsigned long inod;

	inod = strtoul(ino, &r, 10);
	if (( *r != '\0' && !isspace(*r) ) || inod <= ROOTINO) {
		msg("Invalid inode argument %s\n", ino);
		msg("The ENTIRE dump is aborted.\n");
		exit(X_STARTUP);
	}
	do_exclude_ino(inod, NULL);
}

/*
 * This reads a file containing one inode number per line and exclude them all
 */
static void 
do_exclude_from_file(char *file) {
	FILE *f;
	char *p, fname[MAXPATHLEN];
	

	if (!( f = fopen(file, "r")) ) {
		msg("Cannot open file for reading: %s\n", file);
		msg("The ENTIRE dump is aborted.\n");
		exit(X_STARTUP);
	}
	while (( p = fgets(fname, MAXPATHLEN, f))) {
		if ( *p && *(p + strlen(p) - 1) == '\n' ) /* possible null string */
			*(p + strlen(p) - 1) = '\0';
		if ( !*p ) /* skip empty lines */
			continue;
		do_exclude_ino_str(p);
	}
	fclose(f);
}

static void incompat_flags(int cond, char flag1, char flag2) {
	if (cond) {
	        msg("You cannot use the %c and %c flags together.\n", 
		    flag1, flag2);
		msg("The ENTIRE dump is aborted.\n");
		exit(X_STARTUP);
	}
}
