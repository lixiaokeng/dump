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
	"$Id: rmt.c,v 1.28 2003/11/22 16:52:16 stelian Exp $";
#endif /* not linux */

/*
 * rmt
 */
#include <config.h>
#include <compatlfs.h>
#include <rmtflags.h>
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

static int	tape = -1;

static char	*record;
static int	maxrecsize = -1;

#define	SSIZE	64
static char	device[SSIZE];
static char	count[SSIZE], filemode[SSIZE], pos[SSIZE], op[SSIZE];

static char	resp[BUFSIZ];

static FILE	*debug;
#define	DEBUG(f)	if (debug) fprintf(debug, f)
#define	DEBUG1(f,a)	if (debug) fprintf(debug, f, a)
#define	DEBUG2(f,a1,a2)	if (debug) fprintf(debug, f, a1, a2)

/*
 * Support for Sun's extended RMT protocol
 * 	code originally written by Jörg Schilling <schilling@fokus.gmd.de>
 * 	and relicensed by his permission from GPL to BSD for use in dump.
 *
 * 	rmt_version is 0 for regular clients (Linux included)
 * 	rmt_version is 1 for extended clients (Sun especially). In this case
 * 		we support some extended commands (see below) and we remap
 * 		the ioctl commands to the UNIX "standard", as per:
 * 			ftp://ftp.fokus.gmd.de/pub/unix/star/README.mtio
 *
 * 	In order to use rmt version 1, a client must send "I-1\n0\n" 
 * 	before issuing the other I commands.
 */
static int	rmt_version = 0;
#define RMTI_VERSION	-1
#define RMT_VERSION 	1
 
/* Extended 'i' commands */
#define RMTI_CACHE	0
#define RMTI_NOCACHE	1
#define RMTI_RETEN	2
#define RMTI_ERASE	3
#define RMTI_EOM	4
#define RMTI_NBSF	5
 
/* Extended 's' comands */
#define MTS_TYPE	'T'
#define MTS_DSREG	'D'
#define MTS_ERREG	'E'
#define MTS_RESID	'R'
#define MTS_FILENO	'F'
#define MTS_BLKNO	'B'
#define MTS_FLAGS	'f'
#define MTS_BF		'b'

static char	*checkbuf __P((char *, int));
static void	 error __P((int));
static void	 getstring __P((char *));
static unsigned long swaplong __P((unsigned long inv));
#ifdef ERMT
char	*cipher __P((char *, int, int));
void	decrypt __P((void));
#endif

int
main(int argc, char *argv[])
{
	OFF_T rval = 0;
	char c;
	int n, i, cc, oflags;
	unsigned long block = 0;
	char *cp;

	int magtape = 0;

#ifdef ERMT
	if (argc > 1 && strcmp(argv[1], "-d") == 0)
		decrypt(); /* decrypt stdin to stdout, and exit() */
#endif
	/* Skip "-c /etc/rmt", which appears when rmt is used as a shell */
	if (argc > 2 && strcmp(argv[1], "-c") == 0)
		argc -= 2, argv += 2;
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
		 * Translate extended GNU syntax into its numeric platform equivalent
		 */
		oflags = rmtflags_toint(filemode);
#ifdef  O_TEXT
		/*
		 * Default to O_BINARY the client may not know that we need it.
		 */
		if ((oflags & O_TEXT) == 0)
			oflags |= O_BINARY;
#endif
		DEBUG2("rmtd: O %s %d\n", device, oflags);
		/*
		 * XXX the rmt protocol does not provide a means to
		 * specify the permission bits; allow rw for everyone,
		 * as modified by the users umask
		 */
		tape = OPEN(device, oflags, 0666);
		if (tape < 0)
			goto ioerror;
		block = 0;
		{
		struct mtget mt_stat;
		magtape = ioctl(tape, MTIOCGET, (char *)&mt_stat) == 0;
		}
		goto respond;

	case 'C':
		DEBUG1("rmtd: C  (%lu blocks)\n", block);
		getstring(device);		/* discard */
		if (close(tape) < 0)
			goto ioerror;
		tape = -1;
		block = 0;
		goto respond;

#ifdef USE_QFA
#define LSEEK_GET_TAPEPOS 	10
#define LSEEK_GO2_TAPEPOS	11
#endif

	case 'L':
		getstring(count);
		getstring(pos);
		DEBUG2("rmtd: L %s %s\n", count, pos);
		if (!magtape) { /* traditional */
			switch (atoi(pos)) {
			case SEEK_SET:
			case SEEK_CUR:
			case SEEK_END:
				rval = LSEEK(tape, (OFF_T)atoll(count), atoi(pos));
				break;
#ifdef USE_QFA
			case LSEEK_GET_TAPEPOS:
				rval = LSEEK(tape, (OFF_T)0, SEEK_CUR);
				break;
			case LSEEK_GO2_TAPEPOS:
				rval = LSEEK(tape, (OFF_T)atoll(count), SEEK_SET);
				break;
#endif /* USE_QFA */
			default:
				errno = EINVAL;
				goto ioerror;
				break;
			}
		}
		else {
			switch (atoi(pos)) {
			case SEEK_SET:
			case SEEK_CUR:
			case SEEK_END:
				rval = LSEEK(tape, (OFF_T)atoll(count), atoi(pos));
				break;
#ifdef USE_QFA
			case LSEEK_GET_TAPEPOS: /* QFA */
			case LSEEK_GO2_TAPEPOS:
				{
				struct mtop buf;
				long mtpos;

				buf.mt_op = MTSETDRVBUFFER;
				buf.mt_count = MT_ST_BOOLEANS | MT_ST_SCSI2LOGICAL;
				if (ioctl(tape, MTIOCTOP, &buf) < 0) {
					goto ioerror;
				}

				if (atoi(pos) == LSEEK_GET_TAPEPOS) { /* get tapepos */
					if (ioctl(tape, MTIOCPOS, &mtpos) < 0) {
						goto ioerror;
					}
					rval = (OFF_T)mtpos;
				} else {
					buf.mt_op = MTSEEK;
					buf.mt_count = atoi(count);
					if (ioctl(tape, MTIOCTOP, &buf) < 0) {
						goto ioerror;
					}
					rval = (OFF_T)buf.mt_count;
				}
				}
				break;
#endif /* USE_QFA */
			default:
				errno = EINVAL;
				goto ioerror;
			}
		}
		if (rval < 0)
			goto ioerror;
		goto respond;

	case 'W':
		getstring(count);
		n = atoi(count);
		if (n < 1)
			exit(2);
		DEBUG2("rmtd: W %s (block = %lu)\n", count, block);
		record = checkbuf(record, n);
		for (i = 0; i < n; i += cc) {
			cc = read(0, &record[i], n - i);
			if (cc <= 0) {
				DEBUG("rmtd: premature eof\n");
				exit(2);
			}
		}
#ifdef ERMT
		if ((cp = cipher(record, n, 1)) == NULL)
			goto ioerror;
#else
		cp = record;
#endif
		rval = write(tape, cp, n);
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
#ifdef ERMT
		if ((cp = cipher(record, rval, 0)) == NULL)
			goto ioerror;
#else
		cp = record;
#endif
		(void)sprintf(resp, "A%lld\n", (long long)rval);
		(void)write(1, resp, strlen(resp));
		(void)write(1, cp, rval);
		block += n >> 10;
		goto top;

	case 'I':
		getstring(op);
		getstring(count);
		DEBUG2("rmtd: I %s %s\n", op, count);
		if (atoi(op) == RMTI_VERSION) {
			rval = RMT_VERSION;
			rmt_version = 1;
		} 
		else { 
			struct mtop mtop;
			mtop.mt_op = -1;
			if (rmt_version) {
				/* rmt version 1, assume UNIX/Solaris/Mac OS X client */
				switch (atoi(op)) {
#ifdef  MTWEOF
					case 0:
						mtop.mt_op = MTWEOF;
						break;
#endif
#ifdef  MTFSF
					case 1:
						mtop.mt_op = MTFSF;
						break;
#endif
#ifdef  MTBSF
					case 2:
						mtop.mt_op = MTBSF;
						break;
#endif
#ifdef  MTFSR
					case 3:
						mtop.mt_op = MTFSR;
						break;
#endif
#ifdef  MTBSR
					case 4:
						mtop.mt_op = MTBSR;
						break;
#endif
#ifdef  MTREW
					case 5:
						mtop.mt_op = MTREW;
						break;
#endif
#ifdef  MTOFFL
					case 6:
						mtop.mt_op = MTOFFL;
						break;
#endif
#ifdef  MTNOP
					case 7:
						mtop.mt_op = MTNOP;
						break;
#endif
#ifdef  MTRETEN
                    case 8:
                        mtop.mt_op = MTRETEN;
                        break;
#endif
#ifdef  MTERASE
                    case 9:
                        mtop.mt_op = MTERASE;
                        break;
#endif
#ifdef  MTEOM
                    case 10:
                        mtop.mt_op = MTEOM;
                        break;
#endif
				}
				if (mtop.mt_op == -1) {
					errno = EINVAL;
					goto ioerror;
				}
			}
			else {
				/* rmt version 0, assume linux client */
				mtop.mt_op = atoi(op);
			}
			mtop.mt_count = atoi(count);
			if (ioctl(tape, MTIOCTOP, (char *)&mtop) < 0) {
				goto ioerror;
			}
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
		if (ioctl (tape, MTIOCTOP, (char *) &mtop) < 0) {
			goto ioerror;
		}

		rval = mtop.mt_count;

		goto respond;
	}

	case 'S':		/* status */
		DEBUG("rmtd: S\n");
		{ struct mtget mtget;

		  if (ioctl(tape, MTIOCGET, (char *)&mtget) < 0) {
			goto ioerror;
		  }

		  if (rmt_version) {
			rval = sizeof(mtget);
            		/* assume byte order:
            		Linux on Intel (little), Solaris on SPARC (big), Mac OS X on PPC (big)
            		thus need byte swapping from little to big
            		*/
			mtget.mt_type = swaplong(mtget.mt_type);
			mtget.mt_resid = swaplong(mtget.mt_resid);
			mtget.mt_dsreg = swaplong(mtget.mt_dsreg);
			mtget.mt_gstat = swaplong(mtget.mt_gstat);
			mtget.mt_erreg = swaplong(mtget.mt_erreg);
			mtget.mt_fileno = swaplong(mtget.mt_fileno);
			mtget.mt_blkno = swaplong(mtget.mt_blkno);
			(void)sprintf(resp, "A%lld\n", (long long)rval);
			(void)write(1, resp, strlen(resp));
			(void)write(1, (char *)&mtget, sizeof (mtget));
		  } else {
		  	rval = sizeof (mtget);
			(void)sprintf(resp, "A%lld\n", (long long)rval);
			(void)write(1, resp, strlen(resp));
			(void)write(1, (char *)&mtget, sizeof (mtget));
		  }
		  goto top;
		}

	case 's':
	{	char s;
		struct mtget mtget;
 
		DEBUG ("rmtd: s\n");

		if (read (0, &s, 1) != 1)
			goto top;
		DEBUG1 ("rmtd: s %d\n", s);
 
		if (ioctl (tape, MTIOCGET, (char *) &mtget) < 0) {
			goto ioerror;
		}

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

	case 'V':	/* version */
		getstring(op);
		DEBUG1("rmtd: V %s\n", op);
		rval = 2;
		goto respond;

	default:
		DEBUG1("rmtd: garbage command %c\n", c);
		exit(3);
	}
respond:
	DEBUG1("rmtd: A %lld\n", (long long)rval);
	(void)sprintf(resp, "A%lld\n", (long long)rval);
	(void)write(1, resp, strlen(resp));
	goto top;
ioerror:
	error(errno);
	goto top;
}

static void getstring(char *bp)
{
	int i;
	char *cp = bp;

	for (i = 0; i < SSIZE - 1; i++) {
		if (read(0, cp+i, 1) != 1)
			exit(0);
		if (cp[i] == '\n')
			break;
	}
	cp[i] = '\0';
}

static char *
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

static void
error(int num)
{

	DEBUG2("rmtd: E %d (%s)\n", num, strerror(num));
	(void)snprintf(resp, sizeof(resp), "E%d\n%s\n", num, strerror(num));
	(void)write(1, resp, strlen(resp));
}

static unsigned long
swaplong(unsigned long inv)
{
	 union lconv {
		unsigned long   ul;
		unsigned char   uc[4];
	} *inp, outv;

	inp = (union lconv *)&inv;

	outv.uc[0] = inp->uc[3];
	outv.uc[1] = inp->uc[2];
	outv.uc[2] = inp->uc[1];
	outv.uc[3] = inp->uc[0];

	return (outv.ul);
}
