/*
 *	Ported to Linux's Second Extended File System as part of the
 *	dump and restore backup suit
 *	Remy Card <card@Linux.EU.Org>, 1994-1997
 *	Stelian Pop <stelian@popies.net>, 1999-2000
 *	Stelian Pop <stelian@popies.net> - Alc�ve <www.alcove.com>, 2000-2002
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

#ifndef lint
static const char rcsid[] =
	"$Id: itime.c,v 1.28 2004/06/17 09:01:15 stelian Exp $";
#endif /* not lint */

#include <config.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#ifdef __STDC__
#include <stdlib.h>
#include <string.h>
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <time.h>
#ifdef	__linux__
#ifdef HAVE_EXT2FS_EXT2_FS_H
#include <ext2fs/ext2_fs.h>
#else
#include <linux/ext2_fs.h>
#endif
#include <ext2fs/ext2fs.h>
#include <bsdcompat.h>
#include <sys/file.h>
#include <unistd.h>
#elif defined sunos
#include <sys/vnode.h>

#include <ufs/fsdir.h>
#include <ufs/inode.h>
#include <ufs/fs.h>
#else
#include <ufs/ufs/dinode.h>
#endif

#include <protocols/dumprestore.h>

#include "dump.h"

struct	dumpdates **ddatev;
int	nddates;
int	ddates_in;
struct	dumptime *dthead;

static	void dumprecout __P((FILE *, struct dumpdates *));
static	int getrecord __P((FILE *, struct dumpdates *));
static	int makedumpdate __P((struct dumpdates *, char *));
static	void readdumptimes __P((FILE *));

void
initdumptimes(int createdumpdates)
{
	FILE *df;
	struct flock lock;

	if ((df = fopen(dumpdates, "r")) == NULL) {
		if (errno != ENOENT) {
			quit("cannot read %s: %s\n", dumpdates,
			    strerror(errno));
			/* NOTREACHED */
		}
		if (createdumpdates) {
			/*
		 	 * Dumpdates does not exist, make an empty one.
		 	 */
			msg("WARNING: no file `%s', making an empty one\n", dumpdates);
			if ((df = fopen(dumpdates, "w")) == NULL) {
				quit("cannot create %s: %s\n", dumpdates,
			    	strerror(errno));
				/* NOTREACHED */
			}
			(void) fclose(df);
			if ((df = fopen(dumpdates, "r")) == NULL) {
				quit("cannot read %s even after creating it: %s\n",
			    	dumpdates, strerror(errno));
				/* NOTREACHED */
			}
		}
		else
			msg("WARNING: no file `%s'\n", dumpdates);
	}
	if (df != NULL) {
		memset(&lock, 0, sizeof(lock));
		lock.l_type = F_RDLCK;
		if (fcntl(fileno(df), F_SETLKW, &lock) < 0)
			quit("cannot set read lock on %s: %s\n",
				dumpdates, strerror(errno));
		readdumptimes(df);
		(void) fclose(df);
	}
}

static void
readdumptimes(FILE *df)
{
	int i;
	struct	dumptime *dtwalk;

	for (;;) {
		dtwalk = (struct dumptime *)calloc(1, sizeof (struct dumptime));
		if (getrecord(df, &(dtwalk->dt_value)) < 0)
			break;
		nddates++;
		dtwalk->dt_next = dthead;
		dthead = dtwalk;
	}

	ddates_in = 1;
	/*
	 *	arrayify the list, leaving enough room for the additional
	 *	record that we may have to add to the ddate structure
	 */
	ddatev = (struct dumpdates **)
		calloc((unsigned) (nddates + 1), sizeof (struct dumpdates *));
	dtwalk = dthead;
	for (i = nddates - 1; i >= 0; i--, dtwalk = dtwalk->dt_next)
		ddatev[i] = &dtwalk->dt_value;
}

void
getdumptime(int createdumpdates)
{
	struct dumpdates *ddp;
	int i;

#ifdef FDEBUG
	msg("Looking for name %s in dumpdates = %s for level = %s\n",
		disk, dumpdates, level);
#endif
	spcl.c_ddate = 0;
	memset(&lastlevel, 0, NUM_STR_SIZE);

	/* If this is a level 0 dump, and we're not updating 
	   dumpdates, there's no point in trying to read
	   dumpdates.  It may not exist yet, or may not be mounted.  For
	   incrementals, we *must* read dumpdates (fail if it's not there!) */
	if ( (!strcmp(level, lastlevel)) && !createdumpdates)
		return;
	initdumptimes(createdumpdates);
	if (ddatev == NULL)
		return;
	/*
	 *	Go find the entry with the same name for a lower increment
	 *	and older date
	 */
	ITITERATE(i, ddp) {
		if (strncmp(disk, ddp->dd_name, sizeof (ddp->dd_name)) != 0)
			continue;
		if (ddp->dd_level >= atoi(level))
			continue;
		if (ddp->dd_ddate <= (time_t)spcl.c_ddate)
			continue;
		spcl.c_ddate = ddp->dd_ddate;
		snprintf(lastlevel, NUM_STR_SIZE, "%d", ddp->dd_level);
	}
}

void
putdumptime(void)
{
	FILE *df;
	struct dumpdates *dtwalk;
	int i;
	int fd;
	struct flock lock;

	if(uflag == 0)
		return;
	if ((df = fopen(dumpdates, "r+")) == NULL)
		quit("cannot rewrite %s: %s\n", dumpdates, strerror(errno));
	fd = fileno(df);
	memset(&lock, 0, sizeof(lock));
	lock.l_type = F_WRLCK;
	if (fcntl(fd, F_SETLKW, &lock) < 0)
		quit("cannot set write lock on %s: %s\n", dumpdates, strerror(errno));
	free((char *)ddatev);
	ddatev = 0;
	nddates = 0;
	dthead = 0;
	ddates_in = 0;
	readdumptimes(df);
	if (fseek(df, 0L, 0) < 0)
		quit("fseek: %s\n", strerror(errno));
	spcl.c_ddate = 0;
	ITITERATE(i, dtwalk) {
		if (strncmp(disk, dtwalk->dd_name,
				sizeof (dtwalk->dd_name)) != 0)
			continue;
		if (dtwalk->dd_level != atoi(level))
			continue;
		goto found;
	}
	/*
	 *	construct the new upper bound;
	 *	Enough room has been allocated.
	 */
	dtwalk = ddatev[nddates] =
		(struct dumpdates *)calloc(1, sizeof (struct dumpdates));
	nddates += 1;
  found:
	(void) strncpy(dtwalk->dd_name, disk, sizeof (dtwalk->dd_name));
	dtwalk->dd_level = atoi(level);
	dtwalk->dd_ddate = spcl.c_date;

	ITITERATE(i, dtwalk) {
		dumprecout(df, dtwalk);
	}
	if (fflush(df))
		quit("%s: %s\n", dumpdates, strerror(errno));
	if (ftruncate(fd, ftell(df)))
		quit("ftruncate (%s): %s\n", dumpdates, strerror(errno));
	(void) fclose(df);
}

static void
dumprecout(FILE *file, struct dumpdates *what)
{
	char buf[26];
	struct tm *tms;

	tms = localtime(&what->dd_ddate);
	strncpy(buf, asctime(tms), sizeof(buf));
	if (buf[24] != '\n' || buf[25] != '\0')
		quit("asctime returned an unexpected string\n");
	buf[24] = 0;
	if (fprintf(file, "%s %d %s %c%2.2d%2.2d\n",
		    what->dd_name,
		    what->dd_level,
		    buf,
		    (tms->tm_gmtoff < 0 ? '-' : '+'),
		    abs(tms->tm_gmtoff) / 3600,
		    abs(tms->tm_gmtoff) % 3600 / 60) < 0)
		quit("%s: %s\n", dumpdates, strerror(errno));
}

int	recno;

static int
getrecord(FILE *df, struct dumpdates *ddatep)
{
	char tbuf[BUFSIZ];

	recno = 0;
	if (fgets(tbuf, sizeof (tbuf), df) == NULL)
		return(-1);
	recno++;
	if (makedumpdate(ddatep, tbuf) < 0) {
		msg("Unknown format in %s, line %d\n",
			dumpdates, recno);
		return(-1);
	}

#ifdef FDEBUG
	msg("getrecord: %s %d %s", ddatep->dd_name, ddatep->dd_level,
	    ddatep->dd_ddate == 0 ? "the epoch\n" : ctime(&ddatep->dd_ddate));
#endif
	return(0);
}

static int
makedumpdate(struct dumpdates *ddp, char *tbuf)
{
	char *tok;
	/* device name */
	if ( NULL == (tok = strsep( &tbuf, " ")) )
		return(-1);
	if ( !tbuf || strlen(tok) >  MAXPATHLEN )
		return(-1);
	strcpy(ddp->dd_name, tok);

	/* eat whitespace */
	for( ; *tbuf == ' ' ; tbuf++);

	/* dump level */
	ddp->dd_level = atoi(tbuf);
	/* eat the rest of the numbers*/
	for( ; *tbuf >= '0' && *tbuf <= '9' ; tbuf++);

	/* eat whitespace */
	for( ; *tbuf == ' ' ; tbuf++);

	/* dump date */
	ddp->dd_ddate = unctime(tbuf);
	if (ddp->dd_ddate < 0)
		return(-1);
	/* fstab entry */
	ddp->dd_fstab = fstabsearch(ddp->dd_name);
	return(0);
}
