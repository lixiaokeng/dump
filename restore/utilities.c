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
	"$Id: utilities.c,v 1.29 2005/07/07 09:16:08 stelian Exp $";
#endif /* not lint */

#include <config.h>
#include <compatlfs.h>
#include <sys/types.h>
#include <errno.h>
#include <compaterr.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/param.h>
#include <sys/stat.h>

#ifdef	__linux__
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#ifdef HAVE_EXT2FS_EXT2_FS_H
#include <ext2fs/ext2_fs.h>
#else
#include <linux/ext2_fs.h>
#endif
#include <ext2fs/ext2fs.h>
#include <bsdcompat.h>
#else	/* __linux__ */
#ifdef sunos
#include <sys/time.h>
#include <sys/fcntl.h>
#include <bsdcompat.h>
#else
#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#endif
#endif	/* __linux__ */
#ifdef DUMP_MACOSX
#include "darwin.h"
#endif
#include "restore.h"
#include "extern.h"

/*
 * Insure that all the components of a pathname exist.
 */
void
pathcheck(char *name)
{
	char *cp;
	struct entry *ep;
	char *start;

	start = strchr(name, '/');
	if (start == 0)
		return;
	for (cp = start; *cp != '\0'; cp++) {
		if (*cp != '/')
			continue;
		*cp = '\0';
		ep = lookupname(name);
		if (ep == NULL) {
			/* Safe; we know the pathname exists in the dump. */
			ep = addentry(name, pathsearch(name)->d_ino, NODE);
			newnode(ep);
		}
		ep->e_flags |= NEW|KEEP;
		*cp = '/';
	}
}

/*
 * Change a name to a unique temporary name.
 */
void
mktempname(struct entry *ep)
{
	char oldname[MAXPATHLEN];

	if (ep->e_flags & TMPNAME)
		badentry(ep, "mktempname: called with TMPNAME");
	ep->e_flags |= TMPNAME;
	(void) strcpy(oldname, myname(ep));
	freename(ep->e_name);
	ep->e_name = savename(gentempname(ep));
	ep->e_namlen = strlen(ep->e_name);
	renameit(oldname, myname(ep));
}

/*
 * Generate a temporary name for an entry.
 */
char *
gentempname(struct entry *ep)
{
	static char name[MAXPATHLEN];
	struct entry *np;
	long i = 0;

	for (np = lookupino(ep->e_ino);
	    np != NULL && np != ep; np = np->e_links)
		i++;
	if (np == NULL)
		badentry(ep, "not on ino list");
	(void) snprintf(name, sizeof(name), "%s%ld%lu", TMPHDR, i, (unsigned long)ep->e_ino);
	return (name);
}

/*
 * Rename a file or directory.
 */
void
renameit(char *from, char *to)
{
	if (!Nflag && rename(from, to) < 0) {
		warn("cannot rename %s to %s", from, to);
		return;
	}
	Vprintf(stdout, "rename %s to %s\n", from, to);
}

/*
 * Create a new node (directory).
 */
void
newnode(struct entry *np)
{
	char *cp;
	if (np->e_type != NODE)
		badentry(np, "newnode: not a node");
	cp = myname(np);
	if (command == 'C') return;

	if (!Nflag && mkdir(cp, 0777) < 0 && !uflag) {
		np->e_flags |= EXISTED;
		warn("%s", cp);
		return;
	}
	Vprintf(stdout, "Make node %s\n", cp);
}

/*
 * Remove an old node (directory).
 */
void
removenode(struct entry *ep)
{
	char *cp;

	if (ep->e_type != NODE)
		badentry(ep, "removenode: not a node");
	if (ep->e_entries != NULL) {
		int i;
		for (i = 0; i < dirhash_size; i++) {
			if (ep->e_entries[i] != NULL)
				badentry(ep, "removenode: non-empty directory");
		}
	}
	ep->e_flags |= REMOVED;
	ep->e_flags &= ~TMPNAME;
	cp = myname(ep);
	if (!Nflag && rmdir(cp) < 0) {
		warn("%s", cp);
		return;
	}
	Vprintf(stdout, "Remove node %s\n", cp);
}

/*
 * Remove a leaf.
 */
void
removeleaf(struct entry *ep)
{
	char *cp;

	if (command == 'C') return;

	if (ep->e_type != LEAF)
		badentry(ep, "removeleaf: not a leaf");
	ep->e_flags |= REMOVED;
	ep->e_flags &= ~TMPNAME;
	cp = myname(ep);
	if (!Nflag && unlink(cp) < 0) {
		warn("%s", cp);
		return;
	}
	Vprintf(stdout, "Remove leaf %s\n", cp);
}

/*
 * Create a link.
 */
int
linkit(char *existing, char *new, int type)
{

	/* if we want to unlink first, do it now so *link() won't fail */
	if (uflag && !Nflag)
		(void)unlink(new);

	if (type == SYMLINK) {
		if (!Nflag && symlink(existing, new) < 0) {
			warn("cannot create symbolic link %s->%s",
			    new, existing);
			return (FAIL);
		}
	} else if (type == HARDLINK) {
		int ret;

		if (!Nflag && (ret = link(existing, new)) < 0) {

#if !defined(__linux__) && !defined(sunos)
			struct stat s;

			/*
			 * Most likely, the schg flag is set.  Clear the
			 * flags and try again.
			 */
			if (stat(existing, &s) == 0 && s.st_flags != 0 &&
			    chflags(existing, 0) == 0) {
				ret = link(existing, new);
				chflags(existing, s.st_flags);
			}
#else
			unsigned long s;

			/*
			 * Most likely, the immutable or append-only attribute
			 * is set. Clear the attributes and try again.
			 */
#ifdef sunos
#else
			if (lgetflags (existing, &s) != -1 &&
			    lsetflags (existing, 0) != -1) {
			    	ret = link(existing, new);
				lsetflags(existing, s);
			}
#endif
#endif
			if (ret < 0) {
				warn("warning: cannot create hard link %s->%s",
				    new, existing);
				return (FAIL);
			}
		}
	} else {
		panic("linkit: unknown type %d\n", type);
		return (FAIL);
	}
	Vprintf(stdout, "Create %s link %s->%s\n",
		type == SYMLINK ? "symbolic" : "hard", new, existing);
	return (GOOD);
}

#if !defined(__linux__) && !defined(sunos)
/*
 * Create a whiteout.
 */
int
addwhiteout(char *name)
{

	if (!Nflag && mknod(name, S_IFWHT, 0) < 0) {
		warn("cannot create whiteout %s", name);
		return (FAIL);
	}
	Vprintf(stdout, "Create whiteout %s\n", name);
	return (GOOD);
}

/*
 * Delete a whiteout.
 */
void
delwhiteout(struct entry *ep)
{
	char *name;

	if (ep->e_type != LEAF)
		badentry(ep, "delwhiteout: not a leaf");
	ep->e_flags |= REMOVED;
	ep->e_flags &= ~TMPNAME;
	name = myname(ep);
	if (!Nflag && undelete(name) < 0) {
		warn("cannot delete whiteout %s", name);
		return;
	}
	Vprintf(stdout, "Delete whiteout %s\n", name);
}
#endif

/*
 * find lowest number file (above "start") that needs to be extracted
 */
dump_ino_t
lowerbnd(dump_ino_t start)
{
	struct entry *ep;

	for ( ; start < maxino; start++) {
		ep = lookupino(start);
		if (ep == NULL || ep->e_type == NODE)
			continue;
		if (ep->e_flags & (NEW|EXTRACT))
			return (start);
	}
	return (start);
}

/*
 * find highest number file (below "start") that needs to be extracted
 */
dump_ino_t
upperbnd(dump_ino_t start)
{
	struct entry *ep;

	for ( ; start > ROOTINO; start--) {
		ep = lookupino(start);
		if (ep == NULL || ep->e_type == NODE)
			continue;
		if (ep->e_flags & (NEW|EXTRACT))
			return (start);
	}
	return (start);
}

/*
 * report on a badly formed entry
 */
void
badentry(struct entry *ep, const char *msg)
{

	fprintf(stderr, "bad entry: %s\n", msg);
	fprintf(stderr, "name: %s\n", myname(ep));
	fprintf(stderr, "parent name %s\n", myname(ep->e_parent));
	if (ep->e_sibling != NULL)
		fprintf(stderr, "sibling name: %s\n", myname(ep->e_sibling));
	if (ep->e_entries != NULL) {
		int i;
		for (i = 0; i < dirhash_size; i++) {
			if (ep->e_entries[i] != NULL) {
				fprintf(stderr, "next entry name: %s\n", myname(ep->e_entries[i]));
				break;
			}
		}
	}
	if (ep->e_links != NULL)
		fprintf(stderr, "next link name: %s\n", myname(ep->e_links));
	if (ep->e_next != NULL)
		fprintf(stderr,
		    "next hashchain name: %s\n", myname(ep->e_next));
	fprintf(stderr, "entry type: %s\n",
		ep->e_type == NODE ? "NODE" : "LEAF");
	fprintf(stderr, "inode number: %lu\n", (unsigned long)ep->e_ino);
	panic("flags: %s\n", flagvalues(ep));
}

/*
 * Construct a string indicating the active flag bits of an entry.
 */
char *
flagvalues(struct entry *ep)
{
	static char flagbuf[BUFSIZ];

	(void) strcpy(flagbuf, "|NIL");
	flagbuf[0] = '\0';
	if (ep->e_flags & REMOVED)
		(void) strcat(flagbuf, "|REMOVED");
	if (ep->e_flags & TMPNAME)
		(void) strcat(flagbuf, "|TMPNAME");
	if (ep->e_flags & EXTRACT)
		(void) strcat(flagbuf, "|EXTRACT");
	if (ep->e_flags & NEW)
		(void) strcat(flagbuf, "|NEW");
	if (ep->e_flags & KEEP)
		(void) strcat(flagbuf, "|KEEP");
	if (ep->e_flags & EXISTED)
		(void) strcat(flagbuf, "|EXISTED");
	return (&flagbuf[1]);
}

/*
 * Check to see if a name is on a dump tape.
 */
dump_ino_t
dirlookup(const char *name)
{
	struct direct *dp;
	dump_ino_t ino;

	ino = ((dp = pathsearch(name)) == NULL) ? 0 : dp->d_ino;

	if (ino == 0 || TSTINO(ino, dumpmap) == 0)
		fprintf(stderr, "%s is not on the tape\n", name);
	return (ino);
}

/*
 * Elicit a reply.
 */
int
reply(const char *question)
{
	char c;

	do	{
		fprintf(stderr, "%s? [yn] ", question);
		(void) fflush(stderr);
		c = getc(terminal);
		while (c != '\n' && getc(terminal) != '\n')
			if (feof(terminal))
				return (FAIL);
	} while (c != 'y' && c != 'n');
	if (c == 'y')
		return (GOOD);
	return (FAIL);
}

/*
 * handle unexpected inconsistencies
 */
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

void
#ifdef __STDC__
panic(const char *fmt, ...)
#else
panic(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list ap;
#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif

	vfprintf(stderr, fmt, ap);
	if (yflag)
		return;
	if (reply("abort") == GOOD) {
		if (reply("dump core") == GOOD) {
			fchdir(wdfd);
			abort();
		}
		exit(1);
	}
}

void resizemaps(dump_ino_t oldmax, dump_ino_t newmax)
{
	char *map;

	if (usedinomap) {
		map = calloc((unsigned)1, (unsigned)howmany(newmax, NBBY));
		if (map == NULL)
			errx(1, "no memory for active inode map");
		memcpy(map, usedinomap, howmany(oldmax, NBBY));
		free(usedinomap);
		usedinomap = map;
	}
	if (dumpmap) {
		map = calloc((unsigned)1, (unsigned)howmany(newmax, NBBY));
		if (map == NULL)
			errx(1, "no memory for file dump list");
		memcpy(map, dumpmap, howmany(oldmax, NBBY));
		free(dumpmap);
		dumpmap = map;
	}
}

void
GetPathFile(char *source, char *path, char *fname)
{
	char *p, *s;

	*path = 0;
	*fname = 0;
	p = s = source;
	while (*s) {
		if (*s == '/') {
			p = s + 1;
		}
		s++;
	}
	if (p == source) {
		*path = 0;
	} else {
		strncpy(path, source, p - source);
		path[p - source] = 0;
	}
	strcpy(fname, p);
}

#ifdef USE_QFA
/*
 * search for ino in QFA file
 *
 * if exactmatch:
 * if ino found return tape number and tape position
 * if ino not found return tnum=0 and tpos=0
 *
 * if not exactmatch:
 * if ino found return tape number and tape position
 * if ino not found return tape number and tape position of last smaller ino
 * if no smaller inode found return tnum=0 and tpos=0
 */
int
Inode2Tapepos(dump_ino_t ino, long *tnum, long long *tpos, int exactmatch)
{
	char *p, *pp;
	char numbuff[32];
	unsigned long tmpino;
	long tmptnum;
	long long tmptpos;

	*tpos = 0;
	*tnum = 0;
	if (fseek(gTapeposfp, gSeekstart, SEEK_SET) == -1)
		return errno;
	while (1) {
		gSeekstart = ftell(gTapeposfp); /* remember for later use */
		if (fgets(gTps, sizeof(gTps), gTapeposfp) == NULL) {
			return 0;
		}
		gTps[strlen(gTps) - 1] = 0;	/* delete end of line */
		p = gTps;
		bzero(numbuff, sizeof(numbuff));
		pp = numbuff;
		/* read inode */
		while ((*p != 0) && (*p != '\t'))
			*pp++ = *p++;
		tmpino = atol(numbuff);
		if (*p == 0)
			return 1;	/* may NOT happen */    
		p++;
		bzero(numbuff, sizeof(numbuff));
		pp = numbuff;
		/* read tapenum */
		while ((*p != 0) && (*p != '\t'))
			*pp++ = *p++;
		if (*p == 0)
			return 1;	/* may NOT happen */    
		tmptnum = atol(numbuff);
		p++;
		bzero(numbuff, sizeof(numbuff));
		pp = numbuff;
		/* read tapepos */
		while ((*p != 0) && (*p != '\t'))
			*pp++ = *p++;
		tmptpos = atoll(numbuff);

		if (exactmatch) {
			if (tmpino == ino)  {
				*tnum = tmptnum;
				*tpos = tmptpos;
				return 0;
			}
		} else {
			if (tmpino > ino)  {
				return 0;
			} else {
				*tnum = tmptnum;
				*tpos = tmptpos;
			}
		}
	}
	return 0;
}

#ifdef sunos
int
GetSCSIIDFromPath(char *devPath, long *id)
{
	int	len;
	char	fbuff[2048];
	char	path[2048];
	char	fname[2048];
	char	*fpn = fname;
	char	idstr[32];
	char	*ip = idstr;

	bzero(fbuff, sizeof(fbuff));
	if ((len = readlink(devPath, fbuff, 2048)) == -1) {
		return errno;
	}
	fbuff[len] = 0;
	GetPathFile(fbuff, path, fname);
	(void)memset(idstr, 0, sizeof(idstr));
	while (*fpn && (*fpn != ',')) {
		if (*fpn <= '9' && *fpn >= '0') {
			*ip = *fpn;
			ip++;
		}
		fpn++;
	}
	if (*idstr) {
		*id = atol(idstr);
	} else {
		*id = -1;
	}
	return 0;
}
#endif
#endif /* USE_QFA */

#ifdef DUMP_MACOSX
int
CreateAppleDoubleFileRes(char *oFile, FndrFileInfo *finderinfo, mode_t mode, int flags,
		struct timeval *timep, u_int32_t uid, u_int32_t gid)
{
	int		err = 0;
	int		fdout;
	char		*p;
	char		*pp;
	ASDHeaderPtr	hp;
	ASDEntryPtr	ep;
	long		thesize;
	long		n;


	n = 1;	/* number of entries in double file ._ only finderinfo */
	/*
	no data fork
	n++;
	currently no resource fork
	n++;
	*/

	thesize = sizeof(ASDHeader) + (n * sizeof(ASDEntry)) + INFOLEN;
	if ((pp = p = (char *)malloc(thesize)) == NULL) {
		err = errno;
		return err;
	}

	hp = (ASDHeaderPtr)p;
	p += sizeof(ASDHeader);
	ep = (ASDEntryPtr)p;
	p += sizeof(ASDEntry) * n;

	hp->magic = ADOUBLE_MAGIC;
	hp->version = ASD_VERSION2;

	bzero(&hp->filler, sizeof(hp->filler));
	hp->entries = (short)n;
	
	ep->entryID = EntryFinderInfo;
	ep->offset = p - pp - CORRECT;
	ep->len = INFOLEN; /*  sizeof(MacFInfo) + sizeof(FXInfo); */
	bzero(p, ep->len);
	bcopy(finderinfo, p, sizeof(FndrFileInfo));
	p += ep->len;
	ep++;

	if ((fdout = open(oFile, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0) {
		err = errno;
		free(pp);
		return err;
	}

	/* write the ASDHeader */
	if (write(fdout, pp, sizeof(ASDHeader) - CORRECT) == -1) {
		err = errno;
		close(fdout);
		free(pp);
		unlink(oFile);
		return err;
	}
	/* write the ASDEntries */
	if (write(fdout, pp + sizeof(ASDHeader), thesize - sizeof(ASDHeader)) == -1) {
		err = errno;
		close(fdout);
		free(pp);
		unlink(oFile);
		return err;
	}

	(void)fchown(fdout, uid, gid);
	(void)fchmod(fdout, mode);
	close(fdout);
	(void)lsetflags(oFile, flags);
	utimes(oFile, timep);
	free(pp);
	return err;
}
#endif /* DUMP_MACOSX */

int
lgetflags(const char *path, unsigned long *flags)
{
	int err;
	struct STAT sb;

	err = LSTAT(path, &sb);
	if (err < 0)
		return err;

	if (S_ISLNK(sb.st_mode) || S_ISFIFO(sb.st_mode)) {
		// no way to get/set flags on a symlink
		*flags = 0;
		return 0;
	}
	else
		return fgetflags(path, flags);
}

int
lsetflags(const char *path, unsigned long flags)
{
	int err;
	struct STAT sb;

	err = LSTAT(path, &sb);
	if (err < 0)
		return err;

	if (S_ISLNK(sb.st_mode) || S_ISFIFO(sb.st_mode)) {
		// no way to get/set flags on a symlink
		return 0;
	}
	else
		return fsetflags(path, flags);
}
