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
	"$Id: unctime.c,v 1.16 2003/03/30 15:40:37 stelian Exp $";
#endif /* not lint */

#include <config.h>
#include <sys/time.h>
#include <time.h>
#ifdef __STDC__
#include <stdlib.h>
#include <string.h>
#endif

#include <sys/param.h>
#include <stdio.h>

#ifdef __linux__
#ifdef HAVE_EXT2FS_EXT2_FS_H
#include <ext2fs/ext2_fs.h>
#else
#include <linux/ext2_fs.h>
#endif
#include <ext2fs/ext2fs.h>
#include <bsdcompat.h>
#endif

#include "dump.h"

/*
 * Convert a ctime(3) format string into a system format date.
 * Return the date thus calculated.
 *
 * Return -1 if the string is not in ctime format.
 */

/*
 * Offsets into the ctime string to various parts.
 */

#define	E_MONTH		4
#define	E_DAY		8
#define	E_HOUR		11
#define	E_MINUTE	14
#define	E_SECOND	17
#define	E_YEAR		20
#define E_TZOFFSET      25

static	int lookup __P((const char *));


time_t
unctime(const char *str)
{
	struct tm then;
	char dbuf[32];
	time_t rtime;
	int tzoffset;

	(void) strncpy(dbuf, str, sizeof(dbuf) - 1);
	dbuf[sizeof(dbuf) - 1] = '\0';
	dbuf[E_MONTH+3] = '\0';
	if ((then.tm_mon = lookup(&dbuf[E_MONTH])) < 0)
		return (-1);
	then.tm_mday = atoi(&dbuf[E_DAY]);
	then.tm_hour = atoi(&dbuf[E_HOUR]);
	then.tm_min = atoi(&dbuf[E_MINUTE]);
	then.tm_sec = atoi(&dbuf[E_SECOND]);
	then.tm_year = atoi(&dbuf[E_YEAR]) - 1900;
	then.tm_isdst = -1;
	if (strlen(str) >= E_TZOFFSET+5) {
		rtime = timegm(&then);
		/* add timezone offset */
		tzoffset = atoi(&dbuf[E_TZOFFSET]);
		rtime -= (tzoffset / 100 * 3600) + (tzoffset % 100) * 60;
	} else {
		rtime = timelocal(&then);
	}
	return(rtime);
}

static char months[] =
	"JanFebMarAprMayJunJulAugSepOctNovDec";

static int
lookup(const char *str)
{
	const char *cp, *cp2;

	for (cp = months, cp2 = str; *cp != '\0'; cp += 3)
		if (strncmp(cp, cp2, 3) == 0)
			return((cp-months) / 3);
	return(-1);
}
