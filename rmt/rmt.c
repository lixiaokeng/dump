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
	"$Id: rmt.c,v 1.16 2002/01/16 09:32:14 stelian Exp $";
#endif /* not linux */

/*
 * rmt
 */
#include <config.h>
#include <compatlfs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mtio.h>
#include <errno.h>
#include <fcntl.h>
#ifndef __linux__
#include <sgtty.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int	tape = -1;

char	*record;
int	maxrecsize = -1;

#define	SSIZE	64
char	device[SSIZE];
char	count[SSIZE], filemode[SSIZE], pos[SSIZE], op[SSIZE];

char	resp[BUFSIZ];

FILE	*debug;
#define	DEBUG(f)	if (debug) fprintf(debug, f)
#define	DEBUG1(f,a)	if (debug) fprintf(debug, f, a)
#define	DEBUG2(f,a1,a2)	if (debug) fprintf(debug, f, a1, a2)

/*
 * Support for Sun's extended RMT protocol
 */
#define RMTI_VERSION    -1
#define RMT_VERSION 1
 
/* Extended 'i' commands */
#define RMTI_CACHE  0
#define RMTI_NOCACHE    1
#define RMTI_RETEN  2
#define RMTI_ERASE  3
#define RMTI_EOM    4
#define RMTI_NBSF   5
 
/* Extended 's' comands */
#define MTS_TYPE    'T'
#define MTS_DSREG   'D'
#define MTS_ERREG   'E'
#define MTS_RESID   'R'
#define MTS_FILENO  'F'
#define MTS_BLKNO   'B'
#define MTS_FLAGS   'f'
#define MTS_BF      'b'

char	*checkbuf __P((char *, int));
void	 error __P((int));
void	 getstring __P((char *));

int
main(int argc, char *argv[])
{
	int rval = 0;
	char c;
	int n, i, cc;
	unsigned long block = 0;

	argc--, argv++;
	if (argc > 0) {
		debug = fopen(*argv, "w");
		if (debug == 0)
			exit(1);
		(void)setbuf(debug, (char *)0);
	}
top:
	errno = 0;
	rval = 0;
	if (read(0, &c, 1) != 1)
		exit(0);
	switch (c) {

	case 'O':
		if (tape >= 0)
			(void) close(tape);
		getstring(device);
		getstring(filemode);
		DEBUG2("rmtd: O %s %s\n", device, filemode);
		/*
		 * XXX the rmt protocol does not provide a means to
		 * specify the permission bits; allow rw for everyone,
		 * as modified by the users umask
		 */
		tape = OPEN(device, atoi(filemode), 0666);
		if (tape < 0)
			goto ioerror;
		block = 0;
		goto respond;

	case 'C':
		DEBUG1("rmtd: C  (%lu blocks)\n", block);
		getstring(device);		/* discard */
		if (close(tape) < 0)
			goto ioerror;
		tape = -1;
		block = 0;
		goto respond;

	case 'L':
		getstring(count);
		getstring(pos);
		DEBUG2("rmtd: L %s %s\n", count, pos);
		rval = LSEEK(tape, (off_t)atol(count), atoi(pos));
		if (rval < 0)
			goto ioerror;
		goto respond;

	case 'W':
		getstring(count);
		n = atoi(count);
		DEBUG2("rmtd: W %s (block = %lu)\n", count, block);
		record = checkbuf(record, n);
		for (i = 0; i < n; i += cc) {
			cc = read(0, &record[i], n - i);
			if (cc <= 0) {
				DEBUG("rmtd: premature eof\n");
				exit(2);
			}
		}
		rval = write(tape, record, n);
		if (rval < 0)
			goto ioerror;
		block += n >> 10;
		goto respond;

	case 'R':
		getstring(count);
		DEBUG2("rmtd: R %s (block %lu)\n", count, block);
		n = atoi(count);
		record = checkbuf(record, n);
		rval = read(tape, record, n);
		if (rval < 0)
			goto ioerror;
		(void)sprintf(resp, "A%d\n", rval);
		(void)write(1, resp, strlen(resp));
		(void)write(1, record, rval);
		block += n >> 10;
		goto top;

	case 'I':
		getstring(op);
		getstring(count);
		DEBUG2("rmtd: I %s %s\n", op, count);
		if (atoi(op) == RMTI_VERSION) {
			rval = RMT_VERSION;
		} else { 
		  struct mtop mtop;
		  mtop.mt_op = atoi(op);
		  mtop.mt_count = atoi(count);
		  if (ioctl(tape, MTIOCTOP, (char *)&mtop) < 0)
			goto ioerror;
		  rval = mtop.mt_count;
		}
		goto respond;

	case 'i':
	{	struct mtop mtop;
 
		getstring (op);
		getstring (count);
		DEBUG2 ("rmtd: i %s %s\n", op, count);
		switch (atoi(op)) {
#ifdef MTCACHE
			case RMTI_CACHE:
				mtop.mt_op = MTCACHE;
				break;
#endif
#ifdef MTNOCACHE
			case RMTI_NOCACHE:
				mtop.mt_op = MTNOCACHE;
				break;
#endif
#ifdef MTRETEN
			case RMTI_RETEN:
				mtop.mt_op = MTRETEN;
				break;
#endif
#ifdef MTERASE
			case RMTI_ERASE:
				mtop.mt_op = MTERASE;
				break;
#endif
#ifdef MTEOM
			case RMTI_EOM:
				mtop.mt_op = MTEOM;
				break;
#endif
#ifdef MTNBSF
			case RMTI_NBSF:
				mtop.mt_op = MTNBSF;
				break;
#endif
			default:
				errno = EINVAL;
				goto ioerror;
		}
		mtop.mt_count = atoi (count);
		if (ioctl (tape, MTIOCTOP, (char *) &mtop) < 0)
			goto ioerror;

		rval = mtop.mt_count;

		goto respond;
	}

	case 'S':		/* status */
		DEBUG("rmtd: S\n");
		{ struct mtget mtget;
		  if (ioctl(tape, MTIOCGET, (char *)&mtget) < 0)
			goto ioerror;
		  rval = sizeof (mtget);
		  (void)sprintf(resp, "A%d\n", rval);
		  (void)write(1, resp, strlen(resp));
		  (void)write(1, (char *)&mtget, sizeof (mtget));
		  goto top;
		}

	case 's':
	{	char s;
		struct mtget mtget;
 
		if (read (0, &s, 1) != 1)
			goto top;
 
		if (ioctl (tape, MTIOCGET, (char *) &mtget) < 0)
			goto ioerror;

		switch (s) {
			case MTS_TYPE:
				rval = mtget.mt_type;
				break;
			case MTS_DSREG:
				rval = mtget.mt_dsreg;
				break;
			case MTS_ERREG:
				rval = mtget.mt_erreg;
				break;
			case MTS_RESID:
				rval = mtget.mt_resid;
				break;
			case MTS_FILENO:
				rval = mtget.mt_fileno;
				break;
			case MTS_BLKNO:
				rval = mtget.mt_blkno;
				break;
			case MTS_FLAGS:
				rval = mtget.mt_gstat;
				break;
			case MTS_BF:
				rval = 0;
				break;
			default:
				errno = EINVAL;
				goto ioerror;
		}

		goto respond;
	}

        case 'V':               /* version */
                getstring(op);
                DEBUG1("rmtd: V %s\n", op);
                rval = 2;
                goto respond;

	default:
		DEBUG1("rmtd: garbage command %c\n", c);
		exit(3);
	}
respond:
	DEBUG1("rmtd: A %d\n", rval);
	(void)sprintf(resp, "A%d\n", rval);
	(void)write(1, resp, strlen(resp));
	goto top;
ioerror:
	error(errno);
	goto top;
}

void getstring(char *bp)
{
	int i;
	char *cp = bp;

	for (i = 0; i < SSIZE; i++) {
		if (read(0, cp+i, 1) != 1)
			exit(0);
		if (cp[i] == '\n')
			break;
	}
	cp[i] = '\0';
}

char *
checkbuf(char *record, int size)
{

	if (size <= maxrecsize)
		return (record);
	if (record != 0)
		free(record);
	record = malloc(size);
	if (record == 0) {
		DEBUG("rmtd: cannot allocate buffer space\n");
		exit(4);
	}
	maxrecsize = size;
	while (size > 1024 &&
	       setsockopt(0, SOL_SOCKET, SO_RCVBUF, &size, sizeof (size)) < 0)
		size -= 1024;
	return (record);
}

void
error(int num)
{

	DEBUG2("rmtd: E %d (%s)\n", num, strerror(num));
	(void)snprintf(resp, sizeof(resp), "E%d\n%s\n", num, strerror(num));
	(void)write(1, resp, strlen(resp));
}
