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
	"$Id: rmtflags.c,v 1.2 2002/11/15 10:03:12 stelian Exp $";
#endif /* not linux */

/*
 * rmt
 */
#include <config.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <rmtflags.h>

struct openflags {
	char 	*name;
	int	value;
} openflags[] = {
	{ "O_RDONLY",	O_RDONLY },
	{ "O_WRONLY",	O_WRONLY },
	{ "O_RDWR",	O_RDWR },
#ifdef O_CREAT
	{ "O_CREAT",	O_CREAT },
#endif
#ifdef O_EXCL
	{ "O_EXCL",	O_EXCL },
#endif
#ifdef O_NOCTTY
	{ "O_NOCTTY",	O_NOCTTY },
#endif
#ifdef O_TRUNC
	{ "O_TRUNC",	O_TRUNC },
#endif
#ifdef O_APPEND
	{ "O_APPEND",	O_APPEND },
#endif
#ifdef O_NONBLOCK
	{ "O_NONBLOCK",	O_NONBLOCK },
#endif
#ifdef O_NDELAY
	{ "O_NDELAY",	O_NDELAY },
#endif
#ifdef O_SYNC
	{ "O_SYNC",	O_SYNC },
#endif
#ifdef O_FSYNC
	{ "O_FSYNC",	O_FSYNC },
#endif
#ifdef O_ASYNC
	{ "O_ASYNC",	O_ASYNC },
#endif
#ifdef O_TEXT
	{ "O_TEXT",	O_TEXT },
#endif
#ifdef O_DSYNC
	{ "O_DSYNC",	O_DSYNC },
#endif
#ifdef O_RSYNC
	{ "O_RSYNC",	O_RSYNC },
#endif
#ifdef O_PRIV
	{ "O_PRIV",	O_PRIV },
#endif
#ifdef O_LARGEFILE
	{ "O_LARGEFILE",O_LARGEFILE },
#endif
	{ NULL,		0 }
};

/* Parts of this stolen again from Jörg Schilling's star package... */
int
rmtflags_toint(char *filemode)
{
	char	*p = filemode;
	struct openflags *op;
	int	result = 0;
	int	numresult = 0;
	int	seentext = 0;

	do {
		/* skip space */
		while (*p != '\0' && *p == ' ')
			p++;
		/* get O_XXXX constant */
		if (p[0] != 'O' || p[1] != '_') {
			/* numeric syntax detected */
			numresult = atoi(filemode);
			numresult &= O_RDONLY | O_WRONLY | O_RDWR;
			while (*p != ' ' && *p != '\0')
				p++;
			while (*p != '\0' && *p == ' ')
				p++;
		}

		if (*p == '\0')
			break;

		/* translate O_XXXX constant */
		for (op = openflags; op->name; op++) {
			int slen = strlen(op->name);
			if ((strncmp(op->name, p, slen) == 0) &&
			    (p[slen] == '|' || p[slen] == ' ' ||
			     p[slen] == '\0')) {
				seentext = 1;
				result |= op->value;
				break;
			}
		}

		/* goto next constant */
		p = strchr(p, '|');
	} while (p && *p++ == '|');

	if (!seentext)
		result = numresult;
	return result;
}

char *
rmtflags_tochar(int filemode)
{
	struct openflags *op;
	char *result = (char *) malloc(4096);	/* enough space */

	switch (filemode & O_ACCMODE) {
	case O_RDONLY: 
		strcpy(result, "O_RDONLY"); 
		break;
	case O_WRONLY: 
		strcpy(result, "O_WRONLY");
		break;
	case O_RDWR: 
		strcpy(result, "O_RDWR");
		break;
	default:
		strcat(result, "ERROR");
	}
	for (op = openflags; op->name; op++) {
		if (op->value == O_RDONLY ||
		    op->value == O_WRONLY ||
		    op->value == O_RDWR)
			continue;
		if (filemode & op->value) {
			strcat(result, "|");
			strcat(result, op->name);
		}
	}
	return result;
}
