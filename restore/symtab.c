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

/*
 * These routines maintain the symbol table which tracks the state
 * of the file system being restored. They provide lookup by either
 * name or inode number. They also provide for creation, deletion,
 * and renaming of entries. Because of the dynamic nature of pathnames,
 * names should not be saved, but always constructed just before they
 * are needed, by calling "myname".
 */

#include <config.h>
#include <sys/param.h>
#include <sys/stat.h>

#ifdef	__linux__
#include <sys/time.h>
#include <time.h>
#ifdef HAVE_EXT2FS_EXT2_FS_H
#include <ext2fs/ext2_fs.h>
#else
#include <linux/ext2_fs.h>
#endif
#include <bsdcompat.h>
#else	/* __linux__ */
#ifdef sunos
#include <sys/fcntl.h>
#include <bsdcompat.h>
#else
#include <ufs/ufs/dinode.h>
#endif
#endif	/* __linux__ */

#include <errno.h>
#include <compaterr.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef	__linux__
#include <ext2fs/ext2fs.h>
#endif

#include "restore.h"
#include "extern.h"

/*
 * The following variables define the inode symbol table.
 * The primary hash table is dynamically allocated based on
 * the number of inodes in the file system (maxino), scaled by
 * HASHFACTOR. The variable "entry" points to the hash table;
 * the variable "entrytblsize" indicates its size (in entries).
 */
#define HASHFACTOR 5
static struct entry **entry;
static long entrytblsize;

static void		 addino (dump_ino_t, struct entry *);
static struct entry	*lookupparent (char *);
static void		 removeentry (struct entry *);
static int		 dir_hash (char *);

/*
 * Returns a hash given a file name
 */
static int
dir_hash(char *name)
{
	unsigned long hash0 = 0x12a3fe2d, hash1 = 0x37abe8f9;
	int len = strlen(name);

	while (len--) {
		unsigned long hash = hash1 + (hash0 ^ (*name++ * 7152373));
		if (hash & 0x80000000) hash -= 0x7fffffff;
		hash1 = hash0;
		hash0 = hash;
	}
	return hash0 % dirhash_size;
}

/*
 * Look up an entry by inode number
 */
struct entry *
lookupino(dump_ino_t inum)
{
	struct entry *ep;

	if (inum < WINO || inum >= maxino)
		return (NULL);
	for (ep = entry[inum % entrytblsize]; ep != NULL; ep = ep->e_next)
		if (ep->e_ino == inum)
			return (ep);
	return (NULL);
}

/*
 * Add an entry into the entry table
 */
static void
addino(dump_ino_t inum, struct entry *np)
{
	struct entry **epp;

	if (inum < WINO || inum >= maxino)
		panic("addino: out of range %d\n", inum);
	epp = &entry[inum % entrytblsize];
	np->e_ino = inum;
	np->e_next = *epp;
	*epp = np;
	if (dflag)
		for (np = np->e_next; np != NULL; np = np->e_next)
			if (np->e_ino == inum)
				badentry(np, "duplicate inum");
}

/*
 * Delete an entry from the entry table
 */
void
deleteino(dump_ino_t inum)
{
	struct entry *next;
	struct entry **prev;

	if (inum < WINO || inum >= maxino)
		panic("deleteino: out of range %d\n", inum);
	prev = &entry[inum % entrytblsize];
	for (next = *prev; next != NULL; next = next->e_next) {
		if (next->e_ino == inum) {
			next->e_ino = 0;
			*prev = next->e_next;
			return;
		}
		prev = &next->e_next;
	}
	panic("deleteino: %d not found\n", inum);
}

/*
 * Look up an entry by name
 */
struct entry *
lookupname(char *name)
{
	struct entry *ep, *oldep;
	char *np, *cp;
	char buf[MAXPATHLEN];

	ep = lookupino(ROOTINO);

	cp = name;
	if (*cp == '.')
		++cp;
	if (*cp == '/')
		++cp;
	if (*cp == '\0')
		return ep;

	while (ep != NULL) {
		for (np = buf; *cp != '/' && *cp != '\0' &&
				np < &buf[sizeof(buf)]; )
			*np++ = *cp++;
		if (np == &buf[sizeof(buf)])
			break;
		*np = '\0';

		oldep = ep;

		if (ep->e_entries != NULL) {

			ep = ep->e_entries[dir_hash(buf)];
			for ( ; ep != NULL; ep = ep->e_sibling)
				if (strcmp(ep->e_name, buf) == 0)
					break;

			/* search all hash lists for renamed inodes */
			if (ep == NULL) {
				int j;
				for (j = 0; j < dirhash_size; j++) {
					ep = oldep->e_entries[j];
					for ( ; ep != NULL; ep = ep->e_sibling)
						if (strcmp(ep->e_name, buf) == 0)
							break;
					if (ep != NULL)
						break;
				}
			}
		}

		if (ep == NULL)
			break;
		if (*cp++ == '\0')
			return (ep);
	}
	return (NULL);
}

/*
 * Look up the parent of a pathname
 */
static struct entry *
lookupparent(char *name)
{
	struct entry *ep;
	char *tailindex;

	tailindex = strrchr(name, '/');
	if (tailindex == NULL)
		return (NULL);
	*tailindex = '\0';
	ep = lookupname(name);
	*tailindex = '/';
	if (ep == NULL)
		return (NULL);
	if (ep->e_type != NODE)
		panic("%s is not a directory\n", name);
	return (ep);
}

/*
 * Determine the current pathname of a node or leaf
 */
char *
myname(struct entry *ep)
{
	char *cp;
	static char namebuf[MAXPATHLEN];

	for (cp = &namebuf[MAXPATHLEN - 2]; cp > &namebuf[ep->e_namlen]; ) {
		cp -= ep->e_namlen;
		memmove(cp, ep->e_name, (size_t)ep->e_namlen);
		if (ep == lookupino(ROOTINO))
			return (cp);
		*(--cp) = '/';
		ep = ep->e_parent;
	}
	panic("%s: pathname too long\n", cp);
	return(cp);
}

/*
 * Unused symbol table entries are linked together on a free list
 * headed by the following pointer.
 */
static struct entry *freelist = NULL;

/*
 * add an entry to the symbol table
 */
struct entry *
addentry(char *name, dump_ino_t inum, int type)
{
	struct entry *np, *ep;

	if (freelist != NULL) {
		np = freelist;
		freelist = np->e_next;
		memset(np, 0, sizeof(struct entry));
	} else {
		np = (struct entry *)calloc(1, sizeof(struct entry));
		if (np == NULL)
			errx(1, "no memory to extend symbol table");
	}
	np->e_type = type & ~LINK;
	if (type & NODE) {
		np->e_entries = calloc(1, dirhash_size * sizeof(struct entry *));
		if (np->e_entries == NULL)
			panic("unable to allocate directory entries\n");
	}
	ep = lookupparent(name);
	if (ep == NULL) {
		if (inum != ROOTINO || lookupino(ROOTINO) != NULL)
			panic("bad name to addentry %s\n", name);
		np->e_name = savename(name);
		np->e_namlen = strlen(name);
		np->e_parent = np;
		addino(ROOTINO, np);
		return (np);
	}
	np->e_name = savename(strrchr(name, '/') + 1);
	np->e_namlen = strlen(np->e_name);
	np->e_parent = ep;
	np->e_sibling = ep->e_entries[dir_hash(np->e_name)];
	ep->e_entries[dir_hash(np->e_name)] = np;
	if (type & LINK) {
		ep = lookupino(inum);
		if (ep == NULL)
			panic("link to non-existent name\n");
		np->e_ino = inum;
		np->e_links = ep->e_links;
		ep->e_links = np;
	} else if (inum != 0) {
		if (lookupino(inum) != NULL)
			panic("duplicate entry\n");
		addino(inum, np);
	}
	return (np);
}

/*
 * delete an entry from the symbol table
 */
void
freeentry(struct entry *ep)
{
	struct entry *np;
	dump_ino_t inum;

	if (ep->e_flags != REMOVED)
		badentry(ep, "not marked REMOVED");
	if (ep->e_type == NODE) {
		if (ep->e_links != NULL)
			badentry(ep, "freeing referenced directory");
		if (ep->e_entries != NULL) {
			int i;
			for (i = 0; i < dirhash_size; i++) {
				if (ep->e_entries[i] != NULL)
					badentry(ep, "freeing non-empty directory");
			}
		}
	}
	if (ep->e_ino != 0) {
		np = lookupino(ep->e_ino);
		if (np == NULL)
			badentry(ep, "lookupino failed");
		if (np == ep) {
			inum = ep->e_ino;
			deleteino(inum);
			if (ep->e_links != NULL)
				addino(inum, ep->e_links);
		} else {
			for (; np != NULL; np = np->e_links) {
				if (np->e_links == ep) {
					np->e_links = ep->e_links;
					break;
				}
			}
			if (np == NULL)
				badentry(ep, "link not found");
		}
	}
	removeentry(ep);
	freename(ep->e_name);
	ep->e_next = freelist;
	freelist = ep;
}

/*
 * Relocate an entry in the tree structure
 */
void
moveentry(struct entry *ep, char *newname)
{
	struct entry *np;
	char *cp;

	np = lookupparent(newname);
	if (np == NULL)
		badentry(ep, "cannot move ROOT");
	if (np != ep->e_parent) {
		removeentry(ep);
		ep->e_parent = np;
		ep->e_sibling = np->e_entries[dir_hash(ep->e_name)];
		np->e_entries[dir_hash(ep->e_name)] = ep;
	}
	cp = strrchr(newname, '/') + 1;
	freename(ep->e_name);
	ep->e_name = savename(cp);
	ep->e_namlen = strlen(cp);
	if (strcmp(gentempname(ep), ep->e_name) == 0)
		ep->e_flags |= TMPNAME;
	else
		ep->e_flags &= ~TMPNAME;
}

/*
 * Remove an entry in the tree structure
 */
static void
removeentry(struct entry *ep)
{
	struct entry *np = ep->e_parent;
	struct entry *entry = np->e_entries[dir_hash(ep->e_name)];

	if (entry == ep) {
		np->e_entries[dir_hash(ep->e_name)] = ep->e_sibling;
	} else {
		for (np = entry; np != NULL; np = np->e_sibling) {
			if (np->e_sibling == ep) {
				np->e_sibling = ep->e_sibling;
				break;
			}
		}
		if (np == NULL) {

			/* search all hash lists for renamed inodes */
			int j;
			for (j = 0; j < dirhash_size; j++) {
				np = ep->e_parent;
				entry = np->e_entries[j];
				if (entry == ep) {
					np->e_entries[j] = ep->e_sibling;
					return;
				}
				else {
					for (np = entry; np != NULL; np = np->e_sibling) {
						if (np->e_sibling == ep) {
							np->e_sibling = ep->e_sibling;
							return;
						}
					}
				}
			}
			badentry(ep, "cannot find entry in parent list");
		}
	}
}

/*
 * Table of unused string entries, sorted by length.
 *
 * Entries are allocated in STRTBLINCR sized pieces so that names
 * of similar lengths can use the same entry. The value of STRTBLINCR
 * is chosen so that every entry has at least enough space to hold
 * a "struct strtbl" header. Thus every entry can be linked onto an
 * appropriate free list.
 *
 * NB. The macro "allocsize" below assumes that "struct strhdr"
 *     has a size that is a power of two.
 */
struct strhdr {
	struct strhdr *next;
};

#define STRTBLINCR	(sizeof(struct strhdr))
#define allocsize(size)	(((size) + 1 + STRTBLINCR - 1) & ~(STRTBLINCR - 1))

static struct strhdr strtblhdr[allocsize(MAXNAMLEN) / STRTBLINCR];

/*
 * Allocate space for a name. It first looks to see if it already
 * has an appropriate sized entry, and if not allocates a new one.
 */
char *
savename(char *name)
{
	struct strhdr *np;
	long len;
	char *cp;

	if (name == NULL)
		panic("bad name\n");
	len = strlen(name);
	np = strtblhdr[len / STRTBLINCR].next;
	if (np != NULL) {
		strtblhdr[len / STRTBLINCR].next = np->next;
		cp = (char *)np;
	} else {
		cp = malloc((unsigned)allocsize(len));
		if (cp == NULL)
			errx(1, "no space for string table");
	}
	(void) strcpy(cp, name);
	return (cp);
}

/*
 * Free space for a name. The resulting entry is linked onto the
 * appropriate free list.
 */
void
freename(char *name)
{
	struct strhdr *tp, *np;

	tp = &strtblhdr[strlen(name) / STRTBLINCR];
	np = (struct strhdr *)name;
	np->next = tp->next;
	tp->next = np;
}

/*
 * Useful quantities placed at the end of a dumped symbol table.
 */
struct symtableheader {
	int32_t	volno;
	int32_t	stringsize;
	int32_t hashsize;
	int32_t	entrytblsize;
	time_t	dumptime;
	time_t	dumpdate;
	dump_ino_t maxino;
	int32_t	ntrec;
	int32_t zflag;
};

/*
 * dump a snapshot of the symbol table
 */
void
dumpsymtable(char *filename, long checkpt)
{
	struct entry *ep, *tep;
	dump_ino_t i;
	struct entry temp, *tentry;
	long mynum = 1, stroff = 0, hashoff = 0;
	FILE *fd;
	struct symtableheader hdr;
	struct entry **temphash;

	Vprintf(stdout, "Check pointing the restore\n");
	if (Nflag)
		return;
	if ((fd = fopen(filename, "w")) == NULL) {
		warn("fopen");
		panic("cannot create save file %s for symbol table\n",
			filename);
	}
	clearerr(fd);
	/*
	 * Assign indices to each entry
	 * Write out the string entries
	 */
	for (i = WINO; i <= maxino; i++) {
		for (ep = lookupino(i); ep != NULL; ep = ep->e_links) {
			ep->e_index = mynum++;
			(void) fwrite(ep->e_name, (int)allocsize(ep->e_namlen),
				      sizeof(char), fd);
		}
	}
	/*
	 * Write out e_entries tables
	 */
	temphash = calloc(1, dirhash_size * sizeof(struct entry *));
	if (temphash == NULL)
		errx(1, "no memory for saving hashtable");
	for (i = WINO; i <= maxino; i++) {
		for (ep = lookupino(i); ep != NULL; ep = ep->e_links) {
			if (ep->e_entries != NULL) {
				int j;
				memcpy(temphash, ep->e_entries, dirhash_size * sizeof(struct entry *));
				for (j = 0; j < dirhash_size; j++) {
					if (temphash[j])
						temphash[j] = (struct entry *)ep->e_entries[j]->e_index;
				}
				fwrite(temphash, sizeof(struct entry *), dirhash_size, fd);
			}
		}
	}
	free(temphash);

	/*
	 * Convert pointers to indexes, and output
	 */
	tep = &temp;
	stroff = 0;
	hashoff = 0;
	for (i = WINO; i <= maxino; i++) {
		for (ep = lookupino(i); ep != NULL; ep = ep->e_links) {
			memmove(tep, ep, sizeof(struct entry));
			tep->e_name = (char *)stroff;
			stroff += allocsize(ep->e_namlen);
			tep->e_parent = (struct entry *)ep->e_parent->e_index;
			if (ep->e_links != NULL)
				tep->e_links =
					(struct entry *)ep->e_links->e_index;
			if (ep->e_sibling != NULL)
				tep->e_sibling =
					(struct entry *)ep->e_sibling->e_index;
			if (ep->e_entries != NULL) {
				tep->e_entries = (struct entry **)hashoff;
				hashoff += dirhash_size * sizeof(struct entry *);
			}
			if (ep->e_next != NULL)
				tep->e_next =
					(struct entry *)ep->e_next->e_index;
			(void) fwrite((char *)tep, sizeof(struct entry), 1, fd);
		}
	}
	/*
	 * Convert entry pointers to indexes, and output
	 */
	for (i = 0; (long)i < entrytblsize; i++) {
		if (entry[i] == NULL)
			tentry = NULL;
		else
			tentry = (struct entry *)entry[i]->e_index;
		(void) fwrite((char *)&tentry, sizeof(struct entry *), 1, fd);
	}
	hdr.volno = checkpt;
	hdr.maxino = maxino;
	hdr.entrytblsize = entrytblsize;
	hdr.stringsize = stroff;
	hdr.hashsize = hashoff;
	hdr.dumptime = dumptime;
	hdr.dumpdate = dumpdate;
	hdr.ntrec = ntrec;
	hdr.zflag = zflag;
	(void) fwrite((char *)&hdr, sizeof(struct symtableheader), 1, fd);
	if (ferror(fd)) {
		warn("fwrite");
		panic("output error to file %s writing symbol table\n",
			filename);
	}
	(void) fclose(fd);
}

/*
 * Initialize a symbol table from a file
 */
void
initsymtable(char *filename)
{
	char *base;
	long tblsize;
	struct entry *ep;
	struct entry *baseep, *lep;
	struct symtableheader hdr;
	struct stat stbuf;
	long i;
	int fd;

	Vprintf(stdout, "Initialize symbol table.\n");
	if (filename == NULL) {
		entrytblsize = maxino / HASHFACTOR;
		entry = (struct entry **)
			calloc((unsigned)entrytblsize, sizeof(struct entry *));
		if (entry == (struct entry **)NULL)
			errx(1, "no memory for entry table");
		ep = addentry(".", ROOTINO, NODE);
		ep->e_flags |= NEW;
		return;
	}
	if ((fd = open(filename, O_RDONLY, 0)) < 0) {
		warn("open");
		errx(1, "cannot open symbol table file %s", filename);
	}
	if (fstat(fd, &stbuf) < 0) {
		warn("stat");
		errx(1, "cannot stat symbol table file %s", filename);
	}
	tblsize = stbuf.st_size - sizeof(struct symtableheader);
	base = calloc(sizeof(char), (unsigned)tblsize);
	if (base == NULL)
		errx(1, "cannot allocate space for symbol table");
	if (read(fd, base, (int)tblsize) < 0 ||
	    read(fd, (char *)&hdr, sizeof(struct symtableheader)) < 0) {
		warn("read");
		errx(1, "cannot read symbol table file %s", filename);
	}
	switch (command) {
	case 'r':
		/*
		 * For normal continuation, insure that we are using
		 * the next incremental tape
		 */
		if (hdr.dumpdate != dumptime)
			errx(1, "Incremental tape too %s",
				(hdr.dumpdate < dumptime) ? "low" : "high");
		break;
	case 'R':
		/*
		 * For restart, insure that we are using the same tape
		 */
		curfile.action = SKIP;
		dumptime = hdr.dumptime;
		dumpdate = hdr.dumpdate;
		zflag = hdr.zflag;
		if (!bflag)
			newtapebuf(hdr.ntrec);
		getvol(hdr.volno);
		break;
	default:
		panic("initsymtable called from command %c\n", command);
		break;
	}
	if (hdr.maxino > maxino) {
		resizemaps(maxino, hdr.maxino);
		maxino = hdr.maxino;
	}
	entrytblsize = hdr.entrytblsize;
	entry = (struct entry **)
		(base + tblsize - (entrytblsize * sizeof(struct entry *)));
	baseep = (struct entry *)(base + hdr.stringsize + hdr.hashsize - sizeof(struct entry));
	lep = (struct entry *)entry;
	for (i = 0; i < entrytblsize; i++) {
		if (entry[i] == NULL)
			continue;
		entry[i] = &baseep[(long)entry[i]];
	}
	for (ep = &baseep[1]; ep < lep; ep++) {
		ep->e_name = base + (long)ep->e_name;
		ep->e_parent = &baseep[(long)ep->e_parent];
		if (ep->e_sibling != NULL)
			ep->e_sibling = &baseep[(long)ep->e_sibling];
		if (ep->e_links != NULL)
			ep->e_links = &baseep[(long)ep->e_links];
		if (ep->e_type == NODE) {
			int i;
			ep->e_entries = (struct entry **)(base + hdr.stringsize + (long)ep->e_entries);
			for (i = 0; i < dirhash_size; i++) {
				if (ep->e_entries[i])
					ep->e_entries[i] = &baseep[(long)ep->e_entries[i]];
			}
		}
		else
			ep->e_entries = NULL;
		if (ep->e_next != NULL)
			ep->e_next = &baseep[(long)ep->e_next];
	}
}
