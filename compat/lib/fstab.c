/*
 *	Ported to Linux's Second Extended File System as part of the
 *	dump and restore backup suit
 *	Remy Card <card@Linux.EU.Org>, 1994-1997
 *	Stelian Pop <pop@noos.fr>, 1999-2000
 *	Stelian Pop <pop@noos.fr> - Alcôve <www.alcove.fr>, 2000
 */

/*
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
	"$Id: fstab.c,v 1.11 2001/03/19 13:22:48 stelian Exp $";
#endif /* not lint */

#include <config.h>
#include <errno.h>
#include <fstab.h>
#include <mntent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <bylabel.h>

static FILE *_fs_fp;
static struct fstab _fs_fstab;

static void error __P((int));
static int fstabscan __P((void));

void msg __P((const char *fmt, ...));

static
int fstabscan(void)
{
	struct mntent	*mnt;
	register char *cp;
	int typexx;
#define	MAXLINELENGTH	1024
	char subline[MAXLINELENGTH];

	for (;;) {
		const char *device_name;
		if (!(mnt = getmntent(_fs_fp)))
			return 0;

		device_name = get_device_name(mnt->mnt_fsname);
		if (!device_name) {
			msg("Warning: unable to translate %s\n", mnt->mnt_fsname);
			continue;
		}
		_fs_fstab.fs_spec = device_name;
		_fs_fstab.fs_file = mnt->mnt_dir;
		_fs_fstab.fs_vfstype = mnt->mnt_type;
		_fs_fstab.fs_mntops = mnt->mnt_opts;
		_fs_fstab.fs_type = FSTAB_RW;	/* rw by default under Linux */
		_fs_fstab.fs_freq = mnt->mnt_freq;
		_fs_fstab.fs_passno = mnt->mnt_passno;

		strcpy(subline, _fs_fstab.fs_mntops);
		for (typexx = 0, cp = strtok(subline, ","); cp;
		     cp = strtok((char *)NULL, ",")) {
			if (!strcmp(cp, FSTAB_RW)) {
				_fs_fstab.fs_type = FSTAB_RW;
				break;
			}
			if (!strcmp(cp, FSTAB_RQ)) {
				_fs_fstab.fs_type = FSTAB_RQ;
				break;
			}
			if (!strcmp(cp, FSTAB_RO)) {
				_fs_fstab.fs_type = FSTAB_RO;
				break;
			}
			if (!strcmp(cp, FSTAB_SW)) {
				_fs_fstab.fs_type = FSTAB_SW;
				break;
			}
			if (!strcmp(cp, FSTAB_XX)) {
				_fs_fstab.fs_type = FSTAB_XX;
				typexx++;
				break;
			}
		}
		if (typexx)
			continue;

		return 1;
	}
}

struct fstab *
getfsent(void)
{
	if ((!_fs_fp && !setfsent()) || !fstabscan())
		return((struct fstab *)NULL);
	return(&_fs_fstab);
}

struct fstab *
getfsspec(const char *name)
{
	if (setfsent())
		while (fstabscan())
			if (!strcmp(_fs_fstab.fs_spec, name))
				return(&_fs_fstab);
	return((struct fstab *)NULL);
}

struct fstab *
getfsfile(const char *name)
{
	if (setfsent())
		while (fstabscan())
			if (!strcmp(_fs_fstab.fs_file, name))
				return(&_fs_fstab);
	return((struct fstab *)NULL);
}

int
setfsent(void)
{
	if (_fs_fp) {
		rewind(_fs_fp);
		return(1);
	}
	if ((_fs_fp = setmntent(_PATH_FSTAB, "r")))
		return(1);
	error(errno);
	return(0);
}

void
endfsent(void)
{
	if (_fs_fp) {
		(void)endmntent(_fs_fp);
		_fs_fp = NULL;
	}
}

static
void error(int err)
{
	char *p;

	(void)write(STDERR_FILENO, "fstab: ", 7);
	(void)write(STDERR_FILENO, _PATH_FSTAB, sizeof(_PATH_FSTAB) - 1);
	(void)write(STDERR_FILENO, ": ", 1);
	p = strerror(err);
	(void)write(STDERR_FILENO, p, strlen(p));
	(void)write(STDERR_FILENO, "\n", 1);
}
