/*
 *	Ported to Linux's Second Extended File System as part of the
 *	dump and restore backup suit
 *	Remy Card <card@Linux.EU.Org>, 1994-1997
 *	Stelian Pop <stelian@popies.net>, 1999-2000
 *	Stelian Pop <stelian@popies.net> - Alcôve <www.alcove.com>, 2000-2002
 */

/*-
 * Copyright (c) 1980, 1991, 1993
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
	"$Id: tape.c,v 1.74 2003/03/30 15:40:37 stelian Exp $";
#endif /* not lint */

#include <config.h>
#include <compatlfs.h>
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <compaterr.h>
#include <system.h>
#ifdef __STDC__
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#else
int    write(), read();
#endif

#ifdef __linux__
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#endif
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/mtio.h>
#ifdef __linux__
#ifdef HAVE_EXT2FS_EXT2_FS_H
#include <ext2fs/ext2_fs.h>
#else
#include <linux/ext2_fs.h>
#endif
#include <ext2fs/ext2fs.h>
#include <sys/stat.h>
#include <bsdcompat.h>
#elif defined sunos
#include <sys/vnode.h>

#include <ufs/fs.h>
#include <ufs/inode.h>
#else
#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>
#endif	/* __linux__ */

#include <protocols/dumprestore.h>

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif /* HAVE_ZLIB */

#ifdef HAVE_BZLIB
#include <bzlib.h>
#endif /* HAVE_BZLIB */

#include "dump.h"

int	writesize;		/* size of malloc()ed buffer for tape */
long	lastspclrec = -1;	/* tape block number of last written header */
int	trecno = 0;		/* next record to write in current block */
extern	long blocksperfile;	/* number of blocks per output file */
long	blocksthisvol;		/* number of blocks on current output file */
extern	int ntrec;		/* blocking factor on tape */
extern	int cartridge;
char	*nexttape;
extern  pid_t rshpid;
long long tapea_bytes = 0;	/* bytes_written at start of current volume */
static int magtapeout;		/* output is really a tape */

static	ssize_t dump_atomic_read __P((int, void *, size_t));
static	ssize_t dump_atomic_write __P((int, const void *, size_t));
#ifdef WRITEDEBUG
static	void doslave __P((int, int, int));
#else
static	void doslave __P((int, int));
#endif
static	void enslave __P((void));
static	void flushtape __P((void));
static	void killall __P((void));
static	void rollforward __P((void));
#ifdef USE_QFA
static int GetTapePos __P((long long *));
static void MkTapeString __P((struct s_spcl *, long long));
#endif

/*
 * Concurrent dump mods (Caltech) - disk block reading and tape writing
 * are exported to several slave processes.  While one slave writes the
 * tape, the others read disk blocks; they pass control of the tape in
 * a ring via signals. The parent process traverses the filesystem and
 * sends writeheader()'s and lists of daddr's to the slaves via pipes.
 * The following structure defines the instruction packets sent to slaves.
 */
struct req {
	daddr_t dblk;
	int count;
};
int reqsiz;

struct slave_results {
	ssize_t unclen;		/* uncompressed length */
	ssize_t clen;		/* compressed length */
};

#define SLAVES 3		/* 1 slave writing, 1 reading, 1 for slack */
struct slave {
	int tapea;		/* header number at start of this chunk */
	int count;		/* count to next header (used for TS_TAPE */
				/* after EOT) */
	int inode;		/* inode that we are currently dealing with */
	int fd;			/* FD for this slave */
	int pid;		/* PID for this slave */
	int sent;		/* 1 == we've sent this slave requests */
	int firstrec;		/* record number of this block */
	char (*tblock)[TP_BSIZE]; /* buffer for data blocks */
	struct req *req;	/* buffer for requests */
} slaves[SLAVES+1];
struct slave *slp;

char	(*nextblock)[TP_BSIZE];

static time_t tstart_volume;	/* time of volume start */ 
static int tapea_volume;	/* value of spcl.c_tapea at volume start */

int master;		/* pid of master, for sending error signals */
int tenths;		/* length of tape overhead per block written */
static int caught;	/* have we caught the signal to proceed? */
static int ready;	/* have we reached the lock point without having */
			/* received the SIGUSR2 signal from the prev slave? */
static sigjmp_buf jmpbuf;	/* where to jump to if we are ready when the */
			/* SIGUSR2 arrives from the previous slave */
#ifdef USE_QFA
static int gtperr = 0;
#endif

int
alloctape(void)
{
	int pgoff = getpagesize() - 1;
	char *buf;
	int i;

	writesize = ntrec * TP_BSIZE;
	reqsiz = (ntrec + 1) * sizeof(struct req);
	/*
	 * CDC 92181's and 92185's make 0.8" gaps in 1600-bpi start/stop mode
	 * (see DEC TU80 User's Guide).  The shorter gaps of 6250-bpi require
	 * repositioning after stopping, i.e, streaming mode, where the gap is
	 * variable, 0.30" to 0.45".  The gap is maximal when the tape stops.
	 */
	if (blocksperfile == 0 && !unlimited)
		tenths = (cartridge ? 16 : density == 625 ? 5 : 8);
	else {
		tenths = 0;
		density = 1;
	}
	/*
	 * Allocate tape buffer contiguous with the array of instruction
	 * packets, so flushtape() can write them together with one write().
	 * Align tape buffer on page boundary to speed up tape write().
	 */
	for (i = 0; i <= SLAVES; i++) {
		buf = (char *)
		    malloc((unsigned)(reqsiz + writesize + pgoff + TP_BSIZE));
		if (buf == NULL)
			return(0);
		slaves[i].tblock = (char (*)[TP_BSIZE])
#ifdef	__linux__
		    (((long)&buf[reqsiz] + pgoff) &~ pgoff);
#else
		    (((long)&buf[ntrec + 1] + pgoff) &~ pgoff);
#endif
		slaves[i].req = (struct req *)slaves[i].tblock - ntrec - 1;
	}
	slp = &slaves[0];
	slp->count = 1;
	slp->tapea = 0;
	slp->firstrec = 0;
	nextblock = slp->tblock;
	return(1);
}

void
writerec(const void *dp, int isspcl)
{

	slp->req[trecno].dblk = (daddr_t)0;
	slp->req[trecno].count = 1;
	/* XXX post increment triggers an egcs-1.1.2-12 bug on alpha/sparc */
	*(union u_spcl *)(*(nextblock)) = *(union u_spcl *)dp;

	/* Need to write it to the archive file */
	if (! AfileActive && isspcl && (spcl.c_type == TS_END))
		AfileActive = 1;
	if (AfileActive && Afile >= 0) {
		/* When we dump an inode which is not a directory,
		 * it means we ended the archive contents */
		if (isspcl && (spcl.c_type == TS_INODE) &&
		    ((spcl.c_dinode.di_mode & S_IFMT) != IFDIR))
			AfileActive = 0;
		else {
			union u_spcl tmp;
			tmp = *(union u_spcl *)dp;
			/* Write the record, _uncompressed_ */
			if (isspcl) {
				tmp.s_spcl.c_flags &= ~DR_COMPRESSED;
				mkchecksum(&tmp);
			}
			if (write(Afile, &tmp, TP_BSIZE) != TP_BSIZE)
				msg("error writing archive file: %s\n", 
			    	strerror(errno));
		}
	}

	nextblock++;
	if (isspcl)
		lastspclrec = spcl.c_tapea;
	trecno++;
	spcl.c_tapea++;
	if (trecno >= ntrec)
		flushtape();
}

void
dumpblock(daddr_t blkno, int size)
{
	int avail, tpblks, dblkno;

	dblkno = fsbtodb(sblock, blkno);
	tpblks = size >> tp_bshift;
	while ((avail = MIN(tpblks, ntrec - trecno)) > 0) {
		slp->req[trecno].dblk = dblkno;
		slp->req[trecno].count = avail;
		trecno += avail;
		spcl.c_tapea += avail;
		if (trecno >= ntrec)
			flushtape();
		dblkno += avail << (tp_bshift - dev_bshift);
		tpblks -= avail;
	}
}

int	nogripe = 0;

static void
tperror(int errnum)
{

	if (pipeout) {
		msg("write error on %s: %s\n", tape, strerror(errnum));
		quit("Cannot recover\n");
		/* NOTREACHED */
	}
	msg("write error %d blocks into volume %d: %s\n", 
	    blocksthisvol, tapeno, strerror(errnum));
	broadcast("DUMP WRITE ERROR!\n");
	if (query("Do you want to rewrite this volume?")) {
		msg("Closing this volume.  Prepare to restart with new media;\n");
		msg("this dump volume will be rewritten.\n");
		killall();
		nogripe = 1;
		close_rewind();
		Exit(X_REWRITE);
	}
	if (query("Do you want to start the next tape?"))
		return;
	dumpabort(0);
}

static void
sigpipe(UNUSED(int signo))
{

	quit("Broken pipe\n");
}

/*
 * do_stats --
 *     Update xferrate stats
 */
time_t
do_stats(void)
{
	time_t tnow, ttaken;
	int blocks;

	tnow = time(NULL);
	ttaken = tnow - tstart_volume;
	blocks = spcl.c_tapea - tapea_volume;
	msg("Volume %d completed at: %s", tapeno, ctime(&tnow));
	if (! compressed)
		msg("Volume %d %ld blocks (%.2fMB)\n", tapeno, 
			blocks, ((double)blocks * TP_BSIZE / 1048576));
	if (ttaken > 0) {
		long volkb = (bytes_written - tapea_bytes) / 1024;
		long txfrate = volkb / ttaken;
		msg("Volume %d took %d:%02d:%02d\n", tapeno,
			ttaken / 3600, (ttaken % 3600) / 60, ttaken % 60);
		msg("Volume %d transfer rate: %ld kB/s\n", tapeno,
			txfrate);
		xferrate += txfrate;
		if (compressed) {
			double rate = .0005 + (double) blocks / (double) volkb;
			msg("Volume %d %ldkB uncompressed, %ldkB compressed,"
				" %1.3f:1\n",
				tapeno, blocks, volkb, rate);
		}
	}
	return(tnow);
}

char *
mktimeest(time_t tnow)
{
	static char msgbuf[128];
	time_t deltat;

	msgbuf[0] = '\0';

	if (blockswritten < 500)
		return NULL;
	if (blockswritten > tapesize)
		tapesize = blockswritten;
	deltat = tstart_writing - tnow + (1.0 * (tnow - tstart_writing))
		/ blockswritten * tapesize;
	if (tnow > tstart_volume)
		(void)snprintf(msgbuf, sizeof(msgbuf),
			"%3.2f%% done at %ld kB/s, finished in %d:%02d\n",
			(blockswritten * 100.0) / tapesize,
			(spcl.c_tapea - tapea_volume) / (tnow - tstart_volume),
			(int)(deltat / 3600), (int)((deltat % 3600) / 60));
	else
		(void)snprintf(msgbuf, sizeof(msgbuf),
			"%3.2f%% done, finished in %d:%02d\n",
			(blockswritten * 100.0) / tapesize,
			(int)(deltat / 3600), (int)((deltat % 3600) / 60));

	return msgbuf;
}

#if defined(SIGINFO)
/*
 * statussig --
 *     information message upon receipt of SIGINFO
 */
void
statussig(int notused)
{
	int save_errno = errno;
	char *buf;

	buf = mktimeest(time(NULL));
	if (buf)
		write(STDERR_FILENO, buf, strlen(buf));
	errno = save_errno;
}
#endif

static void
flushtape(void)
{
	int i, blks, got;
	long lastfirstrec;
	struct slave_results returned;

	int siz = (char *)nextblock - (char *)slp->req;

	slp->req[trecno].count = 0;			/* Sentinel */

	if (dump_atomic_write( slp->fd, (char *)slp->req, siz) != siz)
		quit("error writing command pipe: %s\n", strerror(errno));
	slp->sent = 1; /* we sent a request, read the response later */

	lastfirstrec = slp->firstrec;

	if (++slp >= &slaves[SLAVES])
		slp = &slaves[0];

	/* Read results back from next slave */
	if (slp->sent) {
		if (dump_atomic_read( slp->fd, (char *)&returned, sizeof returned)
		    != sizeof returned) {
			perror("  DUMP: error reading command pipe in master");
			dumpabort(0);
		}
		got = returned.unclen;
		bytes_written += returned.clen;
		if (returned.unclen == returned.clen)
			uncomprblks++;
		slp->sent = 0;

		/* Check for errors or end of tape */
		if (got <= 0) {
			/* Check for errors */
			if (got < 0)
				tperror(-got);
			else
				msg("End of tape detected\n");

			/*
			 * Drain the results, don't care what the values were.
			 * If we read them here then trewind won't...
			 */
			for (i = 0; i < SLAVES; i++) {
				if (slaves[i].sent) {
					if (dump_atomic_read( slaves[i].fd,
					    (char *)&returned, sizeof returned)
					    != sizeof returned) {
						perror("  DUMP: error reading command pipe in master");
						dumpabort(0);
					}
					slaves[i].sent = 0;
				}
			}

			close_rewind();
			rollforward();
			return;
		}
	}

	blks = 0;
	if (spcl.c_type != TS_END) {
		for (i = 0; i < spcl.c_count; i++)
			if (spcl.c_addr[i] != 0)
				blks++;
	}
	slp->count = lastspclrec + blks + 1 - spcl.c_tapea;
	slp->tapea = spcl.c_tapea;
	slp->firstrec = lastfirstrec + ntrec;
	slp->inode = curino;
	nextblock = slp->tblock;
	trecno = 0;
	asize += tenths + returned.clen / density;
	blockswritten += ntrec;
	blocksthisvol += ntrec;
	if (!pipeout && !unlimited) {
		if (blocksperfile) {
			if ( compressed ? (bytes_written - tapea_bytes + SLAVES * (writesize + sizeof(struct tapebuf))) >= (((long long)blocksperfile) * 1024)
					: blocksthisvol >= blocksperfile ) {
				close_rewind();
				startnewtape(0);
			}
		}
		else if (asize > tsize) {
			close_rewind();
			startnewtape(0);
		}
	}
	timeest();
}

time_t
trewind(void)
{
	int f;
	int got;
	struct slave_results returned;

	for (f = 0; f < SLAVES; f++) {
		/*
		 * Drain the results, but unlike EOT we DO (or should) care
		 * what the return values were, since if we detect EOT after
		 * we think we've written the last blocks to the tape anyway,
		 * we have to replay those blocks with rollforward.
		 *
		 * fixme: punt for now.
		 */
		if (slaves[f].sent) {
			if (dump_atomic_read( slaves[f].fd, (char *)&returned, sizeof returned)
			    != sizeof returned) {
				perror("  DUMP: error reading command pipe in master");
				dumpabort(0);
			}
			got = returned.unclen;
			bytes_written += returned.clen;
			if (returned.unclen == returned.clen)
				uncomprblks++;
			slaves[f].sent = 0;

			if (got < 0)
				tperror(-got);

			if (got == 0) {
				msg("EOT detected in last 2 tape records!\n");
				msg("Use a longer tape, decrease the size estimate\n");
				quit("or use no size estimate at all.\n");
			}
		}
		(void) close(slaves[f].fd);
	}
	while (wait((int *)NULL) >= 0)	/* wait for any signals from slaves */
		/* void */;

	if (!pipeout) {

		msg("Closing %s\n", tape);

#ifdef RDUMP
		if (host) {
			rmtclose();
			while (rmtopen(tape, O_RDONLY) < 0)
				sleep(10);
			rmtclose();
		}
		else 
#endif
		{
			(void) close(tapefd);
			if (!fifoout) {
				while ((f = OPEN(tape, O_RDONLY)) < 0)
					sleep (10);
				(void) close(f);
			}
		}
	}
	return do_stats();
}

		
void
close_rewind(void)
{
	int eot_code = 1;
	(void)trewind();
	if (nexttape || Mflag)
		return;
	if (eot_script) {
		msg("Launching %s\n", eot_script);
		eot_code = system_command(eot_script, tape, tapeno);
	}
	if (eot_code != 0 && eot_code != 1) {
		msg("Dump aborted by the end of tape script\n");
		dumpabort(0);
	}
	if (eot_code == 0)
		return;
	if (!nogripe) {
		msg("Change Volumes: Mount volume #%d\n", tapeno+1);
		broadcast("CHANGE DUMP VOLUMES!\7\7\n");
	}
	while (!query("Is the new volume mounted and ready to go?"))
		if (query("Do you want to abort?")) {
			dumpabort(0);
			/*NOTREACHED*/
		}
}

void
rollforward(void)
{
	struct req *p, *q = NULL, *prev;
	struct slave *tslp;
	int i, size, savedtapea, got;
	union u_spcl *ntb, *otb;
	struct slave_results returned;
#ifdef __linux__
	int blks;
	long lastfirstrec;
#endif
	tslp = &slaves[SLAVES];
	ntb = (union u_spcl *)tslp->tblock[1];

	/*
	 * Each of the N slaves should have requests that need to
	 * be replayed on the next tape.  Use the extra slave buffers
	 * (slaves[SLAVES]) to construct request lists to be sent to
	 * each slave in turn.
	 */
	for (i = 0; i < SLAVES; i++) {
		q = &tslp->req[1];
		otb = (union u_spcl *)slp->tblock;

		/*
		 * For each request in the current slave, copy it to tslp.
		 */

		prev = NULL;
		for (p = slp->req; p->count > 0; p += p->count) {
			*q = *p;
			if (p->dblk == 0)
				*ntb++ = *otb++; /* copy the datablock also */
			prev = q;
			q += q->count;
		}
		if (prev == NULL)
			quit("rollforward: protocol botch");
		if (prev->dblk != 0)
			prev->count -= 1;
		else
			ntb--;
		q -= 1;
		q->count = 0;
		q = &tslp->req[0];
		if (i == 0) {
			q->dblk = 0;
			q->count = 1;
			trecno = 0;
			nextblock = tslp->tblock;
			savedtapea = spcl.c_tapea;
			spcl.c_tapea = slp->tapea;
			startnewtape(0);
			spcl.c_tapea = savedtapea;
			lastspclrec = savedtapea - 1;
		}
		size = (char *)ntb - (char *)q;
		if (dump_atomic_write( slp->fd, (char *)q, size) != size) {
			perror("  DUMP: error writing command pipe");
			dumpabort(0);
		}
		slp->sent = 1;
#ifdef __linux__
		lastfirstrec = slp->firstrec;
#endif
		if (++slp >= &slaves[SLAVES])
			slp = &slaves[0];

		q->count = 1;

		if (prev->dblk != 0) {
			/*
			 * If the last one was a disk block, make the
			 * first of this one be the last bit of that disk
			 * block...
			 */
			q->dblk = prev->dblk +
				prev->count * (TP_BSIZE / DEV_BSIZE);
			ntb = (union u_spcl *)tslp->tblock;
		} else {
			/*
			 * It wasn't a disk block.  Copy the data to its
			 * new location in the buffer.
			 */
			q->dblk = 0;
			*((union u_spcl *)tslp->tblock) = *ntb;
			ntb = (union u_spcl *)tslp->tblock[1];
		}
	}
	slp->req[0] = *q;
	nextblock = slp->tblock;
	if (q->dblk == 0) {
#ifdef __linux__
	/* XXX post increment triggers an egcs-1.1.2-12 bug on alpha/sparc */
		*(union u_spcl *)(*nextblock) = *(union u_spcl *)tslp->tblock;
#endif
		nextblock++;
	}
	trecno = 1;

	/*
	 * Clear the first slaves' response.  One hopes that it
	 * worked ok, otherwise the tape is much too short!
	 */
	if (slp->sent) {
		if (dump_atomic_read( slp->fd, (char *)&returned, sizeof returned)
		    != sizeof returned) {
			perror("  DUMP: error reading command pipe in master");
			dumpabort(0);
		}
		got = returned.unclen;
		bytes_written += returned.clen;
		if (returned.clen == returned.unclen)
			uncomprblks++;
		slp->sent = 0;

		if (got < 0)
			tperror(-got);

		if (got == 0) {
			quit("EOT detected at start of the tape!\n");
		}
	}

#ifdef __linux__
	blks = 0;
	if (spcl.c_type != TS_END) {
		for (i = 0; i < spcl.c_count; i++)
			if (spcl.c_addr[i] != 0)
				blks++;
	}

	slp->firstrec = lastfirstrec + ntrec;
	slp->count = lastspclrec + blks + 1 - spcl.c_tapea;
	slp->inode = curino;
	asize += tenths + returned.clen / density;
	blockswritten += ntrec;
	blocksthisvol += ntrec;
#endif
}

/*
 * We implement taking and restoring checkpoints on the tape level.
 * When each tape is opened, a new process is created by forking; this
 * saves all of the necessary context in the parent.  The child
 * continues the dump; the parent waits around, saving the context.
 * If the child returns X_REWRITE, then it had problems writing that tape;
 * this causes the parent to fork again, duplicating the context, and
 * everything continues as if nothing had happened.
 */
void
startnewtape(int top)
{
	int	parentpid;
	int	childpid;
	int	status;
	int	waitpid;
	char	*p;

#ifdef	__linux__
	sigset_t sigs;
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGINT);
	sigprocmask(SIG_BLOCK, &sigs, NULL);
#else	/* __linux__ */
#ifdef sunos
	void	(*interrupt_save)();
#else
	sig_t	interrupt_save;
#endif
	interrupt_save = signal(SIGINT, SIG_IGN);
#endif	/* __linux__ */

	parentpid = getpid();
	tapea_volume = spcl.c_tapea;
	tapea_bytes = bytes_written;
	tstart_volume = time(NULL);

restore_check_point:
#ifdef	__linux__
	sigprocmask(SIG_UNBLOCK, &sigs, NULL);
#else
	(void)signal(SIGINT, interrupt_save);
#endif
	/*
	 *	All signals are inherited...
	 */
	childpid = fork();
	if (childpid < 0) {
		msg("Context save fork fails in parent %d\n", parentpid);
		Exit(X_ABORT);
	}
	if (childpid != 0) {
		/*
		 *	PARENT:
		 *	save the context by waiting
		 *	until the child doing all of the work returns.
		 *	don't catch the interrupt
		 */
#ifdef	__linux__
		sigprocmask(SIG_BLOCK, &sigs, NULL);
#else
		signal(SIGINT, SIG_IGN);
#endif
#ifdef TDEBUG
		msg("Tape: %d; parent process: %d child process %d\n",
			tapeno+1, parentpid, childpid);
#endif /* TDEBUG */
		while ((waitpid = wait(&status)) != childpid)
			if (waitpid != rshpid)
				msg("Parent %d waiting for child %d has another child %d return\n",
				parentpid, childpid, waitpid);
		if (status & 0xFF) {
			msg("Child %d returns LOB status %o\n",
				childpid, status&0xFF);
		}
		status = (status >> 8) & 0xFF;
#ifdef TDEBUG
		switch(status) {
			case X_FINOK:
				msg("Child %d finishes X_FINOK\n", childpid);
				break;
			case X_ABORT:
				msg("Child %d finishes X_ABORT\n", childpid);
				break;
			case X_REWRITE:
				msg("Child %d finishes X_REWRITE\n", childpid);
				break;
			default:
				msg("Child %d finishes unknown %d\n",
					childpid, status);
				break;
		}
#endif /* TDEBUG */
		switch(status) {
			case X_FINOK:
				Exit(X_FINOK);
			case X_ABORT:
				Exit(X_ABORT);
			case X_REWRITE:
				goto restore_check_point;
			default:
				msg("Bad return code from dump: %d\n", status);
				Exit(X_ABORT);
		}
		/*NOTREACHED*/
	} else {	/* we are the child; just continue */
#ifdef TDEBUG
		sleep(4);	/* allow time for parent's message to get out */
		msg("Child on Tape %d has parent %d, my pid = %d\n",
			tapeno+1, parentpid, getpid());
#endif /* TDEBUG */
		/*
		 * If we have a name like "/dev/rmt0,/dev/rmt1",
		 * use the name before the comma first, and save
		 * the remaining names for subsequent volumes.
		 */
		tapeno++;               /* current tape sequence */
		if (Mflag) {
			snprintf(tape, MAXPATHLEN, "%s%03d", tapeprefix, tapeno);
			tape[MAXPATHLEN - 1] = '\0';
			msg("Dumping volume %d on %s\n", tapeno, tape);
		}
		else if (nexttape || strchr(tapeprefix, ',')) {
			if (nexttape && *nexttape)
				tapeprefix = nexttape;
			if ((p = strchr(tapeprefix, ',')) != NULL) {
				*p = '\0';
				nexttape = p + 1;
			} else
				nexttape = NULL;
			strncpy(tape, tapeprefix, MAXPATHLEN);
			tape[MAXPATHLEN - 1] = '\0';
			msg("Dumping volume %d on %s\n", tapeno, tape);
		}
#ifdef RDUMP
		while ((tapefd = (host ? rmtopen(tape, O_WRONLY|O_CREAT|O_TRUNC) : pipeout ? 
			fileno(stdout) : 
			OPEN(tape, O_WRONLY|O_CREAT|O_TRUNC, 0666))) < 0)
#else
		while ((tapefd = (pipeout ? fileno(stdout) :
				  OPEN(tape, O_WRONLY|O_CREAT|O_TRUNC, 0666))) < 0)
#endif
		    {
			msg("Cannot open output \"%s\": %s\n", tape, 
			    strerror(errno));
			if (!query("Do you want to retry the open?"))
				dumpabort(0);
		}
#ifdef RDUMP
		if (!host)
#endif
			{
				struct mtget mt_stat;
				magtapeout = ioctl(tapefd, MTIOCGET, (char *)&mt_stat) == 0;
				/*
				msg("Output is to %s\n", 
					magtapeout ? "tape" : "file/pipe");
				*/
			}

		enslave();  /* Share open tape file descriptor with slaves */

		asize = 0;
		blocksthisvol = 0;
		if (top)
			newtape++;		/* new tape signal */
		spcl.c_count = slp->count;
		/*
		 * measure firstrec in TP_BSIZE units since restore doesn't
		 * know the correct ntrec value...
		 */
		spcl.c_firstrec = slp->firstrec;
		spcl.c_volume++;
		spcl.c_type = TS_TAPE;
		spcl.c_flags |= DR_NEWHEADER;
		spcl.c_ntrec = ntrec;
		if (compressed)
			spcl.c_flags |= DR_COMPRESSED;
		writeheader((dump_ino_t)slp->inode);
		spcl.c_flags &=~ DR_NEWHEADER;
		msg("Volume %d started with block %ld at: %s", tapeno, 
		    spcl.c_tapea, ctime(&tstart_volume));
		if (tapeno > 1)
			msg("Volume %d begins with blocks from inode %d\n",
				tapeno, slp->inode);
		if (tapeno < (int)TP_NINOS)
			volinfo[tapeno] = slp->inode;
	}
}

void
dumpabort(UNUSED(int signo))
{

	if (master != 0 && master != getpid())
		/* Signals master to call dumpabort */
		(void) kill(master, SIGTERM);
	else {
		killall();
		msg("The ENTIRE dump is aborted.\n");
	}
#ifdef RDUMP
	rmtclose();
#endif
	Exit(X_ABORT);
}

void
Exit(int status)
{

#ifdef TDEBUG
	msg("pid = %d exits with status %d\n", getpid(), status);
#endif /* TDEBUG */
	exit(status);
}

/*
 * proceed - handler for SIGUSR2, used to synchronize IO between the slaves.
 */
static void
proceed(UNUSED(int signo))
{
	if (ready)
		siglongjmp(jmpbuf, 1);
	caught++;
}

void
enslave(void)
{
	int cmd[2];
#ifdef	LINUX_FORK_BUG
	int i, j;
#else
	int i, j;
#endif

	master = getpid();

    {	struct sigaction sa;
	memset(&sa, 0, sizeof sa);
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = dumpabort;
	sigaction(SIGTERM, &sa, NULL); /* Slave sends SIGTERM on dumpabort() */
	sa.sa_handler = sigpipe;
	sigaction(SIGPIPE, &sa, NULL);
	sa.sa_handler = proceed;
	sa.sa_flags = SA_RESTART;
	sigaction(SIGUSR2, &sa, NULL); /* Slave sends SIGUSR2 to next slave */
   }

	for (i = 0; i < SLAVES; i++) {
		if (i == slp - &slaves[0]) {
			caught = 1;
		} else {
			caught = 0;
		}

		if (socketpair(AF_UNIX, SOCK_STREAM, 0, cmd) < 0 ||
		    (slaves[i].pid = fork()) < 0)
			quit("too many slaves, %d (recompile smaller): %s\n",
			    i, strerror(errno));

		slaves[i].fd = cmd[1];
		slaves[i].sent = 0;
		if (slaves[i].pid == 0) { 	    /* Slave starts up here */
			sigset_t sigs;
			for (j = 0; j <= i; j++)
			        (void) close(slaves[j].fd);
			sigemptyset(&sigs);
			sigaddset(&sigs, SIGINT);  /* Master handles this */
#if defined(SIGINFO)
			sigaddset(&sigs, SIGINFO);
#endif
			sigprocmask(SIG_BLOCK, &sigs, NULL);

#ifdef	LINUX_FORK_BUG
			if (dump_atomic_write( cmd[0], (char *) &i, sizeof i)
			    != sizeof i)
				quit("master/slave protocol botched 3\n");
#endif
			doslave(cmd[0], 
#ifdef WRITEDEBUG
				i, 
#endif
				(slaves[i].pid == slp->pid));
			Exit(X_FINOK);
		}
		else
			close(cmd[0]);
	}

#ifdef	LINUX_FORK_BUG
	/*
	 * Wait for all slaves to _actually_ start to circumvent a bug in
	 * Linux kernels >= 2.1.3 where a signal sent to a child that hasn't
	 * returned from fork() causes a SEGV in the child process
	 */
	for (i = 0; i < SLAVES; i++)
		if (dump_atomic_read( slaves[i].fd, (char *) &j, sizeof j) != sizeof j)
			quit("master/slave protocol botched 4\n");
#endif

	for (i = 0; i < SLAVES; i++)
		(void) dump_atomic_write( slaves[i].fd, 
			      (char *) &slaves[(i + 1) % SLAVES].pid, 
		              sizeof slaves[0].pid);
		
	master = 0; 
}

void
killall(void)
{
	int i;

	for (i = 0; i < SLAVES; i++)
		if (slaves[i].pid > 0) {
			(void) kill(slaves[i].pid, SIGKILL);
			slaves[i].sent = 0;
		}
}

/*
 * Synchronization - each process waits for a SIGUSR2 from the
 * previous process before writing to the tape, and sends SIGUSR2
 * to the next process when the tape write completes. On tape errors
 * a SIGUSR1 is sent to the master which then terminates all of the
 * slaves.
 */
static void
doslave(int cmd, 
#ifdef WRITEDEBUG
	int slave_number, 
#endif
	int first)
{
	int nread;
	int nextslave;
	volatile int wrote = 0, size, eot_count, bufsize;
	char * volatile buffer;
#if defined(HAVE_ZLIB) || defined(HAVE_BZLIB)
	struct tapebuf * volatile comp_buf = NULL;
	int compresult;
	volatile int do_compress = !first;
	unsigned long worklen;
#ifdef HAVE_BZLIB
	unsigned int worklen2;
#endif
#endif /* HAVE_ZLIB || HAVE_BZLIB */
	struct slave_results returns;
#ifdef	__linux__
	errcode_t retval;
#endif
#ifdef USE_QFA
	long long curtapepos;
	union u_spcl *uspclptr;
	struct s_spcl *spclptr;
#endif /* USE_QFA */
	sigset_t set;

	sigemptyset(&set);
	sigaddset(&set, SIGUSR2);
	sigprocmask(SIG_BLOCK, &set, NULL);
	sigemptyset(&set);

	/*
	 * Need our own seek pointer.
	 */
	(void) close(diskfd);
	if ((diskfd = OPEN(disk, O_RDONLY)) < 0)
		quit("slave couldn't reopen disk: %s\n", strerror(errno));
#ifdef	__linux__
#ifdef BLKFLSBUF
	(void)ioctl(diskfd, BLKFLSBUF);
#endif
	ext2fs_close(fs);
	retval = dump_fs_open(disk, &fs);
	if (retval)
		quit("slave couldn't reopen disk: %s\n", error_message(retval));
#endif	/* __linux__ */

	/*
	 * Need the pid of the next slave in the loop...
	 */
	if ((nread = dump_atomic_read( cmd, (char *)&nextslave, sizeof nextslave))
	    != sizeof nextslave) {
		quit("master/slave protocol botched - didn't get pid of next slave.\n");
	}

#if defined(HAVE_ZLIB) || defined(HAVE_BZLIB)
	/* if we're doing a compressed dump, allocate the compress buffer */
	if (compressed) {
		comp_buf = malloc(sizeof(struct tapebuf) + TP_BSIZE + writesize);
		if (comp_buf == NULL)
			quit("couldn't allocate a compress buffer.\n");
		if (bzipflag)
			comp_buf->flags = COMPRESS_BZLIB;
		else
			comp_buf->flags = COMPRESS_ZLIB;
	}
#endif /* HAVE_ZLIB || HAVE_BZLIB */

	/*
	 * Get list of blocks to dump, read the blocks into tape buffer
	 */
	while ((nread = dump_atomic_read( cmd, (char *)slp->req, reqsiz)) == reqsiz) {
		struct req *p = slp->req;

		for (trecno = 0; trecno < ntrec;
		     trecno += p->count, p += p->count) {
			if (p->dblk) {	/* read a disk block */
				bread(p->dblk, slp->tblock[trecno],
					p->count * TP_BSIZE);
			} else {	/* read record from pipe */
				if (p->count != 1 || dump_atomic_read( cmd,
				    (char *)slp->tblock[trecno],
				    TP_BSIZE) != TP_BSIZE)
				       quit("master/slave protocol botched.\n");
			}
		}

		/* Try to write the data... */
		wrote = 0;
		eot_count = 0;
		size = 0;
		buffer = (char *) slp->tblock[0];	/* set write pointer */
		bufsize = writesize;			/* length to write */
		returns.clen = returns.unclen = bufsize;

#if defined(HAVE_ZLIB) || defined(HAVE_BZLIB)
		/* 
		 * When writing a compressed dump, each block except
		 * the first one on each tape is written
		 * from struct tapebuf with an 4 byte prefix
		 * followed by the data. This can be less than
		 * writesize. Restore, on a short read, can compare the
		 * length read to the compressed length in the header
		 * to verify that the read was good. Blocks which don't
		 * compress well are written uncompressed.
		 * The first block written by each slave is not compressed
		 * and does not have a prefix.
		 */

		if (compressed && do_compress) {
			comp_buf->length = bufsize;
			worklen = TP_BSIZE + writesize;
			compresult = 1;
#ifdef HAVE_ZLIB
			if (!bzipflag) {
				compresult = compress2(comp_buf->buf, 
						       &worklen,
						       (char *)slp->tblock[0],
						       writesize, 
						       compressed);
				if (compresult == Z_OK)
					compresult = 1;
				else
					compresult = 0;
			}
#endif /* HAVE_ZLIB */
#ifdef HAVE_BZLIB
			if (bzipflag) {
				worklen2 = worklen;
				compresult = BZ2_bzBuffToBuffCompress(
						       comp_buf->buf,
						       &worklen2,
						       (char *)slp->tblock[0],
						       writesize,
						       compressed,
						       0, 30);
				worklen = worklen2;
				if (compresult == BZ_OK)
					compresult = 1;
				else
					compresult = 0;
			}

#endif /* HAVE_BZLIB */
			if (compresult && worklen <= ((unsigned long)writesize - 16)) {
				/* write the compressed buffer */
				comp_buf->length = worklen;
				comp_buf->compressed = 1;
				buffer = (char *) comp_buf;
				returns.clen = bufsize = worklen + sizeof(struct tapebuf);
			}
			else {
				/* write the data uncompressed */
				comp_buf->length = writesize;
				comp_buf->compressed = 0;
				buffer = (char *) comp_buf;
				returns.clen = bufsize = writesize + sizeof(struct tapebuf);
				returns.unclen = returns.clen;
				memcpy(comp_buf->buf, (char *)slp->tblock[0], writesize);
			}
		}
		/* compress the remaining blocks if we're compressing */
		do_compress = compressed;
#endif /* HAVE_ZLIB  || HAVE_BZLIB */

		if (sigsetjmp(jmpbuf, 1) == 0) {
			ready = 1;
			if (!caught)
				sigsuspend(&set);
		}
		ready = 0;
		caught = 0;

#ifdef USE_QFA
		if (gTapeposfd >= 0) {
			int i;
			int firstpass = 1;
			for (i = 0; i < ntrec; ++i) {
				uspclptr = (union u_spcl *)&slp->tblock[i];
				spclptr = &uspclptr->s_spcl;
				if ((spclptr->c_magic == NFS_MAGIC) && 
				    (spclptr->c_type == TS_INODE) &&
		                    ((spclptr->c_dinode.di_mode & S_IFMT) != IFDIR) &&
				    (spclptr->c_date == gThisDumpDate)) {
					/* if an error occured previously don't
					 * try again */
					if (firstpass) {
						firstpass = 0;
						if (gtperr == 0) 
							gtperr = GetTapePos(&curtapepos);
					}
					if (gtperr == 0)
							MkTapeString(spclptr, curtapepos);
				}
			}
		}
#endif /* USE_QFA */
						
		while (eot_count < 10 && size < bufsize) {
#ifdef RDUMP
			if (host)
				wrote = rmtwrite(buffer + size, bufsize - size);
			else
#endif
				wrote = write(tapefd, buffer + size, bufsize - size);
#ifdef WRITEDEBUG
			printf("slave %d wrote %d\n", slave_number, wrote);
#endif
			if (wrote < 0)
				break;
			if (wrote == 0)
				eot_count++;
			size += wrote;
		}

#ifdef WRITEDEBUG
		if (size != bufsize)
		 printf("slave %d only wrote %d out of %d bytes and gave up.\n",
		     slave_number, size, bufsize);
#endif

		/*
		 * Handle ENOSPC as an EOT condition.
		 */
		if (wrote < 0 && errno == ENOSPC) {
			wrote = 0;
			eot_count++;
		}

		if (eot_count > 0)
			returns.clen = returns.unclen = 0;

		/*
		 * pass errno back to master for special handling
		 */
		if (wrote < 0)
			returns.unclen = -errno;

		/*
		 * pass size of data and size of write back to master
		 * (for EOT handling)
		 */
		(void) dump_atomic_write( cmd, (char *)&returns, sizeof returns);

		/*
		 * Signal the next slave to go.
		 */
		(void) kill(nextslave, SIGUSR2);
	}
	if (nread != 0)
		quit("error reading command pipe: %s\n", strerror(errno));
}

/*
 * Since a read from a pipe may not return all we asked for,
 * or a write may not write all we ask if we get a signal,
 * loop until the count is satisfied (or error).
 */
static ssize_t
dump_atomic_read(int fd, void *buf, size_t count)
{
	int got, need = count;

	do {
		while ((got = read(fd, buf, need)) > 0 && (need -= got) > 0)
			(char *)buf += got;
	} while (got == -1 && errno == EINTR);
	return (got < 0 ? got : (ssize_t)count - need);
}

/*
 * Since a read from a pipe may not return all we asked for,
 * or a write may not write all we ask if we get a signal,
 * loop until the count is satisfied (or error).
 */
static ssize_t
dump_atomic_write(int fd, const void *buf, size_t count)
{
	int got, need = count;

	do {
		while ((got = write(fd, buf, need)) > 0 && (need -= got) > 0)
			(char *)buf += got;
	} while (got == -1 && errno == EINTR);
	return (got < 0 ? got : (ssize_t)count - need);
}


#ifdef USE_QFA
/*
 * read the current tape position
 */
static int
GetTapePos(long long *pos)
{
	int err = 0;

#ifdef RDUMP
	if (host) {
		*pos = (long long) rmtseek(0, SEEK_CUR);
		err = *pos < 0;
	}
	else 
#endif
	{
	if (magtapeout) {
		long mtpos;
		*pos = 0;
		err = (ioctl(tapefd, MTIOCPOS, &mtpos) < 0);
		*pos = (long long)mtpos;
	}
	else {
		*pos = LSEEK(tapefd, 0, SEEK_CUR);
		err = (*pos < 0);
	}
	}
	if (err) {
		err = errno;
		msg("[%ld] error: %d (getting tapepos: %lld)\n", getpid(), 
			err, *pos);
		return err;
	}
	return err;
}

static void 
MkTapeString(struct s_spcl *spclptr, long long curtapepos) {

#ifdef DEBUG_QFA
	msg("inode %ld at tapepos %lld\n", spclptr->c_inumber, curtapepos);
#endif

	snprintf(gTps, sizeof(gTps), "%ld\t%d\t%lld\n", 
		 (unsigned long)spclptr->c_inumber, 
		 tapeno, 
		 curtapepos);
	gTps[sizeof(gTps) - 1] = '\0';
	if (write(gTapeposfd, gTps, strlen(gTps)) != (ssize_t)strlen(gTps)) {
      		warn("error writing tapepos file.\n");
	}
}
#endif /* USE_QFA */
