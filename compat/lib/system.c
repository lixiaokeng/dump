/*
 *	Ported to Linux's Second Extended File System as part of the
 *	dump and restore backup suit
 *	Remy Card <card@Linux.EU.Org>, 1994-1997
 *	Stelian Pop <pop@noos.fr>, 1999-2000
 *	Stelian Pop <pop@noos.fr> - Alcôve <www.alcove.fr>, 2000
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
	"$Id: system.c,v 1.2 2001/08/13 16:17:52 stelian Exp $";
#endif /* not lint */

#include <config.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "system.h"

/*
 * Executes the command in a shell.
 * Returns -1 if an error occured, the exit status of
 * the command on success.
 */
int system_command(const char *command, const char *device, int volnum) {
	int pid, status;
	char commandstr[4096];

	pid = fork();
	if (pid == -1) {
		perror("  DUMP: unable to fork");
		return -1;
	}
	if (pid == 0) {
		setuid(getuid());
		setgid(getgid());
#if OLD_STYLE_FSCRIPT
		snprintf(commandstr, sizeof(commandstr), "%s", command);
#else
		snprintf(commandstr, sizeof(commandstr), "%s %s %d", command, device, volnum);
#endif
		commandstr[sizeof(commandstr) - 1] = '\0';
		execl("/bin/sh", "sh", "-c", commandstr, NULL);
		perror("unable to execute shell");
		exit(-1);
	}
	do {
		if (waitpid(pid, &status, 0) == -1) {
			if (errno != EINTR) {
				perror("waitpid error");
				return -1;
			}
		} else {
			if (WIFEXITED(status))
				return WEXITSTATUS(status);
			else
				return -1;
		}
	} while(1);
}

