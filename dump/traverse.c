/*
 *	Ported to Linux's Second Extended File System as part of the
 *	dump and restore backup suit
 *	Remy Card <card@Linux.EU.Org>, 1994-1997
 *      Stelian Pop <pop@cybercable.fr>, 1999 
 *
 */

/*-
 * Copyright (c) 1980, 1988, 1991, 1993
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
#if 0
static char sccsid[] = "@(#)traverse.c	8.7 (Berkeley) 6/15/95";
#endif
static const char rcsid[] =
	"$Id: traverse.c,v 1.3 1999/10/11 12:59:19 stelian Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#ifdef	__linux__
#include <linux/ext2_fs.h>
#include <bsdcompat.h>
#include <compaterr.h>
#include <stdlib.h>
#define swab32(x) ext2fs_swab32(x)
#else	/* __linux__ */
#define swab32(x) x
#ifdef sunos
#include <sys/vnode.h>

#include <ufs/fs.h>
#include <ufs/fsdir.h>
#include <ufs/inode.h>
#else
#include <ufs/ufs/dir.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>
#endif
#endif	/* __linux__ */

#include <protocols/dumprestore.h>

#include <ctype.h>
#include <stdio.h>
#ifdef __STDC__
#include <string.h>
#include <unistd.h>
#endif

#ifdef	__linux__
#include <ext2fs/ext2fs.h>
#endif

#include "dump.h"

#define	HASDUMPEDFILE	0x1
#define	HASSUBDIRS	0x2

#ifdef	FS_44INODEFMT
typedef	quad_t fsizeT;
#else
typedef	long fsizeT;
#endif

#ifdef	__linux__
static	int searchdir __P((struct ext2_dir_entry *dp, int offset,
			   int blocksize, char *buf, void *private));
loff_t llseek (int fd, loff_t offset, int origin);
#else
static	int dirindir __P((ino_t ino, daddr_t blkno, int level, long *size));
static	void dmpindir __P((ino_t ino, daddr_t blk, int level, fsizeT *size));
static	int searchdir __P((ino_t ino, daddr_t blkno, long size, long filesize));
#endif

/*
 * This is an estimation of the number of TP_BSIZE blocks in the file.
 * It estimates the number of blocks in files with holes by assuming
 * that all of the blocks accounted for by di_blocks are data blocks
 * (when some of the blocks are usually used for indirect pointers);
 * hence the estimate may be high.
 */
long
blockest(struct dinode *dp)
{
	long blkest, sizeest;

	/*
	 * dp->di_size is the size of the file in bytes.
	 * dp->di_blocks stores the number of sectors actually in the file.
	 * If there are more sectors than the size would indicate, this just
	 *	means that there are indirect blocks in the file or unused
	 *	sectors in the last file block; we can safely ignore these
	 *	(blkest = sizeest below).
	 * If the file is bigger than the number of sectors would indicate,
	 *	then the file has holes in it.	In this case we must use the
	 *	block count to estimate the number of data blocks used, but
	 *	we use the actual size for estimating the number of indirect
	 *	dump blocks (sizeest vs. blkest in the indirect block
	 *	calculation).
	 */
	blkest = howmany(dbtob(dp->di_blocks), TP_BSIZE);
	sizeest = howmany(dp->di_size, TP_BSIZE);
	if (blkest > sizeest)
		blkest = sizeest;
#ifdef	__linux__
	if (dp->di_size > fs->blocksize * NDADDR) {
		/* calculate the number of indirect blocks on the dump tape */
		blkest +=
			howmany(sizeest - NDADDR * fs->blocksize / TP_BSIZE,
			TP_NINDIR);
	}
#else
	if (dp->di_size > sblock->fs_bsize * NDADDR) {
		/* calculate the number of indirect blocks on the dump tape */
		blkest +=
			howmany(sizeest - NDADDR * sblock->fs_bsize / TP_BSIZE,
			TP_NINDIR);
	}
#endif
	return (blkest + 1);
}

/* Auxiliary macro to pick up files changed since previous dump. */
#define	CHANGEDSINCE(dp, t) \
	((dp)->di_mtime >= (t) || (dp)->di_ctime >= (t))

/* The WANTTODUMP macro decides whether a file should be dumped. */
#ifdef UF_NODUMP
#define	WANTTODUMP(dp) \
	(CHANGEDSINCE(dp, spcl.c_ddate) && \
	 (nonodump || ((dp)->di_flags & UF_NODUMP) != UF_NODUMP))
#else
#define	WANTTODUMP(dp) CHANGEDSINCE(dp, spcl.c_ddate)
#endif

/*
 * Dump pass 1.
 *
 * Walk the inode list for a filesystem to find all allocated inodes
 * that have been modified since the previous dump time. Also, find all
 * the directories in the filesystem.
 */
int
mapfiles(ino_t maxino, long *tapesize)
{
	register int mode;
	register ino_t ino;
	register struct dinode *dp;
	int anydirskipped = 0;

	for (ino = ROOTINO; ino < maxino; ino++) {
		dp = getino(ino);
		if ((mode = (dp->di_mode & IFMT)) == 0)
			continue;
#ifdef	__linux__
		if (dp->di_nlink == 0 || dp->di_dtime != 0)
			continue;
#endif
		SETINO(ino, usedinomap);
		if (mode == IFDIR)
			SETINO(ino, dumpdirmap);
		if (WANTTODUMP(dp)) {
			SETINO(ino, dumpinomap);
			if (mode != IFREG && mode != IFDIR && mode != IFLNK)
				*tapesize += 1;
			else
				*tapesize += blockest(dp);
			continue;
		}
		if (mode == IFDIR)
			anydirskipped = 1;
	}
	/*
	 * Restore gets very upset if the root is not dumped,
	 * so ensure that it always is dumped.
	 */
	SETINO(ROOTINO, dumpinomap);
	return (anydirskipped);
}

#ifdef	__linux__
struct mapfile_context {
	long *tapesize;
	int anydirskipped;
};

static int
mapfilesindir(struct ext2_dir_entry *dirent, int offset, int blocksize, char *buf, void *private)
{
	register struct dinode *dp;
	register int mode;
	ino_t ino;
	errcode_t retval;
	struct mapfile_context *mfc;

	ino = dirent->inode;
	mfc = (struct mapfile_context *)private;
	dp = getino(ino);
	if ((mode = (dp->di_mode & IFMT)) != 0 &&
	    dp->di_nlink != 0 && dp->di_dtime == 0) {
		SETINO(ino, usedinomap);
		if (mode == IFDIR)
			SETINO(ino, dumpdirmap);
		if (WANTTODUMP(dp)) {
			SETINO(ino, dumpinomap);
			if (mode != IFREG && mode != IFDIR && mode != IFLNK)
				*mfc->tapesize += 1;
			else
				*mfc->tapesize += blockest(dp);
		}
		if (mode == IFDIR) {
			mfc->anydirskipped = 1;
			if ((dirent->name[0] != '.' || dirent->name_len != 1) &&
			    (dirent->name[0] != '.' || dirent->name[1] != '.' ||
			     dirent->name_len != 2)) {
			retval = ext2fs_dir_iterate(fs, ino, 0, NULL,
						    mapfilesindir, private);
			if (retval)
				return retval;
			}
		}
	}
	return 0;
}

/*
 * Dump pass 1.
 *
 * Walk the inode list for a filesystem to find all allocated inodes
 * that have been modified since the previous dump time. Also, find all
 * the directories in the filesystem.
 */
int
mapfilesfromdir(ino_t maxino, long *tapesize, char *directory)
{
	errcode_t retval;
	struct mapfile_context mfc;
	ino_t dir_ino;
	char dir_name [MAXPATHLEN];
	int i;

	/*
	 * Mark every directory in the path as being dumped
	 */
	for (i = 0; i < strlen (directory); i++) {
		if (directory[i] == '/') {
			strncpy (dir_name, directory, i);
			dir_name[i] = '\0';
			retval = ext2fs_namei(fs, ROOTINO, ROOTINO, dir_name,
					      &dir_ino);
			if (retval) {
				com_err(disk, retval, "while translating %s",
					dir_name);
				exit(X_ABORT);
			}
/*			SETINO(dir_ino, dumpinomap); */
			SETINO(dir_ino, dumpdirmap);
		}
	}
	/*
	 * Mark the final directory
	 */
	retval = ext2fs_namei(fs, ROOTINO, ROOTINO, directory, &dir_ino);
	if (retval) {
		com_err(disk, retval, "while translating %s", directory);
		exit(X_ABORT);
	}
/*	SETINO(dir_ino, dumpinomap); */
	SETINO(dir_ino, dumpdirmap);

	mfc.tapesize = tapesize;
	mfc.anydirskipped = 0;
	retval = ext2fs_dir_iterate(fs, dir_ino, 0, NULL, mapfilesindir,
				    (void *)&mfc);

	if (retval) {
		com_err(disk, retval, "while mapping files in %s", directory);
		exit(X_ABORT);
	}
	/*
	 * Restore gets very upset if the root is not dumped,
	 * so ensure that it always is dumped.
	 */
/*	SETINO(ROOTINO, dumpinomap); */
	SETINO(ROOTINO, dumpdirmap);
	return (mfc.anydirskipped);
}
#endif

/*
 * Dump pass 2.
 *
 * Scan each directory on the filesystem to see if it has any modified
 * files in it. If it does, and has not already been added to the dump
 * list (because it was itself modified), then add it. If a directory
 * has not been modified itself, contains no modified files and has no
 * subdirectories, then it can be deleted from the dump list and from
 * the list of directories. By deleting it from the list of directories,
 * its parent may now qualify for the same treatment on this or a later
 * pass using this algorithm.
 */
int
mapdirs(ino_t maxino, long *tapesize)
{
	register struct	dinode *dp;
	register int isdir;
	register char *map;
	register ino_t ino;
#ifndef __linux
	register int i;
	long filesize;
#endif
	int ret, change = 0;

	isdir = 0;		/* XXX just to get gcc to shut up */
	for (map = dumpdirmap, ino = 1; ino < maxino; ino++) {
		if (((ino - 1) % NBBY) == 0)	/* map is offset by 1 */
			isdir = *map++;
		else
			isdir >>= 1;
		if ((isdir & 1) == 0 || TSTINO(ino, dumpinomap))
			continue;
		dp = getino(ino);
#ifdef	__linux__
		ret = 0;
		ext2fs_dir_iterate(fs, ino, 0, NULL, searchdir, (void *) &ret);
#else	/* __linux__ */
		filesize = dp->di_size;
		for (ret = 0, i = 0; filesize > 0 && i < NDADDR; i++) {
			if (dp->di_db[i] != 0)
				ret |= searchdir(ino, dp->di_db[i],
					(long)dblksize(sblock, dp, i),
					filesize);
			if (ret & HASDUMPEDFILE)
				filesize = 0;
			else
				filesize -= sblock->fs_bsize;
		}
		for (i = 0; filesize > 0 && i < NIADDR; i++) {
			if (dp->di_ib[i] == 0)
				continue;
			ret |= dirindir(ino, dp->di_ib[i], i, &filesize);
		}
#endif	/* __linux__ */
		if (ret & HASDUMPEDFILE) {
			SETINO(ino, dumpinomap);
			*tapesize += blockest(dp);
			change = 1;
			continue;
		}
		if ((ret & HASSUBDIRS) == 0) {
			if (!TSTINO(ino, dumpinomap)) {
				CLRINO(ino, dumpdirmap);
				change = 1;
			}
		}
	}
	return (change);
}

#ifndef	__linux__
/*
 * Read indirect blocks, and pass the data blocks to be searched
 * as directories. Quit as soon as any entry is found that will
 * require the directory to be dumped.
 */
static int
dirindir(ino_t ino, daddr_t blkno, int ind_level, long *filesize)
{
	int ret = 0;
	register int i;
	daddr_t	idblk[MAXNINDIR];

	bread(fsbtodb(sblock, blkno), (char *)idblk, (int)sblock->fs_bsize);
	if (ind_level <= 0) {
		for (i = 0; *filesize > 0 && i < NINDIR(sblock); i++) {
			blkno = idblk[i];
			if (blkno != 0)
				ret |= searchdir(ino, blkno, sblock->fs_bsize,
					*filesize);
			if (ret & HASDUMPEDFILE)
				*filesize = 0;
			else
				*filesize -= sblock->fs_bsize;
		}
		return (ret);
	}
	ind_level--;
	for (i = 0; *filesize > 0 && i < NINDIR(sblock); i++) {
		blkno = idblk[i];
		if (blkno != 0)
			ret |= dirindir(ino, blkno, ind_level, filesize);
	}
	return (ret);
}
#endif	/* !__linux__ */

/*
 * Scan a disk block containing directory information looking to see if
 * any of the entries are on the dump list and to see if the directory
 * contains any subdirectories.
 */
#ifdef	__linux__
static	int
searchdir(struct ext2_dir_entry *dp, int offset, int blocksize, char *buf, void *private)
{
	int *ret = (int *) private;

	if (dp->inode == 0)
		return 0;
	if (dp->name[0] == '.') {
		if (dp->name_len == 1)
			return 0;
		if (dp->name[1] == '.' && dp->name_len == 2)
			return 0;
	}
	if (TSTINO(dp->inode, dumpinomap)) {
		*ret |= HASDUMPEDFILE;
		if (*ret & HASSUBDIRS)
			return DIRENT_ABORT;
	}
	if (TSTINO(dp->inode, dumpdirmap)) {
		*ret |= HASSUBDIRS;
		if (*ret & HASDUMPEDFILE)
			return DIRENT_ABORT;
	}
	return 0;
}

#else	/* __linux__ */

static int
searchdir(ino_t ino, daddr_t blkno, long size, long filesize)
{
	register struct direct *dp;
	register long loc, ret = 0;
	char dblk[MAXBSIZE];

	bread(fsbtodb(sblock, blkno), dblk, (int)size);
	if (filesize < size)
		size = filesize;
	for (loc = 0; loc < size; ) {
		dp = (struct direct *)(dblk + loc);
		if (dp->d_reclen == 0) {
			msg("corrupted directory, inumber %d\n", ino);
			break;
		}
		loc += dp->d_reclen;
		if (dp->d_ino == 0)
			continue;
		if (dp->d_name[0] == '.') {
			if (dp->d_name[1] == '\0')
				continue;
			if (dp->d_name[1] == '.' && dp->d_name[2] == '\0')
				continue;
		}
		if (TSTINO(dp->d_ino, dumpinomap)) {
			ret |= HASDUMPEDFILE;
			if (ret & HASSUBDIRS)
				break;
		}
		if (TSTINO(dp->d_ino, dumpdirmap)) {
			ret |= HASSUBDIRS;
			if (ret & HASDUMPEDFILE)
				break;
		}
	}
	return (ret);
}
#endif	/* __linux__ */

#ifdef	__linux__

struct block_context {
	ino_t	ino;
	int	*buf;
	int	cnt;
	int	max;
	int	next_block;
};

/*
 * Dump a block to the tape
 */
static int
dumponeblock(ext2_filsys fs, blk_t *blocknr, int blockcnt, void *private)
{
	struct block_context *p;
	int i;

	if (blockcnt < NDADDR)
		return 0;
	p = (struct block_context *)private;
	for (i = p->next_block; i < blockcnt; i++) {
		p->buf[p->cnt++] = 0;
		if (p->cnt == p->max) {
			blksout (p->buf, p->cnt, p->ino);
			p->cnt = 0;
		}
	}
	p->buf[p->cnt++] = *blocknr;
	if (p->cnt == p->max) {
		blksout (p->buf, p->cnt, p->ino);
		p->cnt = 0;
	}
	p->next_block = blockcnt + 1;
	return 0;
}
#endif

/*
 * Dump passes 3 and 4.
 *
 * Dump the contents of an inode to tape.
 */
void
dumpino(struct dinode *dp, ino_t ino)
{
	int cnt;
	fsizeT size;
	char buf[TP_BSIZE];
	struct old_bsd_inode obi;
#ifdef	__linux__
	struct block_context bc;
#else
	int ind_level;
#endif

	if (newtape) {
		newtape = 0;
		dumpmap(dumpinomap, TS_BITS, ino);
	}
	CLRINO(ino, dumpinomap);
#ifdef	__linux__
	memset(&obi, 0, sizeof(obi));
	obi.di_mode = dp->di_mode;
	obi.di_uid = dp->di_uid;
	obi.di_gid = dp->di_gid;
	obi.di_qsize.v = (u_quad_t)dp->di_size;
	obi.di_atime = dp->di_atime;
	obi.di_mtime = dp->di_mtime;
	obi.di_ctime = dp->di_ctime;
	obi.di_nlink = dp->di_nlink;
	obi.di_blocks = dp->di_blocks;
	obi.di_flags = dp->di_flags;
	obi.di_gen = dp->di_gen;
	memmove(&obi.di_db, &dp->di_db, (NDADDR + NIADDR) * sizeof(daddr_t));
	if (dp->di_file_acl || dp->di_dir_acl)
		warn("ACLs in inode #%ld won't be dumped", ino);
	memmove(&spcl.c_dinode, &obi, sizeof(obi));
#else	/* __linux__ */
	spcl.c_dinode = *dp;
#endif	/* __linux__ */
	spcl.c_type = TS_INODE;
	spcl.c_count = 0;
	switch (dp->di_mode & S_IFMT) {

	case 0:
		/*
		 * Freed inode.
		 */
		return;

#ifdef	__linux__
	case S_IFDIR:
		msg("Warning: dumpino called on a directory (ino %d)\n", ino);
		return;
#endif

	case S_IFLNK:
		/*
		 * Check for short symbolic link.
		 */
#ifdef	__linux__
		if (dp->di_size > 0 &&
		    dp->di_size < EXT2_N_BLOCKS * sizeof (daddr_t)) {
			spcl.c_addr[0] = 1;
			spcl.c_count = 1;
			writeheader(ino);
			memmove(buf, dp->di_db, (u_long)dp->di_size);
			buf[dp->di_size] = '\0';
			writerec(buf, 0);
			return;
		}
#endif	/* __linux__ */
#ifdef FS_44INODEFMT
		if (dp->di_size > 0 &&
		    dp->di_size < sblock->fs_maxsymlinklen) {
			spcl.c_addr[0] = 1;
			spcl.c_count = 1;
			writeheader(ino);
			memmove(buf, dp->di_shortlink, (u_long)dp->di_size);
			buf[dp->di_size] = '\0';
			writerec(buf, 0);
			return;
		}
#endif
		/* fall through */

#ifndef	__linux__
	case S_IFDIR:
#endif
	case S_IFREG:
		if (dp->di_size > 0)
			break;
		/* fall through */

	case S_IFIFO:
	case S_IFSOCK:
	case S_IFCHR:
	case S_IFBLK:
		writeheader(ino);
		return;

	default:
		msg("Warning: undefined file type 0%o\n", dp->di_mode & IFMT);
		return;
	}
	if (dp->di_size > NDADDR * sblock->fs_bsize)
#ifdef	__linux__
		cnt = NDADDR * EXT2_FRAGS_PER_BLOCK(fs->super);
#else
		cnt = NDADDR * sblock->fs_frag;
#endif
	else
		cnt = howmany(dp->di_size, sblock->fs_fsize);
	blksout(&dp->di_db[0], cnt, ino);
	if ((size = dp->di_size - NDADDR * sblock->fs_bsize) <= 0)
		return;
#ifdef	__linux__
	bc.max = NINDIR(sblock) * EXT2_FRAGS_PER_BLOCK(fs->super);
	bc.buf = (int *)malloc (bc.max * sizeof (int));
	bc.cnt = 0;
	bc.ino = ino;
	bc.next_block = NDADDR;

	ext2fs_block_iterate (fs, ino, 0, NULL, dumponeblock, (void *)&bc);
	if (bc.cnt > 0) {
		blksout (bc.buf, bc.cnt, bc.ino);
	}
#else
	for (ind_level = 0; ind_level < NIADDR; ind_level++) {
		dmpindir(ino, dp->di_ib[ind_level], ind_level, &size);
		if (size <= 0)
			return;
	}
#endif
}

#ifdef	__linux__

struct convert_dir_context {
	char *buf;
	int prev_offset;
	int offset;
	int bs;
};

/*
 * This function converts an ext2fs directory entry to the BSD format.
 *
 * Basically, it adds a null-character at the end of the name, recomputes the
 * size of the entry, and creates it in a temporary buffer
 */
static int
convert_dir(struct ext2_dir_entry *dirent, int offset, int blocksize, char *buf, void *private)
{
	struct convert_dir_context *p;
	struct direct *dp;
	int reclen;

	p = (struct convert_dir_context *)private;

	reclen = EXT2_DIR_REC_LEN(dirent->name_len + 1);
	if (((p->offset + reclen - 1) / p->bs) != (p->offset / p->bs)) {
		dp = (struct direct *)(p->buf + p->prev_offset);
		dp->d_reclen += p->bs - (p->offset % p->bs);
		p->offset += p->bs - (p->offset % p->bs);
	}

	dp = (struct direct *)(p->buf + p->offset);
	dp->d_ino = dirent->inode;
	dp->d_reclen = reclen;
	dp->d_type = 0;
	dp->d_namlen = dirent->name_len;
	strncpy(dp->d_name, dirent->name, dirent->name_len);
	dp->d_name[dp->d_namlen] = '\0';
	p->prev_offset = p->offset;
	p->offset += reclen;

	return 0;
}

/*
 * Dump pass 3
 *
 * Dumps a directory to tape after converting it to the BSD format
 */
void
dumpdirino(struct dinode *dp, ino_t ino)
{
	fsizeT size;
	char buf[TP_BSIZE];
	struct old_bsd_inode obi;
	struct convert_dir_context cdc;
	errcode_t retval;
	struct ext2_dir_entry *de;
	fsizeT dir_size;

	if (newtape) {
		newtape = 0;
		dumpmap(dumpinomap, TS_BITS, ino);
	}
	CLRINO(ino, dumpinomap);

	/*
	 * Convert the directory to the BSD format
	 */
	/* Allocate a buffer for the conversion (twice the size of the
	   ext2fs directory to avoid problems ;-) */
	cdc.buf = (char *)malloc(dp->di_size * 2 * sizeof(char));
	if (cdc.buf == NULL)
		err(1, "Cannot allocate buffer to convert directory #%lu\n",
		    (unsigned long)ino);
	cdc.offset = 0;
	cdc.prev_offset = 0;
	cdc.bs = MIN(DIRBLKSIZ, TP_BSIZE);
	/* Do the conversion */
	retval = ext2fs_dir_iterate(fs, ino, 0, NULL, convert_dir, (void *)&cdc);
	if (retval) {
		com_err(disk, retval, "while converting directory #%ld\n", ino);
		exit(X_ABORT);
	}
	/* Fix the last entry */
	if ((cdc.offset % cdc.bs) != 0) {
		de = (struct ext2_dir_entry *)(cdc.buf + cdc.prev_offset);
		de->rec_len += cdc.bs - (cdc.offset % cdc.bs);
		cdc.offset += cdc.bs - (cdc.offset % cdc.bs);
	}

	dir_size = cdc.offset;

#ifdef	__linux__
	memset(&obi, 0, sizeof(obi));
	obi.di_mode = dp->di_mode;
	obi.di_uid = dp->di_uid;
	obi.di_gid = dp->di_gid;
	obi.di_qsize.v = dir_size; /* (u_quad_t)dp->di_size; */
	obi.di_atime = dp->di_atime;
	obi.di_mtime = dp->di_mtime;
	obi.di_ctime = dp->di_ctime;
	obi.di_nlink = dp->di_nlink;
	obi.di_blocks = dp->di_blocks;
	obi.di_flags = dp->di_flags;
	obi.di_gen = dp->di_gen;
	memmove(&obi.di_db, &dp->di_db, (NDADDR + NIADDR) * sizeof(daddr_t));
	if (dp->di_file_acl || dp->di_dir_acl)
		warn("ACLs in inode #%ld won't be dumped", ino);
	memmove(&spcl.c_dinode, &obi, sizeof(obi));
#else	/* __linux__ */
	spcl.c_dinode = *dp;
#endif	/* __linux__ */
	spcl.c_type = TS_INODE;
	spcl.c_count = 0;
	switch (dp->di_mode & S_IFMT) {

	case 0:
		/*
		 * Freed inode.
		 */
		return;

	case S_IFDIR:
		if (dir_size > 0)
			break;
		msg("Warning: size of directory inode #%d is <= 0 (%d)!\n",
			ino, dir_size);
		return;

	default:
		msg("Warning: dumpdirino called with file type 0%o (inode #%d)\n",
			dp->di_mode & IFMT, ino);
		return;
	}
	for (size = 0; size < dir_size; size += TP_BSIZE) {
		spcl.c_addr[0] = 1;
		spcl.c_count = 1;
		writeheader(ino);
		memmove(buf, cdc.buf + size, TP_BSIZE);
		writerec(buf, 0);
		spcl.c_type = TS_ADDR;
	}

	(void)free(cdc.buf);
}
#endif	/* __linux__ */

#ifndef	__linux__
/*
 * Read indirect blocks, and pass the data blocks to be dumped.
 */
static void
dmpindir(ino_t ino, daddr_t blk, int ind_level, fsizeT *size)
{
	int i, cnt;
#ifdef __linux__
	int max;
	blk_t *swapme;
#endif
	daddr_t idblk[MAXNINDIR];

	if (blk != 0) {
		bread(fsbtodb(sblock, blk), (char *)idblk, (int) sblock->fs_bsize);
#ifdef __linux__
	/* 
	 * My RedHat 4.0 system doesn't have these flags; I haven't
	 * upgraded e2fsprogs yet
	 */
#if defined(EXT2_FLAG_SWAP_BYTES)
	if ((fs->flags & EXT2_FLAG_SWAP_BYTES) ||
	    (fs->flags & EXT2_FLAG_SWAP_BYTES_READ)) {
#endif
		max = sblock->fs_bsize >> 2;
		swapme = (blk_t *) idblk;
		for (i = 0; i < max; i++, swapme++)
			*swapme = swab32(*swapme);
#if defined(EXT2_FLAG_SWAP_BYTES)
	}
#endif
#endif
	else
		memset(idblk, 0, (int)sblock->fs_bsize);
	if (ind_level <= 0) {
		if (*size < NINDIR(sblock) * sblock->fs_bsize)
			cnt = howmany(*size, sblock->fs_fsize);
		else
#ifdef	__linux__
			cnt = NINDIR(sblock) * EXT2_FRAGS_PER_BLOCK(fs->super);
#else
			cnt = NINDIR(sblock) * sblock->fs_frag;
#endif
		*size -= NINDIR(sblock) * sblock->fs_bsize;
		blksout(&idblk[0], cnt, ino);
		return;
	}
	ind_level--;
	for (i = 0; i < NINDIR(sblock); i++) {
		dmpindir(ino, idblk[i], ind_level, size);
		if (*size <= 0)
			return;
	}
}
#endif

/*
 * Collect up the data into tape record sized buffers and output them.
 */
void
blksout(daddr_t *blkp, int frags, ino_t ino)
{
	register daddr_t *bp;
	int i, j, count, blks, tbperdb;

	blks = howmany(frags * sblock->fs_fsize, TP_BSIZE);
	tbperdb = sblock->fs_bsize >> tp_bshift;
	for (i = 0; i < blks; i += TP_NINDIR) {
		if (i + TP_NINDIR > blks)
			count = blks;
		else
			count = i + TP_NINDIR;
		for (j = i; j < count; j++)
			if (blkp[j / tbperdb] != 0)
				spcl.c_addr[j - i] = 1;
			else
				spcl.c_addr[j - i] = 0;
		spcl.c_count = count - i;
		writeheader(ino);
		bp = &blkp[i / tbperdb];
		for (j = i; j < count; j += tbperdb, bp++) {
			if (*bp != 0) {
				if (j + tbperdb <= count)
					dumpblock(*bp, (int)sblock->fs_bsize);
				else
					dumpblock(*bp, (count - j) * TP_BSIZE);
			}
		}
		spcl.c_type = TS_ADDR;
	}
}

/*
 * Dump a map to the tape.
 */
void
dumpmap(char *map, int type, ino_t ino)
{
	register int i;
	char *cp;

	spcl.c_type = type;
	spcl.c_count = howmany(mapsize * sizeof(char), TP_BSIZE);
	writeheader(ino);
	for (i = 0, cp = map; i < spcl.c_count; i++, cp += TP_BSIZE)
		writerec(cp, 0);
}

/*
 * Write a header record to the dump tape.
 */
void
writeheader(ino_t ino)
{
#ifdef	__linux__
	register __s32 sum, cnt, *lp;
#else
	register int32_t sum, cnt, *lp;
#endif

	spcl.c_inumber = ino;
	spcl.c_magic = NFS_MAGIC;
	spcl.c_checksum = 0;
#ifdef	__linux__
	lp = (__s32 *)&spcl;
#else
	lp = (int32_t *)&spcl;
#endif
	sum = 0;
#ifdef	__linux__
	cnt = sizeof(union u_spcl) / (4 * sizeof(__s32));
#else
	cnt = sizeof(union u_spcl) / (4 * sizeof(int32_t));
#endif
	while (--cnt >= 0) {
		sum += *lp++;
		sum += *lp++;
		sum += *lp++;
		sum += *lp++;
	}
	spcl.c_checksum = CHECKSUM - sum;
	writerec((char *)&spcl, 1);
}

#ifdef	__linux__
struct dinode *
getino(ino_t inum)
{
	static struct dinode dinode;

	curino = inum;
	ext2fs_read_inode(fs, inum, (struct ext2_inode *) &dinode);
	return &dinode;
}
#else	/* __linux__ */
struct dinode *
getino(ino_t inum)
{
	static daddr_t minino, maxino;
	static struct dinode inoblock[MAXINOPB];

	curino = inum;
	if (inum >= minino && inum < maxino)
		return (&inoblock[inum - minino]);
	bread(fsbtodb(sblock, ino_to_fsba(sblock, inum)), (char *)inoblock,
	    (int)sblock->fs_bsize);
	minino = inum - (inum % INOPB(sblock));
	maxino = minino + INOPB(sblock);
	return (&inoblock[inum - minino]);
}
#endif	/* __linux__ */

/*
 * Read a chunk of data from the disk.
 * Try to recover from hard errors by reading in sector sized pieces.
 * Error recovery is attempted at most BREADEMAX times before seeking
 * consent from the operator to continue.
 */
int	breaderrors = 0;
#define	BREADEMAX 32

void
bread(daddr_t blkno, char *buf, int size)
{
	int cnt, i;
	extern int errno;

loop:
#ifdef	__linux__
	if (llseek(diskfd, ((ext2_loff_t)blkno << dev_bshift), 0) !=
			((ext2_loff_t)blkno << dev_bshift))
#else
	if (lseek(diskfd, ((off_t)blkno << dev_bshift), 0) !=
						((off_t)blkno << dev_bshift))
#endif
		msg("bread: lseek fails\n");
	if ((cnt = read(diskfd, buf, size)) == size)
		return;
	if (blkno + (size / dev_bsize) > fsbtodb(sblock, sblock->fs_size)) {
		/*
		 * Trying to read the final fragment.
		 *
		 * NB - dump only works in TP_BSIZE blocks, hence
		 * rounds `dev_bsize' fragments up to TP_BSIZE pieces.
		 * It should be smarter about not actually trying to
		 * read more than it can get, but for the time being
		 * we punt and scale back the read only when it gets
		 * us into trouble. (mkm 9/25/83)
		 */
		size -= dev_bsize;
		goto loop;
	}
	if (cnt == -1)
		msg("read error from %s: %s: [block %d]: count=%d\n",
			disk, strerror(errno), blkno, size);
	else
		msg("short read error from %s: [block %d]: count=%d, got=%d\n",
			disk, blkno, size, cnt);
	if (++breaderrors > BREADEMAX) {
		msg("More than %d block read errors from %d\n",
			BREADEMAX, disk);
		broadcast("DUMP IS AILING!\n");
		msg("This is an unrecoverable error.\n");
		if (!query("Do you want to attempt to continue?")){
			dumpabort(0);
			/*NOTREACHED*/
		} else
			breaderrors = 0;
	}
	/*
	 * Zero buffer, then try to read each sector of buffer separately.
	 */
	memset(buf, 0, size);
	for (i = 0; i < size; i += dev_bsize, buf += dev_bsize, blkno++) {
#ifdef	__linux__
		if (llseek(diskfd, ((ext2_loff_t)blkno << dev_bshift), 0) !=
				((ext2_loff_t)blkno << dev_bshift))
#else
		if (lseek(diskfd, ((off_t)blkno << dev_bshift), 0) !=
						((off_t)blkno << dev_bshift))
#endif
			msg("bread: lseek2 fails!\n");
		if ((cnt = read(diskfd, buf, (int)dev_bsize)) == dev_bsize)
			continue;
		if (cnt == -1) {
			msg("read error from %s: %s: [sector %d]: count=%d\n",
				disk, strerror(errno), blkno, dev_bsize);
			continue;
		}
		msg("short read error from %s: [sector %d]: count=%d, got=%d\n",
			disk, blkno, dev_bsize, cnt);
	}
}
