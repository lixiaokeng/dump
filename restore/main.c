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
	"$Id: main.c,v 1.40 2002/06/08 07:10:37 stelian Exp $";
#endif /* not lint */

#include <config.h>
#include <compatlfs.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef	__linux__
#include <sys/time.h>
#include <time.h>
#ifdef HAVE_EXT2FS_EXT2_FS_H
#include <ext2fs/ext2_fs.h>
#else
#include <linux/ext2_fs.h>
#endif
#include <bsdcompat.h>
#include <signal.h>
#include <string.h>
#else	/* __linux__ */
#include <ufs/ufs/dinode.h>
#endif	/* __linux__ */
#include <protocols/dumprestore.h>

#include <compaterr.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef	__linux__
#include <ext2fs/ext2fs.h>
#include <getopt.h>
#endif

#include "pathnames.h"
#include "restore.h"
#include "extern.h"

int	aflag = 0, bflag = 0, cvtflag = 0, dflag = 0, vflag = 0, yflag = 0;
int	hflag = 1, mflag = 1, Mflag = 0, Nflag = 0, Vflag = 0, zflag = 0;
int	uflag = 0, lflag = 0, Lflag = 0, oflag = 0;
char	*Afile = NULL;
int	dokerberos = 0;
char	command = '\0';
long	dumpnum = 1;
long	volno = 0;
long	ntrec;
char	*dumpmap;
char	*usedinomap;
dump_ino_t maxino;
time_t	dumptime;
time_t	dumpdate;
FILE 	*terminal;
char	*tmpdir;
int	compare_ignore_not_found;
int	compare_errors;
char	filesys[NAMELEN];
static const char *stdin_opt = NULL;
char	*bot_script = NULL;
dump_ino_t volinfo[TP_NINOS];

#ifdef USE_QFA
FILE	*gTapeposfp;
char	*gTapeposfile;
char	gTps[255];
long	gSeekstart;
int	tapeposflag;
int	gTapeposfd;
int	createtapeposflag;
unsigned long qfadumpdate;
long long curtapepos;
#endif /* USE_QFA */

#if defined(__linux__) || defined(sunos)
char	*__progname;
#endif

static void obsolete __P((int *, char **[]));
static void usage __P((void));
static void use_stdin __P((const char *));

int
main(int argc, char *argv[])
{
	int ch;
	dump_ino_t ino;
	char *inputdev = _PATH_DEFTAPE;
	char *symtbl = "./restoresymtable";
	char *p, name[MAXPATHLEN];
	FILE *filelist = NULL;
	char fname[MAXPATHLEN];
#ifdef DEBUG_QFA
	time_t tistart, tiend, titaken;
#endif
#ifdef USE_QFA
	tapeposflag = 0;
	createtapeposflag = 0;
#endif /* USE_QFA */

	/* Temp files should *not* be readable.  We set permissions later. */
	(void) umask(077);
	filesys[0] = '\0';
#if defined(__linux__) || defined(sunos)
	__progname = argv[0];
#endif

	if (argc < 2)
		usage();

	if ((inputdev = getenv("TAPE")) == NULL)
		inputdev = _PATH_DEFTAPE;
	if ((tmpdir = getenv("TMPDIR")) == NULL)
		tmpdir = _PATH_TMP;
	if ((tmpdir = strdup(tmpdir)) == NULL)
		err(1, "malloc tmpdir");
	for (p = tmpdir + strlen(tmpdir) - 1; p >= tmpdir && *p == '/'; p--)
		;                                                               
	obsolete(&argc, &argv);
	while ((ch = getopt(argc, argv, 
		"aA:b:CcdD:f:F:hi"
#ifdef KERBEROS
		"k"
#endif
		"lL:mMNo"
#ifdef USE_QFA
		"P:Q:"
#endif
		"Rrs:tT:uvVxX:y")) != -1)
		switch(ch) {
		case 'a':
			aflag = 1;
			break;
		case 'A':
			Afile = optarg;
			aflag = 1;
			break;
		case 'b':
			/* Change default tape blocksize. */
			bflag = 1;
			ntrec = strtol(optarg, &p, 10);
			if (*p)
				errx(1, "illegal blocksize -- %s", optarg);
			if (ntrec <= 0)
				errx(1, "block size must be greater than 0");
			break;
		case 'c':
			cvtflag = 1;
			break;
		case 'D':
			strncpy(filesys, optarg, NAMELEN);
			filesys[NAMELEN - 1] = '\0';
			break;
		case 'T':
			tmpdir = optarg;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'f':
			if( !strcmp(optarg,"-") )
				use_stdin("-f");
			inputdev = optarg;
			break;
		case 'F':
			bot_script = optarg;
			break;
		case 'h':
			hflag = 0;
			break;
#ifdef KERBEROS
		case 'k':
			dokerberos = 1;
			break;
#endif
		case 'C':
		case 'i':
#ifdef USE_QFA
		case 'P':
#endif
		case 'R':
		case 'r':
		case 't':
		case 'x':
			if (command != '\0')
				errx(1,
				    "%c and %c options are mutually exclusive",
				    ch, command);
			command = ch;
#ifdef USE_QFA
			if (ch == 'P') {
				gTapeposfile = optarg;
				createtapeposflag = 1;
			}
#endif

			break;
		case 'l':
			lflag = 1;
			break;
		case 'L':
			Lflag = strtol(optarg, &p, 10);
			if (*p)
				errx(1, "illegal limit -- %s", optarg);
			if (Lflag < 0)
				errx(1, "limit must be greater than 0");
			break;
		case 'm':
			mflag = 0;
			break;
		case 'M':
			Mflag = 1;
			break;
		case 'N':
			Nflag = 1;
			break;
		case 'o':
			oflag = 1;
			break;
#ifdef USE_QFA
		case 'Q':
			gTapeposfile = optarg;
			tapeposflag = 1;
			aflag = 1;
			break;
#endif
		case 's':
			/* Dumpnum (skip to) for multifile dump tapes. */
			dumpnum = strtol(optarg, &p, 10);
			if (*p)
				errx(1, "illegal dump number -- %s", optarg);
			if (dumpnum <= 0)
				errx(1, "dump number must be greater than 0");
			break;
		case 'u':
			uflag = 1;
			break;
		case 'v':
			vflag = 1;
			break;
		case 'V':
			Vflag = 1;
			break;
		case 'X':
			if( !strcmp(optarg,"-") ) {
				use_stdin("-X");
				filelist = stdin;
			}
			else
				if ( !(filelist = fopen(optarg,"r")) )
					errx(1, "can't open file for reading -- %s", optarg);
			break;
		case 'y':
			yflag = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (command == '\0')
		errx(1, "none of C, i, R, r, t or x options specified");

#ifdef USE_QFA
	if (!mflag && tapeposflag)
		errx(1, "m and Q options are mutually exclusive");

	if (tapeposflag && command != 'i' && command != 'x' && command != 't')
		errx(1, "Q option is not valid for %c command", command);
#endif
	
	if (Afile && command != 'i' && command != 'x' && command != 't')
		errx(1, "A option is not valid for %c command", command);

	if (signal(SIGINT, onintr) == SIG_IGN)
		(void) signal(SIGINT, SIG_IGN);
	if (signal(SIGTERM, onintr) == SIG_IGN)
		(void) signal(SIGTERM, SIG_IGN);
	setlinebuf(stderr);

	atexit(cleanup);

	if (command == 'C' && inputdev[0] != '/' && strcmp(inputdev, "-")
#ifdef RRESTORE
	    && !strchr(inputdev, ':')
#endif
	  ) {
		/* since we chdir into the directory we are comparing
		 * to, we must retain the full tape path */
		char wd[MAXPATHLEN], fullpathinput[MAXPATHLEN];
		if (!getcwd(wd, MAXPATHLEN))
			err(1, "can't get current directory");
		snprintf(fullpathinput, MAXPATHLEN, "%s/%s", wd, inputdev);
		fullpathinput[MAXPATHLEN - 1] = '\0';
		setinput(fullpathinput);
	}
	else
		setinput(inputdev);

	if (argc == 0 && !filelist) {
		argc = 1;
		*--argv = ".";
	}

#ifdef USE_QFA
	if (tapeposflag) {
		msg("reading QFA positions from %s\n", gTapeposfile);
		if ((gTapeposfp = fopen(gTapeposfile, "r")) == NULL)
			errx(1, "can't open file for reading -- %s",
				gTapeposfile);
		/* start reading header info */
		if (fgets(gTps, sizeof(gTps), gTapeposfp) == NULL)
			errx(1, "not requested format of -- %s", gTapeposfile);
		gTps[strlen(gTps) - 1] = 0;     /* delete end of line */
		if (strcmp(gTps, QFA_MAGIC) != 0)
			errx(1, "not requested format of -- %s", gTapeposfile);
		if (fgets(gTps, sizeof(gTps), gTapeposfp) == NULL)
			errx(1, "not requested format of -- %s", gTapeposfile);
		gTps[strlen(gTps) - 1] = 0;
		if (strcmp(gTps, QFA_VERSION) != 0)
			errx(1, "not requested format of -- %s", gTapeposfile);
		/* read dumpdate */
		if (fgets(gTps, sizeof(gTps), gTapeposfp) == NULL)
			errx(1, "not requested format of -- %s", gTapeposfile);
		gTps[strlen(gTps) - 1] = 0;
		qfadumpdate = atol(gTps);
		/* read empty line */
		if (fgets(gTps, sizeof(gTps), gTapeposfp) == NULL)
			errx(1, "not requested format of -- %s", gTapeposfile);
		gTps[strlen(gTps) - 1] = 0;
		/* read table header line */
		if (fgets(gTps, sizeof(gTps), gTapeposfp) == NULL)
			errx(1, "not requested format of -- %s", gTapeposfile);
		gTps[strlen(gTps) - 1] = 0;
		/* end reading header info */
		/* tape position table starts here */
		gSeekstart = ftell(gTapeposfp); /* remember for later use */
	}
#endif /* USE_QFA */

	switch (command) {
	/*
	 * Compare contents of tape.
	 */
	case 'C': {
		struct STAT stbuf;

		Vprintf(stdout, "Begin compare restore\n");
		compare_ignore_not_found = 0;
		compare_errors = 0;
		setup();
		printf("filesys = %s\n", filesys);
		if (STAT(filesys, &stbuf) < 0)
			err(1, "cannot stat directory %s", filesys);
		if (chdir(filesys) < 0)
			err(1, "cannot cd to %s", filesys);
		compare_ignore_not_found = dumptime > 0;
		initsymtable((char *)0);
		extractdirs(0);
		treescan(".", ROOTINO, nodeupdates);
		compareleaves();
		checkrestore();
		if (compare_errors) {
			printf("Some files were modified!\n");
			exit(2);
		}
		break;
	}

	/*
	 * Interactive mode.
	 */
	case 'i':
		setup();
		extractdirs(1);
		initsymtable(NULL);
		runcmdshell();
		break;
	/*
	 * Incremental restoration of a file system.
	 */
	case 'r':
		aflag = 1; 	/* in -r or -R mode, -a is default */
		setup();
		if (dumptime > 0) {
			/*
			 * This is an incremental dump tape.
			 */
			Vprintf(stdout, "Begin incremental restore\n");
			initsymtable(symtbl);
			extractdirs(1);
			removeoldleaves();
			Vprintf(stdout, "Calculate node updates.\n");
			treescan(".", ROOTINO, nodeupdates);
			findunreflinks();
			removeoldnodes();
		} else {
			/*
			 * This is a level zero dump tape.
			 */
			Vprintf(stdout, "Begin level 0 restore\n");
			initsymtable((char *)0);
			extractdirs(1);
			Vprintf(stdout, "Calculate extraction list.\n");
			treescan(".", ROOTINO, nodeupdates);
		}
		createleaves(symtbl);
		createlinks();
		setdirmodes(FORCE);
		checkrestore();
		if (dflag) {
			Vprintf(stdout, "Verify the directory structure\n");
			treescan(".", ROOTINO, verifyfile);
		}
		dumpsymtable(symtbl, (long)1);
		break;
	/*
	 * Resume an incremental file system restoration.
	 */
	case 'R':
		aflag = 1; 	/* in -r or -R mode, -a is default */
		initsymtable(symtbl);
		skipmaps();
		skipdirs();
		createleaves(symtbl);
		createlinks();
		setdirmodes(FORCE);
		checkrestore();
		dumpsymtable(symtbl, (long)1);
		break;
	
/* handle file names from either text file (-X) or the command line */
#define NEXTFILE(p) \
	p = NULL; \
	if (argc) { \
		--argc; \
		p = *argv++; \
	} \
	else if (filelist) { \
		if ((p = fgets(fname, MAXPATHLEN, filelist))) { \
			if ( *p && *(p + strlen(p) - 1) == '\n' ) /* possible null string */ \
				*(p + strlen(p) - 1) = '\0'; \
			if ( !*p ) /* skip empty lines */ \
				continue; \
			} \
	}
	
	/*
	 * List contents of tape.
	 */
	case 't':
		setup();
		extractdirs(0);
		initsymtable((char *)0);
		printvolinfo();
		for (;;) {
			NEXTFILE(p);
			if (!p)
				break;
			canon(p, name, sizeof(name));
			ino = dirlookup(name);
			if (ino == 0)
				continue;
			treescan(name, ino, listfile);
		}
		break;
	/*
	 * Batch extraction of tape contents.
	 */
	case 'x':
#ifdef DEBUG_QFA
		tistart = time(NULL);
#endif
		setup();
		extractdirs(1);
		initsymtable((char *)0);
		for (;;) {
			NEXTFILE(p);
			if (!p)
				break;
			canon(p, name, sizeof(name));
			ino = dirlookup(name);
			if (ino == 0)
				continue;
			if (mflag)
				pathcheck(name);
			treescan(name, ino, addfile);
		}
		createfiles();
		createlinks();
		setdirmodes(oflag ? FORCE : 0);
		if (dflag)
			checkrestore();
#ifdef DEBUG_QFA
		tiend = time(NULL);
		titaken = tiend - tistart;
		msg("restore took %d:%02d:%02d\n", titaken / 3600, 
			(titaken % 3600) / 60, titaken % 60);
#endif /* DEBUG_QFA */
		break;
#ifdef USE_QFA
	case 'P':
#ifdef DEBUG_QFA
		tistart = time(NULL);
#endif
		setup();
		msg("writing QFA positions to %s\n", gTapeposfile);
		if ((gTapeposfd = open(gTapeposfile, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR)) < 0)
			errx(1, "can't create tapeposfile\n");
		/* print QFA-file header */
		sprintf(gTps, "%s\n%s\n%ld\n\n", QFA_MAGIC, QFA_VERSION,(unsigned long)spcl.c_date);
		if (write(gTapeposfd, gTps, strlen(gTps)) != strlen(gTps))
			errx(1, "can't write tapeposfile\n");
		sprintf(gTps, "ino\ttapeno\ttapepos\n");
		if (write(gTapeposfd, gTps, strlen(gTps)) != strlen(gTps))
			errx(1, "can't write tapeposfile\n");

		extractdirs(1);
		initsymtable((char *)0);
		for (;;) {
			NEXTFILE(p);
			if (!p)
				break;
			canon(p, name, sizeof(name));
			ino = dirlookup(name);
			if (ino == 0)
				continue;
			if (mflag)
				pathcheck(name);
			treescan(name, ino, addfile);
		}
		createfiles();
#ifdef DEBUG_QFA
		tiend = time(NULL);
		titaken = tiend - tistart;
		msg("writing QFA positions took %d:%02d:%02d\n", titaken / 3600,
			(titaken % 3600) / 60, titaken % 60);
#endif /* DEBUG_QFA */
		break;
#endif /* USE_QFA */
	}
	exit(0);
	/* NOTREACHED */
	return 0;	/* gcc shut up */
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
	(void)fprintf(stderr, "%s %s (using libext2fs %s of %s)\n", 
		      __progname, _DUMP_VERSION, ext2ver, ext2date);
#else
	(void)fprintf(stderr, "%s %s\n", __progname, _DUMP_VERSION);
#endif

#ifdef KERBEROS
#define kerbflag "k"
#else
#define kerbflag
#endif

#ifdef USE_QFA
#define qfaflag "[-Q file] "
#else
#define qfaflag
#endif

	fprintf(stderr,
		"usage:"
		"\t%s -C [-cd" kerbflag "lMvVy] [-b blocksize] [-D filesystem] [-f file]\n"
		"\t%s    [-F script] [-L limit] [-s fileno]\n"
		"\t%s -i [-acdh" kerbflag "lmMouvVy] [-A file] [-b blocksize] [-f file]\n"
		"\t%s    [-F script] " qfaflag "[-s fileno]\n"
#ifdef USE_QFA
		"\t%s -P file [-acdh" kerbflag "lmMuvVy] [-A file] [-b blocksize]\n"
		"\t%s    [-f file] [-F script] [-s fileno] [-X filelist] [file ...]\n"
#endif
		"\t%s -r [-cd" kerbflag "lMuvVy] [-b blocksize] [-f file] [-F script]\n"
		"\t%s    [-s fileno] [-T directory]\n"
		"\t%s -R [-cd" kerbflag "lMuvVy] [-b blocksize] [-f file] [-F script]\n"
		"\t%s    [-s fileno] [-T directory]\n"
		"\t%s -t [-cdh" kerbflag "lMuvVy] [-A file] [-b blocksize] [-f file]\n"
		"\t%s    [-F script] " qfaflag "[-s fileno] [-X filelist] [file ...]\n"
		"\t%s -x [-acdh" kerbflag "lmMouvVy] [-A file] [-b blocksize] [-f file]\n"
		"\t%s    [-F script] " qfaflag "[-s fileno] [-X filelist] [file ...]\n",
		__progname, white, 
		__progname, white, 
#ifdef USE_QFA
		__progname, white, 
#endif
		__progname, white,
		__progname, white, 
		__progname, white, 
		__progname, white);
	exit(1);
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
	char *ap, **argv, *flagsp = NULL, **nargv, *p = NULL;

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
		err(1, "malloc args");

	*nargv++ = *argv;
	argv += 2, argc -= 2;

	for (flags = 0; *ap; ++ap) {
		switch (*ap) {
		case 'A':
		case 'b':
		case 'D':
		case 'f':
		case 'F':
		case 'L':
		case 'Q':
		case 's':
		case 'T':
		case 'X':
			if (*argv == NULL) {
				warnx("option requires an argument -- %c", *ap);
				usage();
			}
			if ((nargv[0] = malloc(strlen(*argv) + 2 + 1)) == NULL)
				err(1, "malloc arg");
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
 * use_stdin --
 *	reserve stdin for opt (avoid conflicts)
 */
void
use_stdin(const char *opt)
{
	if (stdin_opt)
		errx(1, "can't handle standard input for both %s and %s",
			stdin_opt, opt);
	stdin_opt = opt;
}
