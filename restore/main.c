/*
 *	Ported to Linux's Second Extended File System as part of the
 *	dump and restore backup suit
 *	Remy Card <card@Linux.EU.Org>, 1994-1997
 *	Stelian Pop <pop@noos.fr>, 1999-2000
 *	Stelian Pop <pop@noos.fr> - Alcôve <www.alcove.fr>, 2000
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
	"$Id: main.c,v 1.19 2001/03/23 14:40:12 stelian Exp $";
#endif /* not lint */

#include <config.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef	__linux__
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

int	bflag = 0, cvtflag = 0, dflag = 0, vflag = 0, yflag = 0;
int	hflag = 1, mflag = 1, Nflag = 0, zflag = 0;
int	uflag = 0;
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

#ifdef	__linux__
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

	/* Temp files should *not* be readable.  We set permissions later. */
	(void) umask(077);
	filesys[0] = '\0';
#ifdef	__linux__
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
#ifdef KERBEROS
#define	optlist "b:CcdD:f:hikmMNRrs:tT:uvxX:y"
#else
#define	optlist "b:CcdD:f:himMNRrs:tT:uvxX:y"
#endif
	while ((ch = getopt(argc, argv, optlist)) != -1)
		switch(ch) {
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
		case 'R':
		case 'r':
		case 't':
		case 'x':
			if (command != '\0')
				errx(1,
				    "%c and %c options are mutually exclusive",
				    ch, command);
			command = ch;
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

	if (signal(SIGINT, onintr) == SIG_IGN)
		(void) signal(SIGINT, SIG_IGN);
	if (signal(SIGTERM, onintr) == SIG_IGN)
		(void) signal(SIGTERM, SIG_IGN);
	setlinebuf(stderr);

	atexit(cleanup);

	setinput(inputdev);

	if (argc == 0 && !filelist) {
		argc = 1;
		*--argv = ".";
	}

	switch (command) {
	/*
	 * Compare contents of tape.
	 */
	case 'C': {
		struct stat stbuf;

		Vprintf(stdout, "Begin compare restore\n");
		compare_ignore_not_found = 0;
		compare_errors = 0;
		setup();
		printf("filesys = %s\n", filesys);
		if (stat(filesys, &stbuf) < 0)
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
		setdirmodes(0);
		if (dflag)
			checkrestore();
		break;
	}
	exit(0);
	/* NOTREACHED */
	return 0;	/* gcc shut up */
}

static void
usage(void)
{
#ifdef __linux__
	const char *ext2ver, *ext2date;

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
	(void)fprintf(stderr,
	  "usage:\t%s%s\n\t%s%s\n\t%s%s\n\t%s%s\n\t%s%s\n\t%s%s\n",
	  __progname, " -C [-c" kerbflag "Mvy] [-b blocksize] [-D filesystem] [-f file] [-s fileno]",
	  __progname, " -i [-ch" kerbflag "mMuvy] [-b blocksize] [-f file] [-s fileno]",
	  __progname, " -r [-c" kerbflag "Muvy] [-b blocksize] [-f file] [-s fileno] [-T directory]",
	  __progname, " -R [-c" kerbflag "Muvy] [-b blocksize] [-f file] [-s fileno] [-T directory]",
	  __progname, " -t [-ch" kerbflag "Muvy] [-b blocksize] [-f file] [-s fileno] [-X filelist] [file ...]",
	  __progname, " -x [-ch" kerbflag "mMuvy] [-b blocksize] [-f file] [-s fileno] [-X filelist] [file ...]");
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
		case 'b':
		case 'D':
		case 'f':
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
