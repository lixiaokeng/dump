/*
 *	Ported to Linux's Second Extended File System as part of the
 *	dump and restore backup suit
 *	Remy Card <card@Linux.EU.Org>, 1994-1997
 *	Stelian Pop <pop@noos.fr>, 1999-2000
 *	Stelian Pop <pop@noos.fr> - Alc�ve <www.alcove.fr>, 2000
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

#ifndef lint
static const char rcsid[] =
	"$Id: itime.c,v 1.15 2001/02/22 10:57:40 stelian Exp $";
#endif /* not lint */

#include <config.h>
#include <sys/param.h>
#include <sys/time.h>
#ifdef	__linux__
#include <linux/ext2_fs.h>
#include <time.h>
#include <bsdcompat.h>
#include <sys/file.h>
#include <unistd.h>
#else
#ifdef sunos
#include <sys/vnode.h>

#include <ufs/fsdir.h>
#include <ufs/inode.h>
#include <ufs/fs.h>
#else
#include <ufs/ufs/dinode.h>
#endif
#endif

#include <protocols/dumprestore.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#ifdef __STDC__
#include <stdlib.h>
#include <string.h>
#endif

#ifdef	__linux__
#include <ext2fs/ext2fs.h>
#endif

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
		(void) flock(fileno(df), LOCK_SH);
		readdumptimes(df);
		(void) fclose(df);
	}
}

static void
readdumptimes(FILE *df)
{
	register int i;
	register struct	dumptime *dtwalk;

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
	register struct dumpdates *ddp;
	register int i;
	char *fname;

	fname = disk;
#ifdef FDEBUG
	msg("Looking for name %s in dumpdates = %s for level = %c\n",
		fname, dumpdates, level);
#endif
	spcl.c_ddate = 0;
	lastlevel = '0';

	/* If this is a level 0 dump, and we're not updating 
	   dumpdates, there's no point in trying to read
	   dumpdates.  It may not exist yet, or may not be mounted.  For
	   incrementals, we *must* read dumpdates (fail if it's not there!) */
	if ( (level == lastlevel) && !createdumpdates)
		return;
	initdumptimes(createdumpdates);
	if (ddatev == NULL)
		return;
	/*
	 *	Go find the entry with the same name for a lower increment
	 *	and older date
	 */
	ITITERATE(i, ddp) {
		if (strncmp(fname, ddp->dd_name, sizeof (ddp->dd_name)) != 0)
			continue;
		if (ddp->dd_level >= level)
			continue;
#ifdef	__linux__
		if (ddp->dd_ddate <= (time_t)spcl.c_ddate)
#else
		if (ddp->dd_ddate <= spcl.c_ddate)
#endif
			continue;
		spcl.c_ddate = ddp->dd_ddate;
		lastlevel = ddp->dd_level;
	}
}

void
putdumptime(void)
{
	FILE *df;
	register struct dumpdates *dtwalk;
	register int i;
	int fd;
	char *fname;

	if(uflag == 0)
		return;
	if ((df = fopen(dumpdates, "r+")) == NULL)
		quit("cannot rewrite %s: %s\n", dumpdates, strerror(errno));
	fd = fileno(df);
	(void) flock(fd, LOCK_EX);
	fname = disk;
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
		if (strncmp(fname, dtwalk->dd_name,
				sizeof (dtwalk->dd_name)) != 0)
			continue;
		if (dtwalk->dd_level != level)
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
	(void) strncpy(dtwalk->dd_name, fname, sizeof (dtwalk->dd_name));
	dtwalk->dd_level = level;
	dtwalk->dd_ddate = spcl.c_date;

	ITITERATE(i, dtwalk) {
		dumprecout(df, dtwalk);
	}
	if (fflush(df))
		quit("%s: %s\n", dumpdates, strerror(errno));
	if (ftruncate(fd, ftell(df)))
		quit("ftruncate (%s): %s\n", dumpdates, strerror(errno));
	(void) fclose(df);
	msg("level %c dump on %s", level,
#ifdef	__linux__
		spcl.c_date == 0 ? "the epoch\n" : ctime4(&spcl.c_date));
#else
		spcl.c_date == 0 ? "the epoch\n" : ctime(&spcl.c_date));
#endif
}

static void
dumprecout(FILE *file, struct dumpdates *what)
{

	if (fprintf(file, "%s %c %s",
		    what->dd_name,
		    what->dd_level,
		    ctime(&what->dd_ddate)) < 0)
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
	if (makedumpdate(ddatep, tbuf) < 0)
		msg("Unknown intermediate format in %s, line %d\n",
			dumpdates, recno);

#ifdef FDEBUG
	msg("getrecord: %s %c %s", ddatep->dd_name, ddatep->dd_level,
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
	if ( strlen(tok) >  MAXPATHLEN )
		return(-1);
	strcpy(ddp->dd_name, tok);

	/* eat whitespace */
	for( ; *tbuf == ' ' ; tbuf++);

	/* dump level */
	ddp->dd_level = *tbuf;
	++tbuf;

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
