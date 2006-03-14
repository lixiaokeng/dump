/*
 *	Ported to Linux's Second Extended File System as part of the
 *	dump and restore backup suit
 *	Remy Card <card@Linux.EU.Org>, 1994-1997
 *	Stelian Pop <stelian@popies.net>, 1999-2000
 *	Stelian Pop <stelian@popies.net> - Alc�ve <www.alcove.com>, 2000-2002
 */

/*-
 * Copyright (c) 1980, 1988, 1993
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
	"$Id: optr.c,v 1.40 2006/03/14 11:09:51 stelian Exp $";
#endif /* not lint */

#include <config.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>

#include <errno.h>
#include <mntent.h>
#include <paths.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <utmp.h>
#include <sys/stat.h>

#ifdef __linux__
#ifdef HAVE_EXT2FS_EXT2_FS_H
#include <ext2fs/ext2_fs.h>
#else
#include <linux/ext2_fs.h>
#endif
#include <ext2fs/ext2fs.h>
#include <bsdcompat.h>
#include <signal.h>
#endif

#include "dump.h"
#include "pathnames.h"
#include "bylabel.h"

static	void alarmcatch __P((int));
int	datesort __P((const void *, const void *));
static	void sendmes __P((const char *, const char *));

/* List of filesystem types that we can dump (same ext2 on-disk format) */
static char *fstypes[] = { "ext2", "ext3", "InterMezzo", NULL };

/*
 *	Query the operator; This previously-fascist piece of code
 *	no longer requires an exact response.
 *	It is intended to protect dump aborting by inquisitive
 *	people banging on the console terminal to see what is
 *	happening which might cause dump to croak, destroying
 *	a large number of hours of work.
 *
 *	Every 2 minutes we reprint the message, alerting others
 *	that dump needs attention.
 */
static	int timeout;
static	const char *attnmessage;		/* attention message */

int
query(const char *question)
{
	char	replybuffer[64];
	int	back, errcount;
	FILE	*mytty;
	time_t	firstprompt, when_answered;

	if (qflag) {
		msg("%s - forced abort\n", question);
		dumpabort(0);
		/* NOTREACHED */
	}

	firstprompt = time(NULL);

	if ((mytty = fopen(_PATH_TTY, "r")) == NULL)
		quit("fopen on %s fails: %s\n", _PATH_TTY, strerror(errno));
	attnmessage = question;
	timeout = 0;
	alarmcatch(0);
	back = -1;
	errcount = 0;
	do {
		if (fgets(replybuffer, 63, mytty) == NULL) {
			clearerr(mytty);
			if (++errcount > 30)	/* XXX	ugly */
				quit("excessive operator query failures\n");
		} else if (replybuffer[0] == 'y' || replybuffer[0] == 'Y') {
			back = 1;
		} else if (replybuffer[0] == 'n' || replybuffer[0] == 'N') {
			back = 0;
		} else {
			(void) fprintf(stderr,
			    "  DUMP: \"Yes\" or \"No\"?\n");
			(void) fprintf(stderr,
			    "  DUMP: %s: (\"yes\" or \"no\") ", question);
		}
	} while (back < 0);

	/*
	 *	Turn off the alarm, and reset the signal to trap out..
	 */
	(void) alarm(0);
	if (signal(SIGALRM, sig) == SIG_IGN)
		signal(SIGALRM, SIG_IGN);
	(void) fclose(mytty);
	when_answered = time(NULL);
	/*
	 * Adjust the base for time estimates to ignore time we spent waiting
	 * for operator input.
	 */
	if (tstart_writing != 0)
		tstart_writing += (when_answered - firstprompt);
	return(back);
}

char lastmsg[BUFSIZ];

/*
 *	Alert the console operator, and enable the alarm clock to
 *	sleep for 2 minutes in case nobody comes to satisfy dump
 */
static void
alarmcatch(UNUSED(int signo))
{
	int save_errno = errno;
	if (notify == 0) {
		if (timeout == 0)
			(void) fprintf(stderr,
			    "  DUMP: %s: (\"yes\" or \"no\") ",
			    attnmessage);
		else
			msgtail("\7\7");
	} else {
		if (timeout) {
			msgtail("\n");
			broadcast("");		/* just print last msg */
		}
		(void) fprintf(stderr,"  DUMP: %s: (\"yes\" or \"no\") ",
		    attnmessage);
	}
	signal(SIGALRM, alarmcatch);
	(void) alarm(120);
	timeout = 1;
	errno = save_errno;
}

/*
 *	Here if an inquisitive operator interrupts the dump program
 */
void
interrupt(UNUSED(int signo))
{
	msg("Interrupt received.\n");
	if (query("Do you want to abort dump?"))
		dumpabort(0);
}

/*
 *	The following variables and routines manage alerting
 *	operators to the status of dump.
 *	This works much like wall(1) does.
 */
struct	group *gp;

/*
 *	Get the names from the group entry "operator" to notify.
 */
void
set_operators(void)
{
	if (!notify)		/*not going to notify*/
		return;
	gp = getgrnam(OPGRENT);
	(void) endgrent();
	if (gp == NULL) {
		msg("No group entry for %s.\n", OPGRENT);
		notify = 0;
		return;
	}
}

struct tm *localclock;

/*
 *	We fork a child to do the actual broadcasting, so
 *	that the process control groups are not messed up
 */
void
broadcast(const char *message)
{
	time_t		clock;
	FILE	*f_utmp;
	struct	utmp	utmp;
	char	**np;
	int	pid, s;

	if (!notify || gp == NULL)
		return;

	switch (pid = fork()) {
	case -1:
		return;
	case 0:
		break;
	default:
		while (wait(&s) != pid)
			continue;
		return;
	}

	clock = time(NULL);
	localclock = localtime(&clock);

	if ((f_utmp = fopen(_PATH_UTMP, "r")) == NULL) {
		msg("Cannot open %s: %s\n", _PATH_UTMP, strerror(errno));
		return;
	}

	while (!feof(f_utmp)) {
		if (fread((char *) &utmp, sizeof (struct utmp), 1, f_utmp) != 1)
			break;
		if (utmp.ut_name[0] == 0)
			continue;
		for (np = gp->gr_mem; *np; np++) {
			if (strncmp(*np, utmp.ut_name, sizeof(utmp.ut_name)) != 0)
				continue;
			/*
			 *	Do not send messages to operators on dialups
			 */
			if (strncmp(utmp.ut_line, DIALUP, strlen(DIALUP)) == 0)
				continue;
#ifdef DEBUG
			msg("Message to %s at %s\n", *np, utmp.ut_line);
#endif
			sendmes(utmp.ut_line, message);
		}
	}
	(void) fclose(f_utmp);
	Exit(0);	/* the wait in this same routine will catch this */
	/* NOTREACHED */
}

static void
sendmes(const char *tty, const char *message)
{
	char t[MAXPATHLEN], buf[BUFSIZ];
	const char *cp;
	int lmsg = 1;
	FILE *f_tty;

	(void) strcpy(t, _PATH_DEV);
	(void) strncat(t, tty, sizeof t - strlen(_PATH_DEV) - 1);

	if ((f_tty = fopen(t, "w")) != NULL) {
		setbuf(f_tty, buf);
		(void) fprintf(f_tty,
		    "\n\
\7\7\7Message from the dump program to all operators at %d:%02d ...\r\n\n\
DUMP: NEEDS ATTENTION: ",
		    localclock->tm_hour, localclock->tm_min);
		for (cp = lastmsg; ; cp++) {
			if (*cp == '\0') {
				if (lmsg) {
					cp = message;
					if (!(cp && *cp != '\0'))
						break;
					lmsg = 0;
				} else
					break;
			}
			if (*cp == '\n')
				(void) putc('\r', f_tty);
			(void) putc(*cp, f_tty);
		}
		(void) fclose(f_tty);
	}
}

/*
 *	print out an estimate of the amount of time left to do the dump
 */

time_t	tschedule = 0;

void
timeest(void)
{
	time_t tnow = time(NULL);

	if (tnow >= tschedule) {
		char *buf = mktimeest(tnow);
		tschedule = tnow + 300;
		if (buf) {
			fprintf(stderr, "  DUMP: ");
			fwrite(buf, strlen(buf), 1, stderr);
			fflush(stderr);
		}
	}
}

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

	(void) fprintf(stderr,"  DUMP: ");
#ifdef TDEBUG
	(void) fprintf(stderr, "pid=%d ", getpid());
#endif
#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void) fflush(stdout);
	(void) fflush(stderr);
#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void) vsnprintf(lastmsg, sizeof(lastmsg), fmt, ap);
	va_end(ap);
}

void
#ifdef __STDC__
msgtail(const char *fmt, ...)
#else
msgtail(fmt, va_alist)
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
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);
}

void
#ifdef __STDC__
quit(const char *fmt, ...)
#else
quit(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list ap;

	(void) fprintf(stderr,"  DUMP: ");
#ifdef TDEBUG
	(void) fprintf(stderr, "pid=%d ", getpid());
#endif
#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void) fflush(stdout);
	(void) fflush(stderr);
	dumpabort(0);
}

/*
 *	Tell the operator what has to be done;
 *	we don't actually do it
 */

struct	pfstab {
	struct	pfstab *pf_next;
	struct  dumpdates *pf_dd;
	struct	mntent *pf_mntent;
};

static	struct pfstab *table;

static struct mntent *
allocfsent(struct mntent *fs)
{
	struct mntent *new;
	const char *disk;
	struct stat buf, tabbuf;
	struct pfstab *tabpf;
	struct mntent *tabfs;

	new = (struct mntent *)malloc(sizeof (*fs));
	if (new == NULL)
		quit("%s\n", strerror(errno));

	/* Translade UUID=, LABEL= ... */
	disk = get_device_name(fs->mnt_fsname);
	if (disk == NULL)
		disk = strdup(fs->mnt_fsname);

	/* Discard non block devices */
	if (stat(disk, &buf) != 0 || !S_ISBLK(buf.st_mode)) {
		free(new);
		return NULL;
	}

	/* Discard same major/minor devices */
	for (tabpf = table; tabpf != NULL; tabpf = tabpf->pf_next) {
		tabfs = tabpf->pf_mntent;
		if (stat(tabfs->mnt_fsname, &tabbuf) != 0)
			/* should not happen */
			quit("Cannot access %s\n", tabfs->mnt_fsname);
		if (tabbuf.st_rdev == buf.st_rdev) {
			free(new);
			/* Copy passno and freq from /etc/fstab because 
			 * /etc/mtab does always have them as 0 0 */
			if (!tabfs->mnt_passno)
				tabfs->mnt_passno = fs->mnt_passno;
			if (!tabfs->mnt_freq)
				tabfs->mnt_freq = fs->mnt_freq;
			if (tabfs->mnt_freq > 3659)
				quit("Dump frequency in fstab/mtab for %s is too big: %d > 3659\n",
		     		     tabfs->mnt_fsname, tabfs->mnt_freq);
			return NULL;
		}
	}
		
	if (strlen(fs->mnt_dir) > 1 && fs->mnt_dir[strlen(fs->mnt_dir) - 1] == '/')
		fs->mnt_dir[strlen(fs->mnt_dir) - 1] = '\0';
	if ((new->mnt_dir = strdup(fs->mnt_dir)) == NULL ||
	    (new->mnt_type = strdup(fs->mnt_type)) == NULL ||
	    (new->mnt_opts = strdup(fs->mnt_opts)) == NULL ||
	    (new->mnt_fsname = strdup(disk)) == NULL)
		quit("%s\n", strerror(errno));
	new->mnt_passno = fs->mnt_passno;
	new->mnt_freq = fs->mnt_freq;
	if (new->mnt_freq > 3659)
		quit("Dump frequency in fstab/mtab for %s %s is too big: %d > 3659\n",
		     new->mnt_dir, new->mnt_fsname, new->mnt_freq);
	return (new);
}

void
getfstab(void)
{
	struct mntent *fs;
	struct pfstab *pf;
	struct pfstab *pfold = NULL;
	FILE *mntfp;
	char *mnttables[] = { _PATH_MOUNTED, _PATH_MNTTAB, 0 };
	int i;

	for (i = 0; mnttables[i]; i++) {
		mntfp = setmntent(mnttables[i], "r");
		if (mntfp == NULL) {
			msg("Can't open %s for dump table information: %s\n",
			    mnttables[i], strerror(errno));
			continue;
		}
		while ((fs = getmntent(mntfp)) != NULL) {
			fs = allocfsent(fs);
			if (!fs)
				continue;
			fs->mnt_passno = 0;
			if ((pf = (struct pfstab *)malloc(sizeof (*pf))) == NULL)
				quit("%s\n", strerror(errno));
			pf->pf_mntent = fs;
			pf->pf_next = NULL;
	
			/* keep table in /etc/fstab order for use with -w and -W */
			if (pfold) {
				pfold->pf_next = pf;
				pfold = pf;
			} else
				pfold = table = pf;
		}
		(void) endmntent(mntfp);
	}
}

/*
 * Search in the fstab for a file name.
 * This file name can be either the special or the path file name.
 *
 * The entries in the fstab are the BLOCK special names, not the
 * character special names.
 * The caller of fstabsearch assures that the character device
 * is dumped (that is much faster)
 *
 * The file name can omit the leading '/'.
 */
struct mntent *
fstabsearch(const char *key)
{
	struct pfstab *pf;
	struct mntent *fs;
	const char *rn;

	for (pf = table; pf != NULL; pf = pf->pf_next) {
		fs = pf->pf_mntent;
		if (strcmp(fs->mnt_dir, key) == 0 ||
		    strcmp(fs->mnt_fsname, key) == 0)
			return (fs);
		rn = rawname(fs->mnt_fsname);
		if (rn != NULL && strcmp(rn, key) == 0)
			return (fs);
		if (key[0] != '/') {
			if (*fs->mnt_fsname == '/' &&
			    strcmp(fs->mnt_fsname + 1, key) == 0)
				return (fs);
			if (*fs->mnt_dir == '/' &&
			    strcmp(fs->mnt_dir + 1, key) == 0)
				return (fs);
		}
	}
	return (NULL);
}

#ifdef	__linux__
struct mntent *
fstabsearchdir(const char *key, char *directory)
{
	struct pfstab *pf;
	struct mntent *fs;
	struct mntent *found_fs = NULL;
	unsigned int size = 0;
	struct stat buf;

	if (stat(key, &buf) == 0 && S_ISBLK(buf.st_mode))
		return NULL;

	for (pf = table; pf != NULL; pf = pf->pf_next) {
		fs = pf->pf_mntent;
		if (strlen(fs->mnt_dir) > size &&
		    strlen(key) > strlen(fs->mnt_dir) &&
		    strncmp(fs->mnt_dir, key, strlen(fs->mnt_dir)) == 0 &&
		    (key[strlen(fs->mnt_dir)] == '/' ||
		     fs->mnt_dir[strlen(fs->mnt_dir) - 1] == '/')) {
			found_fs = fs;
			size = strlen(fs->mnt_dir);
		}
	}
	if (found_fs != NULL) {
		/*
		 * Ok, we have found a fstab entry which matches the argument
		 * We have to split the argument name into:
		 * - a device name (from the fstab entry)
		 * - a directory name on this device
		 */
		strcpy(directory, key + size);
	}
	return(found_fs);
}
#endif

static void
print_wmsg(char arg, int dumpme, const char *dev, int level,
	   const char *mtpt, time_t ddate)
{
#ifdef FDEBUG
	printf("checking dev %s: lvl %d, mtpt %s\n", dev, level, mtpt);
#endif
	if (!dumpme && arg == 'w')
		return;

	(void) printf("%c %8s\t(%6s) Last dump: ",
		      dumpme && (arg != 'w') ? '>' : ' ',
		      dev,
		      mtpt ? mtpt : "");

	/*
	 * Check ddate > 365 to avoid issues with fs in stab but not dumpdates.
	 * Not a problem, because ddate is in seconds since the epoch anyways.
	 */
	if (level >= 0 && ddate > 365) {
		char *date, *d;

		date = (char *)ctime(&ddate);
		d = strchr(date, '\n');
		if (d) *d = '\0';
		printf("Level %d, Date %s\n", level, date);
	} else
		printf("never\n");
}

/*
 *	Tell the operator what to do
 */
void
lastdump(char arg) /* w ==> just what to do; W ==> most recent dumps */
{
	struct pfstab *pf;
	time_t tnow;

	tnow = time(NULL);
	getfstab();		/* /etc/fstab input */
	initdumptimes(0);	/* dumpdates input */
	if (ddatev == NULL && table == NULL) {
		(void) printf("No %s or %s file found\n",
			      _PATH_MNTTAB, dumpdates);
		return;
	}

	if (arg == 'w')
		(void) printf("Dump these file systems:\n");
	else
		(void) printf("Last dump(s) done (Dump '>' file systems):\n");

	/* For files in dumpdates, get the last dump level and date */
	if (ddatev != NULL) {
		struct dumpdates *dtwalk = NULL;
		int i;
		char *lastname;

		qsort((char *) ddatev, nddates, sizeof(struct dumpdates *), datesort);

		lastname = "??";
		ITITERATE(i, dtwalk) {
			struct mntent *dt;
			if (strncmp(lastname, dtwalk->dd_name,
				sizeof(dtwalk->dd_name)) == 0)
				continue;
			lastname = dtwalk->dd_name;
			if ((dt = dtwalk->dd_fstab) != NULL) {
				/* Overload fs_freq as dump level and
				 * fs_passno as date, because we can't
				 * change struct fstab format.
				 * A positive fs_freq means this
				 * filesystem needs to be dumped.
				 *
				 * UGLY HACK: values in 0-3659 range indicate
				 * original value from /etc/fstab (maximum
				 * 10 years...)
				 *
				 * Values bigger than 3659 indicate that
				 * the fs is to be dumped, and the latest
				 * dump level was x-3660
				 *
				 * Negative values indicate that the fs is
				 * not to be dumped, and the latest dump 
				 * level was -x-1
				 */
				dt->mnt_passno = dtwalk->dd_ddate;
				if (dt->mnt_freq > 0 && (dtwalk->dd_ddate <
				    tnow - (dt->mnt_freq * 86400)))
					dt->mnt_freq = 3660 + dtwalk->dd_level;
				else
					dt->mnt_freq = -dtwalk->dd_level - 1;
#ifdef FDEBUG
				printf("%s fs_freq set to %d\n", lastname,
					dt->mnt_freq);
#endif
			}
		}
	}

	/* print in /etc/fstab order only those filesystem types we can dump */
	for (pf = table; pf != NULL; pf = pf->pf_next) {
		struct mntent *dt = pf->pf_mntent;
		char **type;

		for (type = fstypes; *type != NULL; type++) {
			if (strncmp(dt->mnt_type, *type,
				    sizeof(dt->mnt_type)) == 0) {
				const char *disk = get_device_name(dt->mnt_fsname);
				print_wmsg(arg, dt->mnt_freq > 0,
					   disk ? disk : dt->mnt_fsname,
					   (dt->mnt_freq < 0 ? -dt->mnt_freq - 1 :
					    dt->mnt_freq < 3660 ? -1 :
					    dt->mnt_freq - 3660),
					   dt->mnt_dir,
					   dt->mnt_passno);
			}
		}
	}

	/* print in /etc/dumpdates order if not in /etc/fstab */
	if (ddatev != NULL) {
		struct dumpdates *dtwalk = NULL;
		char *lastname;
		int i;

		lastname = "??";
		ITITERATE(i, dtwalk) {
			if (strncmp(lastname, dtwalk->dd_name,
				sizeof(dtwalk->dd_name)) == 0 ||
			    dtwalk->dd_fstab != NULL)
				continue;
			lastname = dtwalk->dd_name;
			print_wmsg(arg, 0, dtwalk->dd_name,
				   dtwalk->dd_level, NULL, dtwalk->dd_ddate);
		}
	}
}

int
datesort(const void *a1, const void *a2)
{
	struct dumpdates *d1 = *(struct dumpdates **)a1;
	struct dumpdates *d2 = *(struct dumpdates **)a2;
	int diff;

	diff = strncmp(d1->dd_name, d2->dd_name, sizeof(d1->dd_name));
	if (diff == 0)
		return (d2->dd_ddate - d1->dd_ddate);
	return (diff);
}
