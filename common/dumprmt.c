/*
 *	Ported to Linux's Second Extended File System as part of the
 *	dump and restore backup suit
 *	Remy Card <card@Linux.EU.Org>, 1994-1997
 *	Stelian Pop <stelian@popies.net>, 1999-2000
 *	Stelian Pop <stelian@popies.net> - Alcôve <www.alcove.com>, 2000-2002
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
	"$Id: dumprmt.c,v 1.24 2003/01/10 14:42:50 stelian Exp $";
#endif /* not lint */

#include <config.h>
#include <sys/param.h>
#include <sys/mtio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <fcntl.h>
#ifdef __linux__
#include <sys/types.h>
#ifdef HAVE_EXT2FS_EXT2_FS_H
#include <ext2fs/ext2_fs.h>
#else
#include <linux/ext2_fs.h>
#endif
#include <bsdcompat.h>
#include <signal.h>
#elif defined sunos
#include <sys/vnode.h>

#include <ufs/inode.h>
#else
#include <ufs/ufs/dinode.h>
#endif

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#include <protocols/dumprestore.h>

#include <ctype.h>
#include <errno.h>
#include <compaterr.h>
#include <rmtflags.h>
#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#ifdef __STDC__
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#ifdef	__linux__
#include <ext2fs/ext2fs.h>
#endif

#include <pathnames.h>
#include "dump.h"	/* for X_STARTUP, X_ABORT etc */

#define	TS_CLOSED	0
#define	TS_OPEN		1

static	int rmtstate = TS_CLOSED;
static	int tormtape = -1;
static	int fromrmtape = -1;
int rshpid = -1;
static	const char *rmtpeer = 0;

static	int okname __P((const char *));
static	int rmtcall __P((const char *, const char *));
static	void rmtconnaborted __P((int));
static	int rmtgetb __P((void));
static	int rmtgetconn __P((void));
static	void rmtgets __P((char *, size_t));
static	int rmtreply __P((const char *));
static  int piped_child __P((const char **command));
#ifdef KERBEROS
int	krcmd __P((char **, int /*u_short*/, char *, char *, int *, char *));
#endif

static	int errfd = -1;
extern	int dokerberos;
extern	int ntrec;		/* blocking factor on tape */
#ifndef errno
extern	int errno;
#endif

int
rmthost(const char *host)
{
	if (rmtpeer)
		free((void *)rmtpeer);
	if ((rmtpeer = strdup(host)) == NULL)
		rmtpeer = host;
	signal(SIGPIPE, rmtconnaborted);
	return rmtgetconn();
}

static void
rmtconnaborted(UNUSED(int signo))
{
	msg("Lost connection to remote host.\n");
	if (errfd != -1) {
		fd_set r;
		struct timeval t;

		FD_ZERO(&r);
		FD_SET(errfd, &r);
		t.tv_sec = 0;
		t.tv_usec = 0;
		if (select(errfd + 1, &r, NULL, NULL, &t)) {
			int i;
			char buf[2048];

			if ((i = read(errfd, buf, sizeof(buf) - 1)) > 0) {
				buf[i] = '\0';
				msg("on %s: %s%s", rmtpeer, buf,
					buf[i - 1] == '\n' ? "" : "\n");
			}
		}
	}

	exit(X_ABORT);
}

static int
rmtgetconn(void)
{
	char *cp;
	const char *rmt;
	static struct servent *sp = NULL;
	static struct passwd *pwd = NULL;
	const char *tuser;
	const char *rsh;
	int size;
	int throughput;
	int on;
	char *rmtpeercopy;

	rsh = getenv("RSH");

	if (!rsh && sp == NULL) {
		sp = getservbyname(dokerberos ? "kshell" : "shell", "tcp");
		if (sp == NULL)
			errx(1, "%s/tcp: unknown service",
			    dokerberos ? "kshell" : "shell");
	}
	if (pwd == NULL) {
		pwd = getpwuid(getuid());
		if (pwd == NULL)
			errx(1, "who are you?");
	}
	if ((cp = strchr(rmtpeer, '@')) != NULL) {
		tuser = rmtpeer;
		*cp = '\0';
		if (!okname(tuser))
			exit(X_STARTUP);
		rmtpeer = ++cp;
	} else
		tuser = pwd->pw_name;
	if ((rmt = getenv("RMT")) == NULL)
		rmt = _PATH_RMT;
	msg("");

	if (rsh) {
		const char *rshcmd[6];
		rshcmd[0] = rsh;
		rshcmd[1] = rmtpeer;
		rshcmd[2] = "-l";
		rshcmd[3] = tuser;
		rshcmd[4] = rmt;
		rshcmd[5] = NULL;

		/* Restore the uid and gid. We really don't want
		 * to execute whatever is put into RSH variable with
		 * more priviledges than needed... */
		setuid(getuid());
		setgid(getgid());

		if ((rshpid = piped_child(rshcmd)) < 0) {
			msg("cannot open connection\n");
			return 0;
		}
	}
	else {
		/* Copy rmtpeer to rmtpeercopy to ignore the
		   return value from rcmd. I cannot figure if
		   this is this a bug in rcmd or in my code... */
		rmtpeercopy = (char *)rmtpeer;
#ifdef KERBEROS
		if (dokerberos)
			tormtape = krcmd(&rmtpeercopy, sp->s_port, tuser, rmt, &errfd,
				       (char *)0);
		else
#endif
			tormtape = rcmd(&rmtpeercopy, (u_short)sp->s_port, pwd->pw_name,
				      tuser, rmt, &errfd);
		if (tormtape < 0) {
			msg("login to %s as %s failed.\n", rmtpeer, tuser);
			return 0;
		}
		size = ntrec * TP_BSIZE;
		if (size > 60 * 1024)		/* XXX */
			size = 60 * 1024;
		/* Leave some space for rmt request/response protocol */
		size += 2 * 1024;
		while (size > TP_BSIZE &&
		    setsockopt(tormtape, SOL_SOCKET, SO_SNDBUF, &size, sizeof (size)) < 0)
			    size -= TP_BSIZE;
		(void)setsockopt(tormtape, SOL_SOCKET, SO_RCVBUF, &size, sizeof (size));
		throughput = IPTOS_THROUGHPUT;
		if (setsockopt(tormtape, IPPROTO_IP, IP_TOS,
		    &throughput, sizeof(throughput)) < 0)
			perror("IP_TOS:IPTOS_THROUGHPUT setsockopt");
		on = 1;
		if (setsockopt(tormtape, IPPROTO_TCP, TCP_NODELAY, &on, sizeof (on)) < 0)
			perror("TCP_NODELAY setsockopt");
		fromrmtape = tormtape;
	}
	(void)fprintf(stderr, "Connection to %s established.\n", rmtpeer);
	return 1;
}

static int
okname(const char *cp0)
{
	const char *cp;
	int c;

	for (cp = cp0; *cp; cp++) {
		c = *cp;
		if (!isascii(c) || !(isalnum(c) || c == '_' || c == '-')) {
			warnx("invalid user name %s\n", cp0);
			return (0);
		}
	}
	return (1);
}

int
rmtopen(const char *tape, const int mode)
{
	char buf[MAXPATHLEN];
	char *rmtflags;

	rmtflags = rmtflags_tochar(mode);
	(void)snprintf(buf, sizeof (buf), "O%s\n%d %s\n", 
		       tape, 
		       mode & O_ACCMODE, 
		       rmtflags);
	free(rmtflags);
	rmtstate = TS_OPEN;
	return (rmtcall(tape, buf));
}

void
rmtclose(void)
{

	if (rmtstate != TS_OPEN)
		return;
	rmtcall("close", "C\n");
	rmtstate = TS_CLOSED;
}

int
rmtread(char *buf, size_t count)
{
	char line[30];
	int n, i;
	ssize_t cc;

	(void)snprintf(line, sizeof (line), "R%u\n", (unsigned)count);
	n = rmtcall("read", line);
	if (n < 0)
		/* rmtcall() properly sets errno for us on errors. */
		return (n);
	for (i = 0; i < n; i += cc) {
		cc = read(fromrmtape, buf+i, n - i);
		if (cc <= 0)
			rmtconnaborted(0);
	}
	return (n);
}

int
rmtwrite(const char *buf, size_t count)
{
	char line[30];

	(void)snprintf(line, sizeof (line), "W%ld\n", (long)count);
	write(tormtape, line, strlen(line));
	write(tormtape, buf, count);
	return (rmtreply("write"));
}

OFF_T
rmtseek(OFF_T offset, int pos)
{
	char line[80];

	(void)snprintf(line, sizeof (line), "L%lld\n%d\n", (long long)offset, pos);
	return (rmtcall("seek", line));
}

struct	mtget mts;

struct mtget *
rmtstatus(void)
{
	int i;
	char *cp;

	if (rmtstate != TS_OPEN)
		return (NULL);
	rmtcall("status", "S\n");
	for (i = 0, cp = (char *)&mts; i < (int)sizeof(mts); i++)
		*cp++ = rmtgetb();
	return (&mts);
}

int
rmtioctl(int cmd, int count)
{
	char buf[256];

	if (count < 0)
		return (-1);
	(void)snprintf(buf, sizeof (buf), "I%d\n%d\n", cmd, count);
	return (rmtcall("ioctl", buf));
}

static int
rmtcall(const char *cmd, const char *buf)
{

	if (write(tormtape, buf, strlen(buf)) != (ssize_t)strlen(buf))
		rmtconnaborted(0);
	return (rmtreply(cmd));
}

static int
rmtreply(const char *cmd)
{
	char *cp;
	char code[30], emsg[BUFSIZ];

	rmtgets(code, sizeof (code));
	if (*code == 'E' || *code == 'F') {
		rmtgets(emsg, sizeof (emsg));
		msg("%s: %s", cmd, emsg);
		errno = atoi(code + 1);
		if (*code == 'F')
			rmtstate = TS_CLOSED;
		return (-1);
	}
	if (*code != 'A') {
		/* Kill trailing newline */
		cp = code + strlen(code);
		if (cp > code && *--cp == '\n')
			*cp = '\0';

		msg("Protocol to remote tape server botched (code \"%s\").\n",
		    code);
		rmtconnaborted(0);
	}
	return (atoi(code + 1));
}

static int
rmtgetb(void)
{
	char c;

	if (read(fromrmtape, &c, 1) != 1)
		rmtconnaborted(0);
	return (c);
}

/* Get a line (guaranteed to have a trailing newline). */
static void
rmtgets(char *line, size_t len)
{
	char *cp = line;

	while (len > 1) {
		*cp = rmtgetb();
		if (*cp == '\n') {
			cp[1] = '\0';
			return;
		}
		cp++;
		len--;
	}
	*cp = '\0';
	msg("Protocol to remote tape server botched.\n");
	msg("(rmtgets got \"%s\").\n", line);
	rmtconnaborted(0);
}

int piped_child(const char **command) {
	int pid;
	int to_child_pipe[2];
	int from_child_pipe[2];

	if (pipe (to_child_pipe) < 0) {
		msg ("cannot create pipe: %s\n", strerror(errno));
		return -1;
	}
	if (pipe (from_child_pipe) < 0) {
		msg ("cannot create pipe: %s\n", strerror(errno)); 
		return -1;
	}
	pid = fork ();
	if (pid < 0) {
		msg ("cannot fork: %s\n", strerror(errno));
		return -1;
	}
	if (pid == 0) {
		if (dup2 (to_child_pipe[0], STDIN_FILENO) < 0) {
			msg ("cannot dup2 pipe: %s\n", strerror(errno));
			exit(1);
		}
		if (close (to_child_pipe[1]) < 0) {
			msg ("cannot close pipe: %s\n", strerror(errno));
			exit(1);
		}
		if (close (from_child_pipe[0]) < 0) {
			msg ("cannot close pipe: %s\n", strerror(errno));
			exit(1);
		}
		if (dup2 (from_child_pipe[1], STDOUT_FILENO) < 0) {
			msg ("cannot dup2 pipe: %s\n", strerror(errno));
			exit(1);
		}
		setpgid(0, getpid());
		execvp (command[0], (char *const *) command);
		msg("cannot exec %s: %s\n", command[0], strerror(errno));
		exit(1);
	}
	if (close (to_child_pipe[0]) < 0) {
		msg ("cannot close pipe: %s\n", strerror(errno));
		return -1;
	}
	if (close (from_child_pipe[1]) < 0) {
		msg ("cannot close pipe: %s\n", strerror(errno));
		return -1;
	}
	tormtape = to_child_pipe[1];
	fromrmtape = from_child_pipe[0];
	return pid;
}
