/*
 *	Ported to Linux's Second Extended File System as part of the
 *	dump and restore backup suit
 *	Remy Card <card@Linux.EU.Org>, 1994-1997
 *	Stelian Pop <pop@noos.fr>, 1999-2000
 *	Stelian Pop <pop@noos.fr> - Alcôve <www.alcove.fr>, 2000
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
	"$Id: main.c,v 1.37 2001/03/20 10:02:48 stelian Exp $";
#endif /* not lint */

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

#include <sys/param.h>
#include <sys/time.h>
#ifdef __linux__
#include <linux/ext2_fs.h>
#include <ext2fs/ext2fs.h>
#include <time.h>
#include <sys/stat.h>
#include <bsdcompat.h>
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

int	notify = 0;	/* notify operator flag */
int	blockswritten = 0;	/* number of blocks written on current tape */
int	tapeno = 0;	/* current tape number */
int	density = 0;	/* density in bytes/0.1" " <- this is for hilit19 */
int	ntrec = NTREC;	/* # tape blocks in each tape record */
int	cartridge = 0;	/* Assume non-cartridge tape */
int	dokerberos = 0;	/* Use Kerberos authentication */
long	dev_bsize = 1;	/* recalculated below */
long	blocksperfile;	/* output blocks per file */
char	*host = NULL;	/* remote host (if any) */
int	sizest = 0;	/* return size estimate only */
int	compressed = 0;	/* use zlib to compress the output */
long long bytes_written = 0; /* total bytes written */
long	uncomprblks = 0;/* uncompressed blocks written */

#ifdef	__linux__
char	*__progname;
#endif

int 	maxbsize = 64*1024;     /* XXX MAXBSIZE from sys/param.h */
static long numarg __P((const char *, long, long));
static void obsolete __P((int *, char **[]));
static void usage __P((void));

dump_ino_t iexclude_list[IEXCLUDE_MAXNUM];/* the inode exclude list */
int iexclude_num = 0;			/* number of elements in the list */

int
main(int argc, char *argv[])
{
	register dump_ino_t ino;
	register int dirty;
	register struct dinode *dp;
	register struct	fstab *dt;
	register char *map;
	register int ch;
	int i, anydirskipped, bflag = 0, Tflag = 0, honorlevel = 1;
	dump_ino_t maxino;
#ifdef	__linux__
	errcode_t retval;
	char directory[MAXPATHLEN];
	char pathname[MAXPATHLEN];
#endif
	time_t tnow;
	char *diskparam;

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
	level = '0';

	if (argc < 2)
		usage();

	obsolete(&argc, &argv);

	while ((ch = getopt(argc, argv,
			    "0123456789aB:b:cd:e:f:F:h:L:"
#ifdef KERBEROS
			    "k"
#endif
			    "Mns:ST:uWw"
#ifdef HAVE_ZLIB
			    "z"
#endif
			    )) != -1)
#undef optstring
		switch (ch) {
		/* dump level */
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			level = ch;
			break;

		case 'a':		/* `auto-size', Write to EOM. */
			unlimited = 1;
			break;

		case 'B':		/* blocks per output file */
			unlimited = 0;
			blocksperfile = numarg("number of blocks per file",
			    1L, 0L);
			break;

		case 'b':		/* blocks per tape write */
			ntrec = numarg("number of blocks per write",
			    1L, 1000L);
			if (ntrec > maxbsize/1024) {
				msg("Please choose a blocksize <= %dKB\n",
					maxbsize/1024);
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
			
			                /* 04-Feb-00 ILC */
		case 'e':		/* exclude an inode */
			if (iexclude_num == IEXCLUDE_MAXNUM) {
				(void)fprintf(stderr, "Too many -e options\n");
				exit(X_STARTUP);
			}
		        iexclude_list[iexclude_num++] = numarg("inode to exclude",0L,0L);
			if (iexclude_list[iexclude_num-1] <= ROOTINO) {
				(void)fprintf(stderr, "Cannot exclude inode %ld\n", (long)iexclude_list[iexclude_num-1]);
				exit(X_STARTUP);
			}
			msg("Added %d to exclude list\n",
			    iexclude_list[iexclude_num-1]);
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

		case 'M':		/* multi-volume flag */
			Mflag = 1;
			break;

		case 'n':		/* notify operators */
			notify = 1;
			break;

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
				(void)fprintf(stderr, "bad time \"%s\"\n",
				    optarg);
				exit(X_STARTUP);
			}
			Tflag = 1;
			lastlevel = '?';
			break;

		case 'u':		/* update dumpdates */
			uflag = 1;
			break;

		case 'W':		/* what to do */
		case 'w':
			lastdump(ch);
			exit(X_FINOK);	/* do nothing else */
#ifdef HAVE_ZLIB
		case 'z':
			compressed = 1;
			break;
#endif /* HAVE_ZLIB */

		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc < 1) {
		(void)fprintf(stderr, "Must specify disk or filesystem\n");
		exit(X_STARTUP);
	}
	diskparam = *argv++;
	if (strlen(diskparam) >= MAXPATHLEN) {
		(void)fprintf(stderr, "Disk or filesystem name too long: %s\n", 
			      diskparam);
		exit(X_STARTUP);
	}
	argc--;
	if (argc >= 1) {
		(void)fprintf(stderr, "Unknown arguments to dump:");
		while (argc--)
			(void)fprintf(stderr, " %s", *argv++);
		(void)fprintf(stderr, "\n");
		exit(X_STARTUP);
	}
	if (Tflag && uflag) {
	        (void)fprintf(stderr,
		    "You cannot use the T and u flags together.\n");
		exit(X_STARTUP);
	}
	if (strcmp(tapeprefix, "-") == 0) {
		pipeout++;
		tapeprefix = "standard output";
	}

	if (blocksperfile)
		blocksperfile = blocksperfile / ntrec * ntrec; /* round down */
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

	if (strchr(tapeprefix, ':')) {
		host = tapeprefix;
		tapeprefix = strchr(host, ':');
		*tapeprefix++ = '\0';
#ifdef RDUMP
		if (index(tapeprefix, '\n')) {
		    (void)fprintf(stderr, "invalid characters in tape\n");
		    exit(X_STARTUP);
		}
		if (rmthost(host) == 0)
			exit(X_STARTUP);
#else
		(void)fprintf(stderr, "remote dump not enabled\n");
		exit(X_STARTUP);
#endif
	}
	(void)setuid(getuid()); /* rmthost() is the only reason to be setuid */

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
	set_operators();	/* /etc/group snarfed */
	getfstab();		/* /etc/fstab snarfed */

	/*
	 *      disk may end in / and this can confuse
	 *      fstabsearch.
	 */
	i = strlen(diskparam) - 1;
	if (i > 1 && diskparam[i] == '/')
		diskparam[i] = '\0';

	disk = get_device_name(diskparam);
	if (!disk) {		/* null means the disk is some form
				   of LABEL= or UID= but it was not
				   found */
		msg("Cannot find a disk having %s\n", diskparam);
		exit(X_STARTUP);
	}
	/*
	 *	disk can be either the full special file name,
	 *	the suffix of the special file name,
	 *	the special name missing the leading '/',
	 *	the file system name with or without the leading '/'.
	 */
	if ((dt = fstabsearch(disk)) != NULL) {
		disk = rawname(dt->fs_spec);
		(void)strncpy(spcl.c_dev, dt->fs_spec, NAMELEN);
		(void)strncpy(spcl.c_filesys, dt->fs_file, NAMELEN);
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
			disk = rawname(dt->fs_spec);
			(void)strncpy(spcl.c_dev, dt->fs_spec, NAMELEN);
			(void)strncpy(spcl.c_filesys, dt->fs_file, NAMELEN);
		} else {
			/*
			 * The argument was not found in the fstab
			 * assume that this is a subtree and search for it
			 */
			dt = fstabsearchdir(pathname, directory);
			if (dt != NULL) {
				char name[MAXPATHLEN];
				(void)strncpy(spcl.c_dev, dt->fs_spec, NAMELEN);
				(void)snprintf(name, sizeof(name), "%s (dir %s)",
					      dt->fs_file, directory);
				(void)strncpy(spcl.c_filesys, name, NAMELEN);
				disk = rawname(dt->fs_spec);
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
		if (level != '0') {
			(void)fprintf(stderr, "Only level 0 dumps are allowed on a subdirectory\n");
			exit(X_STARTUP);
		}
		if (uflag) {
			(void)fprintf(stderr, "You can't update the dumpdates file when dumping a subdirectory\n");
			exit(X_STARTUP);
		}
	}
	spcl.c_dev[NAMELEN-1] = '\0';
	spcl.c_filesys[NAMELEN-1] = '\0';
	(void)gethostname(spcl.c_host, NAMELEN);
	spcl.c_host[NAMELEN-1] = '\0';
	spcl.c_level = level - '0';
	spcl.c_type = TS_TAPE;
	if (!Tflag)
	        getdumptime(uflag);		/* dumpdates snarfed */

	if (spcl.c_ddate == 0 && spcl.c_level) {
		msg("WARNING: There is no inferior level dump on this filesystem\n"); 
		msg("WARNING: Assuming a level 0 dump by default\n");
		level = '0';
		spcl.c_level = 0;
	}

	if (Mflag)
		snprintf(tape, MAXPATHLEN, "%s%03d", tapeprefix, tapeno + 1);
	else
		strncpy(tape, tapeprefix, MAXPATHLEN);
	tape[MAXPATHLEN - 1] = '\0';

	if (!sizest) {

		msg("Date of this level %c dump: %s", level,
		    ctime4(&spcl.c_date));
		if (spcl.c_ddate)
	 		msg("Date of last level %c dump: %s", lastlevel,
			    ctime4(&spcl.c_ddate));
		msg("Dumping %s (%s) ", disk, spcl.c_filesys);
		if (host)
			msgtail("to %s on host %s\n", tape, host);
		else
			msgtail("to %s\n", tape);
	} /* end of size estimate */

#ifdef	__linux__
	retval = dump_fs_open(disk, &fs);
	if (retval) {
		com_err(disk, retval, "while opening filesystem");
		if (retval == EXT2_ET_REV_TOO_HIGH)
			printf ("Get a newer version of dump!\n");
		exit(X_STARTUP);
	}
	if (fs->super->s_rev_level > DUMP_CURRENT_REV) {
		com_err(disk, retval, "while opening filesystem");
		printf ("Get a newer version of dump!\n");
		exit(X_STARTUP);
	}
	if ((diskfd = open(disk, O_RDONLY)) < 0) {
		msg("Cannot open %s\n", disk);
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
	maxino = fs->super->s_inodes_count;
#if	0
	spcl.c_flags |= DR_NEWINODEFMT;
#endif
#else	/* __linux __*/
	if ((diskfd = open(disk, O_RDONLY)) < 0) {
		msg("Cannot open %s\n", disk);
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
	if (usedinomap == NULL || dumpdirmap == NULL || dumpinomap == NULL)
		quit("out of memory allocating inode maps\n");
	tapesize = 2 * (howmany(mapsize * sizeof(char), TP_BSIZE) + 1);

	nonodump = spcl.c_level < honorlevel;

	msg("Label: %s\n", spcl.c_label);

#if defined(SIGINFO)
	(void)signal(SIGINFO, statussig);
#endif

	if (!sizest)
		msg("mapping (Pass I) [regular files]\n");
#ifdef	__linux__
	if (directory[0] == 0)
		anydirskipped = mapfiles(maxino, &tapesize);
	else
		anydirskipped = mapfilesfromdir(maxino, &tapesize, directory);
#else
	anydirskipped = mapfiles(maxino, &tapesize);
#endif

	if (!sizest)
		msg("mapping (Pass II) [directories]\n");
	while (anydirskipped) {
		anydirskipped = mapdirs(maxino, &tapesize);
	}

	if (sizest) {
		printf("%.0f\n", ((double)tapesize + 11) * TP_BSIZE);
		exit(X_FINOK);
	} /* stop here for size estimate */

	if (pipeout || unlimited) {
		tapesize += 11;	/* 10 trailer blocks + 1 map header */
		msg("estimated %ld tape blocks.\n", tapesize);
	} else {
		double fetapes;

		if (blocksperfile)
			fetapes = (double) tapesize / blocksperfile;
		else if (cartridge) {
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
		tapesize += etapes + 10;	/* headers + 10 trailer blks */
		msg("estimated %ld tape blocks on %3.2f tape(s).\n",
		    tapesize, fetapes);
	}

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
		 * No need to check here for deleted and not yes reallocated inodes
		 * since this is done in dumpino().
		 */
#endif
		(void)dumpino(dp, ino);
	}

	tend_writing = time(NULL);
	spcl.c_type = TS_END;
	for (i = 0; i < ntrec; i++)
		writeheader(maxino - 1);

	tnow = trewind();

	if (pipeout)
		msg("%ld tape blocks (%.2fMB)\n", spcl.c_tapea,
			((double)spcl.c_tapea * TP_BSIZE / 1048576));
	else
		msg("%ld tape blocks (%.2fMB) on %d volume(s)\n",
		    spcl.c_tapea, 
		    ((double)spcl.c_tapea * TP_BSIZE / 1048576),
		    spcl.c_volume);

	/* report dump performance, avoid division through zero */
	if (tend_writing - tstart_writing == 0)
		msg("finished in less than a second\n");
	else
		msg("finished in %d seconds, throughput %d KBytes/sec\n",
		    tend_writing - tstart_writing,
		    spcl.c_tapea / (tend_writing - tstart_writing));

	putdumptime();
	msg("Date of this level %c dump: %s", level,
		spcl.c_date == 0 ? "the epoch\n" : ctime4(&spcl.c_date));
	msg("Date this dump completed:  %s", ctime(&tnow));

	msg("Average transfer rate: %ld KB/s\n", xferrate / tapeno);
	if (compressed) {
		long tapekb = bytes_written / 1024;
		double rate = .0005 + (double) spcl.c_tapea / tapekb;
		msg("Wrote %ldKB uncompressed, %ldKB compressed,"
			" compression ratio %1.3f\n",
			spcl.c_tapea, tapekb, rate);
	}

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
		"usage:\t%s [-0123456789ac"
#ifdef KERBEROS
		"k"
#endif
		"MnSu"
#ifdef HAVE_ZLIB
		"z"
#endif
		"] [-B records] [-b blocksize] [-d density]\n"
		"\t%s [-e inode#] [-f file] [-h level] [-s feet] [-T date] filesystem\n"
		"\t%s [-W | -w]\n", __progname, white, __progname);
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
		if (pipeout)
			quit("Signal on pipe: cannot recover\n");
		msg("Rewriting attempted as response to unknown signal.\n");
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
		case 'B':
		case 'b':
		case 'd':
		case 'e':
		case 'f':
		case 'F':
		case 'h':
		case 'L':
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
