/*
 *	Ported to Linux's Second Extended File System as part of the
 *	dump and restore backup suit
 *	Remy Card <card@Linux.EU.Org>, 1994-1997
 *	Stelian Pop <stelian@popies.net>, 1999-2000
 *	Stelian Pop <stelian@popies.net> - Alcôve <www.alcove.com>, 2000-2002
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

#include <config.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/types.h>
#ifdef __STDC__
#include <string.h>
#include <unistd.h>
#endif
#include <errno.h>

#include <sys/param.h>
#include <sys/stat.h>
#ifdef	__linux__
#ifdef HAVE_EXT2FS_EXT2_FS_H
#include <ext2fs/ext2_fs.h>
#else
#include <linux/ext2_fs.h>
#endif
#include <ext2fs/ext2fs.h>
#include <bsdcompat.h>
#include <compaterr.h>
#include <stdlib.h>
#elif defined sunos
#include <sys/vnode.h>

#include <ufs/fs.h>
#include <ufs/fsdir.h>
#include <ufs/inode.h>
#else
#include <ufs/ufs/dir.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>
#endif	/* __linux__ */

#include <protocols/dumprestore.h>

#include "dump.h"
#include "indexer.h"

#define	HASDUMPEDFILE	0x1
#define	HASSUBDIRS	0x2

#ifdef	__linux__
static	int searchdir (struct ext2_dir_entry *dp, int offset,
			   int blocksize, char *buf, void *private);
#else
static	int dirindir (dump_ino_t ino, daddr_t blkno, int level, long *size);
static	void dmpindir (dump_ino_t ino, daddr_t blk, int level, uint64_t *size);
static	int searchdir (dump_ino_t ino, daddr_t blkno, long size, long filesize);
#endif
static	void mapfileino (dump_ino_t ino, struct dinode const *dp, long long *tapesize, int *dirskipped);
static void dump_xattr (dump_ino_t ino, struct dinode *dp);

#ifdef HAVE_EXT2_JOURNAL_INUM
#define ext2_journal_ino(sb) (sb->s_journal_inum)
#else
#define ext2_journal_ino(sb) (*((u_int32_t *)sb + 0x38))
#endif
#ifndef HAVE_EXT2_INO_T
typedef ino_t ext2_ino_t;
#endif

#ifndef EXT3_FEATURE_COMPAT_HAS_JOURNAL
#define EXT3_FEATURE_COMPAT_HAS_JOURNAL		0x0004
#endif
#ifndef EXT2_FEATURE_INCOMPAT_FILETYPE
#define EXT2_FEATURE_INCOMPAT_FILETYPE		0x0002
#endif
#ifndef EXT3_FEATURE_INCOMPAT_RECOVER
#define EXT3_FEATURE_INCOMPAT_RECOVER		0x0004
#endif
#ifndef EXT3_FEATURE_INCOMPAT_JOURNAL_DEV
#define EXT3_FEATURE_INCOMPAT_JOURNAL_DEV	0x0008
#endif

#ifndef EXT2_LIB_FEATURE_INCOMPAT_SUPP
#define EXT2_LIB_FEATURE_INCOMPAT_SUPP (EXT3_FEATURE_INCOMPAT_RECOVER | \
					EXT2_FEATURE_INCOMPAT_FILETYPE)
#endif
#ifndef EXT2_RESIZE_INO
#define EXT2_RESIZE_INO			7
#endif
#ifndef EXT2_FT_UNKNOWN
#define EXT2_FT_UNKNOWN		0
#define EXT2_FT_REG_FILE 	1
#define EXT2_FT_DIR		2
#define EXT2_FT_CHRDEV		3
#define EXT2_FT_BLKDEV		4
#define EXT2_FT_FIFO		5
#define EXT2_FT_SOCK		6
#define EXT2_FT_SYMLINK		7
#define EXT2_FT_MAX		8
#endif

int dump_fs_open(const char *disk, ext2_filsys *fs)
{
	int retval;

	retval = ext2fs_open(disk, EXT2_FLAG_FORCE, 0, 0, unix_io_manager, fs);
	if (!retval) {
		struct ext2_super_block *es = (*fs)->super;
		dump_ino_t journal_ino = ext2_journal_ino(es);

		if (es->s_feature_incompat & EXT3_FEATURE_INCOMPAT_JOURNAL_DEV){
			msg("This is a journal, not a filesystem!\n");
			retval = EXT2_ET_UNSUPP_FEATURE;
			ext2fs_close(*fs);
		}
		else if ((retval = es->s_feature_incompat &
					~(EXT2_LIB_FEATURE_INCOMPAT_SUPP |
					  EXT3_FEATURE_INCOMPAT_RECOVER))) {
			msg("Unsupported feature(s) 0x%x in filesystem\n",
				retval);
			retval = EXT2_ET_UNSUPP_FEATURE;
			ext2fs_close(*fs);
		}
		else {
			if (es->s_feature_compat &
				EXT3_FEATURE_COMPAT_HAS_JOURNAL && 
				journal_ino)
				do_exclude_ino(journal_ino, "journal inode");
			do_exclude_ino(EXT2_RESIZE_INO, "resize inode");
		}
	}
	return retval;
}

/*
 * This is an estimation of the number of TP_BSIZE blocks in the file.
 * It estimates the number of blocks in files with holes by assuming
 * that all of the blocks accounted for by di_blocks are data blocks
 * (when some of the blocks are usually used for indirect pointers);
 * hence the estimate may be high.
 */
long
blockest(struct dinode const *dp)
{
	long blkest, sizeest;
	uint64_t i_size;

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
	blkest = howmany((uint64_t)dp->di_blocks * 512, fs->blocksize) * (fs->blocksize / TP_BSIZE);
	i_size = dp->di_size + ((uint64_t) dp->di_size_high << 32);
	sizeest = howmany(i_size, fs->blocksize) * (fs->blocksize / TP_BSIZE);
	if (blkest > sizeest)
		blkest = sizeest;
#ifdef	__linux__
	if ((dp->di_mode & IFMT) == IFDIR) {
		/*
		 * for directories, assume only half of space is filled
		 * with entries.  
		 */
		 blkest = blkest / 2;
		 sizeest = sizeest / 2;
	}
	if (i_size > (uint64_t)fs->blocksize * NDADDR) {
		/* calculate the number of indirect blocks on the dump tape */
		blkest +=
			howmany(sizeest - NDADDR * fs->blocksize / TP_BSIZE,
			TP_NINDIR);
	}
#else
	if (i_size > sblock->fs_bsize * NDADDR) {
		/* calculate the number of indirect blocks on the dump tape */
		blkest +=
			howmany(sizeest - NDADDR * sblock->fs_bsize / TP_BSIZE,
			TP_NINDIR);
	}
#endif
	return (blkest + 1);
}

/* Auxiliary macro to pick up files changed since previous dump. */
#define CSINCE(dp, t) \
	((dp)->di_ctime >= (t))
#define MSINCE(dp, t) \
	((dp)->di_mtime >= (t))
#define	CHANGEDSINCE(dp, t) \
	CSINCE(dp, t)

/* The NODUMP_FLAG macro tests if a file has the nodump flag. */
#ifdef UF_NODUMP
#define NODUMP_FLAG(dp) (!nonodump && (((dp)->di_flags & UF_NODUMP) == UF_NODUMP))
#else
#define NODUMP_FLAG(dp) 0
#endif

/* The WANTTODUMP macro decides whether a file should be dumped. */
#define	WANTTODUMP(dp, ino) \
	(CHANGEDSINCE(dp, ((u_int32_t)spcl.c_ddate)) && \
	 (!NODUMP_FLAG(dp)) && \
	 (!exclude_ino(ino)))

/*
 * Determine if given inode should be dumped. "dp" must either point to a
 * copy of the given inode, or be NULL (in which case it is fetched.)
 */
static void
mapfileino(dump_ino_t ino, struct dinode const *dp, long long *tapesize, int *dirskipped)
{
	int mode;

	/*
	 * Skip inode if we've already marked it for dumping
	 */
	if (TSTINO(ino, usedinomap))
		return;
	if (!dp)
		dp = getino(ino);
	if ((mode = (dp->di_mode & IFMT)) == 0)
		return;
#ifdef	__linux__
	if (dp->di_nlink == 0 || dp->di_dtime != 0)
		return;
#endif
	/*
	 * Put all dirs in dumpdirmap, inodes that are to be dumped in the
	 * used map. All inode but dirs who have the nodump attribute go
	 * to the usedinomap.
	 */
	SETINO(ino, usedinomap);

	if (NODUMP_FLAG(dp))
		do_exclude_ino(ino, "nodump attribute");

	if (mode == IFDIR)
		SETINO(ino, dumpdirmap);
	if (WANTTODUMP(dp, ino)) {
		SETINO(ino, dumpinomap);
		if (!MSINCE(dp, (u_int32_t)spcl.c_ddate))
			SETINO(ino, metainomap);
		if (mode != IFREG && mode != IFDIR && mode != IFLNK)
			*tapesize += 1;
		else
			*tapesize += blockest(dp);
		return;
	}
	if (mode == IFDIR) {
		if (exclude_ino(ino))
			CLRINO(ino, usedinomap);
		*dirskipped = 1;
	}
}

/*
 * Dump pass 1.
 *
 * Walk the inode list for a filesystem to find all allocated inodes
 * that have been modified since the previous dump time. Also, find all
 * the directories in the filesystem.
 */
#ifdef __linux__
int
mapfiles(UNUSED(dump_ino_t maxino), long long *tapesize)
{
	ext2_ino_t ino;
	int anydirskipped = 0;
	ext2_inode_scan scan;
	errcode_t err;
	struct ext2_inode inode;

	/*
	 * We use libext2fs's inode scanning routines, which are particularly
	 * robust.  (Note that getino cannot return an error.)
	 */
	err = ext2fs_open_inode_scan(fs, 0, &scan);
	if (err) {
		com_err(disk, err, "while opening inodes\n");
		exit(X_ABORT);
	}
	for (;;) {
		err = ext2fs_get_next_inode(scan, &ino, &inode);
		if (err == EXT2_ET_BAD_BLOCK_IN_INODE_TABLE)
			continue;
		if (err) {
			com_err(disk, err, "while scanning inode #%ld\n",
				(long)ino);
			exit(X_ABORT);
		}
		if (ino == 0)
			break;

		curino = ino;
		mapfileino(ino, (struct dinode const *)&inode, tapesize,
			   &anydirskipped);
	}
	ext2fs_close_inode_scan(scan);

	/*
	 * Restore gets very upset if the root is not dumped,
	 * so ensure that it always is dumped.
	 */
	SETINO(ROOTINO, dumpinomap);
	return (anydirskipped);
}
#else
int
mapfiles(dump_ino_t maxino, long long *tapesize)
{
	dump_ino_t ino;
	int anydirskipped = 0;

	for (ino = ROOTINO; ino < maxino; ino++)
		mapfileino(ino, tapesize, &anydirskipped);

	/*
	 * Restore gets very upset if the root is not dumped,
	 * so ensure that it always is dumped.
	 */
	SETINO(ROOTINO, dumpinomap);
	return (anydirskipped);
}
#endif /* __linux__ */

#ifdef __linux__
int
maponefile(UNUSED(dump_ino_t maxino), long long *tapesize, char *directory)
{
	errcode_t retval;
	ext2_ino_t dir_ino;
	char dir_name [MAXPATHLEN];
	int i, anydirskipped = 0;

	/*
	 * Mark every directory in the path as being dumped
	 */
	for (i = 0; i < (int)strlen (directory); i++) {
		if (directory[i] == '/') {
			strncpy (dir_name, directory, i);
			dir_name[i] = '\0';
			retval = ext2fs_namei(fs, ROOTINO, ROOTINO, 
					      dir_name, &dir_ino);
			if (retval) {
				com_err(disk, retval, 
					"while translating %s", dir_name);
				exit(X_ABORT);
			}
			mapfileino((dump_ino_t) dir_ino, 0,
				   tapesize, &anydirskipped);
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
	mapfileino((dump_ino_t)dir_ino, 0, tapesize, &anydirskipped);

	mapfileino(ROOTINO, 0, tapesize, &anydirskipped);

	/*
	 * Restore gets very upset if the root is not dumped,
	 * so ensure that it always is dumped.
	 */
	SETINO(ROOTINO, dumpdirmap);
	return anydirskipped;
}
#endif /* __linux__ */

#ifdef	__linux__
struct mapfile_context {
	long long *tapesize;
	int *anydirskipped;
};

static int
mapfilesindir(struct ext2_dir_entry *dirent, UNUSED(int offset), 
	      UNUSED(int blocksize), UNUSED(char *buf), void *private)
{
	struct dinode const *dp;
	int mode;
	errcode_t retval;
	struct mapfile_context *mfc;
	ext2_ino_t ino;

	ino = dirent->inode;
	mfc = (struct mapfile_context *)private;
	dp = getino(dirent->inode);

	mapfileino(dirent->inode, dp, mfc->tapesize, mfc->anydirskipped);

	mode = dp->di_mode & IFMT;
	if (mode == IFDIR && dp->di_nlink != 0 && dp->di_dtime == 0) {
		if ((dirent->name[0] != '.' || ( dirent->name_len & 0xFF ) != 1) &&
		    (dirent->name[0] != '.' || dirent->name[1] != '.' ||
		     ( dirent->name_len & 0xFF ) != 2)) {
		retval = ext2fs_dir_iterate(fs, ino, 0, NULL,
					    mapfilesindir, private);
		if (retval)
			return retval;
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
mapfilesfromdir(UNUSED(dump_ino_t maxino), long long *tapesize, char *directory)
{
	errcode_t retval;
	struct mapfile_context mfc;
	ext2_ino_t dir_ino;
	char dir_name [MAXPATHLEN];
	int i, anydirskipped = 0;

	/*
	 * Mark every directory in the path as being dumped
	 */
	for (i = 0; i < (int)strlen (directory); i++) {
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
			mapfileino(dir_ino, 0, tapesize, &anydirskipped);
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
	mapfileino(dir_ino, 0, tapesize, &anydirskipped);

	mfc.tapesize = tapesize;
	mfc.anydirskipped = &anydirskipped;
	retval = ext2fs_dir_iterate(fs, dir_ino, 0, NULL, mapfilesindir,
				    (void *)&mfc);

	if (retval) {
		com_err(disk, retval, "while mapping files in %s", directory);
		exit(X_ABORT);
	}
	/*
	 * Ensure that the root inode actually appears in the file list
	 * for a subdir
	 */
	mapfileino(ROOTINO, 0, tapesize, &anydirskipped);
	/*
	 * Restore gets very upset if the root is not dumped,
	 * so ensure that it always is dumped.
	 */
	SETINO(ROOTINO, dumpinomap);
	return anydirskipped;
}
#endif

#ifdef __linux__
struct mapdirs_context {
	int *ret;
	int nodump;
	long long *tapesize;
};
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
mapdirs(dump_ino_t maxino, long long *tapesize)
{
	struct	dinode *dp;
	int isdir;
	char *map;
	dump_ino_t ino;
#ifndef __linux__
	int i;
	long filesize;
#else
	struct mapdirs_context mdc;
#endif
	int ret, change = 0, nodump;

	isdir = 0;		/* XXX just to get gcc to shut up */
	for (map = dumpdirmap, ino = 1; ino < maxino; ino++) {
		if (((ino - 1) % NBBY) == 0)	/* map is offset by 1 */
			isdir = *map++;
		else
			isdir >>= 1;
		/*
		 * If dir has been removed from the used map, it's either
		 * because it had the nodump flag, or it herited it from
		 * its parent. A directory can't be in dumpinomap if not
		 * in usedinomap, but we have to go through it anyway 
	 	 * to propagate the nodump attribute.
		 */
		if ((isdir & 1) == 0)
			continue;
		nodump = (TSTINO(ino, usedinomap) == 0);
		if (TSTINO(ino, dumpinomap) && nodump == 0)
			continue;
		dp = getino(ino);
#ifdef	__linux__
		ret = 0;
		mdc.ret = &ret;
		mdc.nodump = nodump;
		mdc.tapesize = tapesize;
		ext2fs_dir_iterate(fs, ino, 0, NULL, searchdir, (void *) &mdc);
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
		if (nodump) {
			if (ret & HASSUBDIRS)
				change = 1; /* subdirs have inherited nodump */
			CLRINO(ino, dumpdirmap);
		} else if ((ret & HASSUBDIRS) == 0) {
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
dirindir(dump_ino_t ino, daddr_t blkno, int ind_level, long *filesize)
{
	int ret = 0;
	int i;
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
searchdir(struct ext2_dir_entry *dp, UNUSED(int offset), 
	  UNUSED(int blocksize), UNUSED(char *buf), void *private)
{
	struct mapdirs_context *mdc;
	int *ret;
	long long *tapesize;
	struct dinode *ip;

	mdc = (struct mapdirs_context *)private;
	ret = mdc->ret;
	tapesize = mdc->tapesize;

	if (dp->inode == 0)
		return 0;
	if (dp->name[0] == '.') {
		if (( dp->name_len & 0xFF ) == 1)
			return 0;
		if (dp->name[1] == '.' && ( dp->name_len & 0xFF ) == 2)
			return 0;
	}
	if (mdc->nodump) {
		ip = getino(dp->inode);
		if (TSTINO(dp->inode, dumpinomap)) {
			CLRINO(dp->inode, dumpinomap);
			*tapesize -= blockest(ip);
		}
		/* Add dir back to the dir map and remove from
		 * usedinomap to propagate nodump */
		if ((ip->di_mode & IFMT) == IFDIR) {
			SETINO(dp->inode, dumpdirmap);
			CLRINO(dp->inode, usedinomap);
			*ret |= HASSUBDIRS;
		}
	} else {
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
	}
	return 0;
}

#else	/* __linux__ */

static int
searchdir(dump_ino_t ino, daddr_t blkno, long size, long filesize)
{
	struct direct *dp;
	long loc, ret = 0;
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
	ext2_ino_t ino;
	blk_t	*buf;
	int	cnt;
	int	max;
	int	next_block;
};

/*
 * Dump a block to the tape
 */
static int
dumponeblock(UNUSED(ext2_filsys fs), blk_t *blocknr, e2_blkcnt_t blockcnt,
	     UNUSED(blk_t ref_block), UNUSED(int ref_offset), void * private)
{
	struct block_context *p;
	e2_blkcnt_t i;

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

static void
dump_xattr(dump_ino_t ino, struct dinode *dp) {

	if (dp->di_extraisize != 0) {
#ifdef HAVE_EXT2FS_READ_INODE_FULL
		char inode[EXT2_INODE_SIZE(fs->super)];
		errcode_t err;
		u_int32_t *magic;

		memset(inode, 0, EXT2_INODE_SIZE(fs->super));
		err = ext2fs_read_inode_full(fs, (ext2_ino_t)ino,
					     (struct ext2_inode *) inode,
					     EXT2_INODE_SIZE(fs->super));
		if (err) {
			com_err(disk, err, "while reading inode #%ld\n", (long)ino);
			exit(X_ABORT);
		}

		magic = (void *)inode + EXT2_GOOD_OLD_INODE_SIZE + dp->di_extraisize;
		if (*magic == EXT2_XATTR_MAGIC) {
			char xattr[EXT2_INODE_SIZE(fs->super)];
			int i;
			char *cp;

			if (vflag)
				msg("dumping EA (inode) in inode #%ld\n", (long)ino);
			memset(xattr, 0, EXT2_INODE_SIZE(fs->super));
			memcpy(xattr, (void *)magic,
			       EXT2_INODE_SIZE(fs->super) - 
			       (EXT2_GOOD_OLD_INODE_SIZE + dp->di_extraisize));
			magic = (u_int32_t *)xattr;
			*magic = EXT2_XATTR_MAGIC2;

			spcl.c_type = TS_INODE;
			spcl.c_dinode.di_size = EXT2_INODE_SIZE(fs->super);
			spcl.c_flags |= DR_EXTATTRIBUTES;
			spcl.c_extattributes = EXT_XATTR;
			spcl.c_count = howmany(EXT2_INODE_SIZE(fs->super), TP_BSIZE);
			for (i = 0; i < spcl.c_count; i++)
				spcl.c_addr[i] = 1;
			writeheader(ino);
			for (i = 0, cp = xattr; i < spcl.c_count; i++, cp += TP_BSIZE)
				writerec(cp, 0);
			spcl.c_flags &= ~DR_EXTATTRIBUTES;
			spcl.c_extattributes = 0;
		}
#endif
	}

	if (dp->di_file_acl) {

		if (vflag)
			msg("dumping EA (block) in inode #%ld\n", (long)ino);

		spcl.c_type = TS_INODE;
		spcl.c_dinode.di_size = sblock->fs_bsize;
		spcl.c_flags |= DR_EXTATTRIBUTES;
		spcl.c_extattributes = EXT_XATTR;
		blksout(&dp->di_file_acl, EXT2_FRAGS_PER_BLOCK(fs->super), ino);
		spcl.c_flags &= ~DR_EXTATTRIBUTES;
		spcl.c_extattributes = 0;
	}
}

/*
 * Dump passes 3 and 4.
 *
 * Dump the contents of an inode to tape.
 */
void
dumpino(struct dinode *dp, dump_ino_t ino, int metaonly)
{
	//unsigned long cnt;
	//uint64_t size;
	uint64_t remaining;
	char buf[TP_BSIZE];
	struct new_bsd_inode nbi;
	int i;
#ifdef	__linux__
	struct block_context bc;
#else
	int ind_level;
#endif
	uint64_t i_size;
	
	if (metaonly)
		i_size = 0;
	else 
		i_size = dp->di_size + ((uint64_t) dp->di_size_high << 32);

	if (newtape) {
		newtape = 0;
		dumpmap(dumpinomap, TS_BITS, ino);
	}
	CLRINO(ino, dumpinomap);
#ifdef	__linux__
	memset(&nbi, 0, sizeof(nbi));
	nbi.di_mode = dp->di_mode;
	nbi.di_nlink = dp->di_nlink;
	nbi.di_ouid = dp->di_uid;
	nbi.di_ogid = dp->di_gid;
	nbi.di_size = i_size;
	nbi.di_atime.tv_sec = dp->di_atime;
	nbi.di_mtime.tv_sec = dp->di_mtime;
	nbi.di_ctime.tv_sec = dp->di_ctime;
	memmove(&nbi.di_db, &dp->di_db, (NDADDR + NIADDR) * sizeof(daddr_t));
	nbi.di_flags = dp->di_flags;
	nbi.di_blocks = dp->di_blocks;
	nbi.di_gen = dp->di_gen;
	nbi.di_uid = (((int32_t)dp->di_uidhigh) << 16) | dp->di_uid;
	nbi.di_gid = (((int32_t)dp->di_gidhigh) << 16) | dp->di_gid;
	memmove(&spcl.c_dinode, &nbi, sizeof(nbi));
#else	/* __linux__ */
	spcl.c_dinode = *dp;
#endif	/* __linux__ */
	spcl.c_type = TS_INODE;
	spcl.c_count = 0;

	indexer->addInode(dp, ino, metaonly);

	if (metaonly && (dp->di_mode & S_IFMT)) {
		spcl.c_flags |= DR_METAONLY;
		spcl.c_count = 0;
		writeheader(ino);
		spcl.c_flags &= ~DR_METAONLY;
		dump_xattr(ino, dp);
		return;
	}

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
		if (i_size > 0 &&
		    i_size < EXT2_N_BLOCKS * sizeof (daddr_t)) {
			spcl.c_addr[0] = 1;
			spcl.c_count = 1;
			writeheader(ino);
			memmove(buf, dp->di_db, (u_long)dp->di_size);
			buf[dp->di_size] = '\0';
			writerec(buf, 0);
			dump_xattr(ino, dp);
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
		if (i_size)
			break;
		/* fall through */

	case S_IFIFO:
	case S_IFSOCK:
	case S_IFCHR:
	case S_IFBLK:
		writeheader(ino);
		dump_xattr(ino, dp);
		return;

	default:
		msg("Warning: undefined file type 0%o\n", dp->di_mode & IFMT);
		return;
	}
#ifdef	__linux__
	bc.max = NINDIR(sblock) * EXT2_FRAGS_PER_BLOCK(fs->super);
	bc.buf = malloc (bc.max * sizeof (int));
	bc.cnt = 0;
	bc.ino = ino;
	bc.next_block = 0;

	ext2fs_block_iterate2(fs, (ext2_ino_t)ino, BLOCK_FLAG_DATA_ONLY, NULL, dumponeblock, (void *)&bc);
	/* deal with holes at the end of the inode */
	if (i_size > ((uint64_t)bc.next_block) * sblock->fs_fsize) {
		remaining = i_size - ((uint64_t)bc.next_block) * sblock->fs_fsize;
		for (i = 0; i < (int)howmany(remaining, sblock->fs_fsize); i++) {
			bc.buf[bc.cnt++] = 0;
			if (bc.cnt == bc.max) {
				blksout (bc.buf, bc.cnt, bc.ino);
				bc.cnt = 0;
			}
		}
	}
	if (bc.cnt > 0) {
		blksout (bc.buf, bc.cnt, bc.ino);
	}
	free(bc.buf);
	dump_xattr(ino, dp);
#else
	if (i_size > (uint64_t)NDADDR * sblock->fs_bsize)
		cnt = NDADDR * sblock->fs_frag;
	else
		cnt = howmany(i_size, sblock->fs_fsize);
	blksout(&dp->di_db[0], cnt, ino);
	if ((int64_t) (size = i_size - NDADDR * sblock->fs_bsize) <= 0) {
		dump_xattr(ino, dp);
		return;
	}
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
convert_dir(struct ext2_dir_entry *dirent, UNUSED(int offset), 
	    UNUSED(int blocksize), UNUSED(char *buf), void *private)
{
	struct convert_dir_context *p;
	struct direct *dp;
	int reclen;

	/* do not save entries to excluded inodes */
	if (exclude_ino(dirent->inode))
		return 0;

	p = (struct convert_dir_context *)private;

	reclen = EXT2_DIR_REC_LEN((dirent->name_len & 0xFF) + 1);
	if (((p->offset + reclen - 1) / p->bs) != (p->offset / p->bs)) {
		dp = (struct direct *)(p->buf + p->prev_offset);
		dp->d_reclen += p->bs - (p->offset % p->bs);
		p->offset += p->bs - (p->offset % p->bs);
	}

	dp = (struct direct *)(p->buf + p->offset);
	dp->d_ino = dirent->inode;
	dp->d_reclen = reclen;
	dp->d_namlen = dirent->name_len & 0xFF;
	switch ((dirent->name_len & 0xFF00) >> 8) {
	default:
		dp->d_type = DT_UNKNOWN;
		break;
	case EXT2_FT_REG_FILE:
		dp->d_type = DT_REG;
		break;
	case EXT2_FT_DIR:
		dp->d_type = DT_DIR;
		break;
	case EXT2_FT_CHRDEV:
		dp->d_type = DT_CHR;
		break;
	case EXT2_FT_BLKDEV:
		dp->d_type = DT_BLK;
		break;
	case EXT2_FT_FIFO:
		dp->d_type = DT_FIFO;
		break;
	case EXT2_FT_SOCK:
		dp->d_type = DT_SOCK;
		break;
	case EXT2_FT_SYMLINK:
		dp->d_type = DT_LNK;
		break;
	}
	strncpy(dp->d_name, dirent->name, dp->d_namlen);
	dp->d_name[dp->d_namlen] = '\0';
	p->prev_offset = p->offset;
	p->offset += reclen;

	//indexer->addDirEntry(dirent, offset, blocksize, buf, private);

	return 0;
}

/*
 * Dump pass 3
 *
 * Dumps a directory to tape after converting it to the BSD format
 */
void
dumpdirino(struct dinode *dp, dump_ino_t ino)
{
	uint64_t size;
	char buf[TP_BSIZE];
	struct new_bsd_inode nbi;
	struct convert_dir_context cdc;
	errcode_t retval;
	struct ext2_dir_entry *de;
	uint64_t dir_size;

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
	retval = ext2fs_dir_iterate(fs, (ext2_ino_t)ino, 0, NULL, convert_dir, (void *)&cdc);
	if (retval) {
		com_err(disk, retval, "while converting directory #%ld\n", (long)ino);
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
	memset(&nbi, 0, sizeof(nbi));
	nbi.di_mode = dp->di_mode;
	nbi.di_nlink = dp->di_nlink;
	nbi.di_ouid = dp->di_uid;
	nbi.di_ogid = dp->di_gid;
	nbi.di_size = dir_size; /* (uint64_t)dp->di_size; */
	nbi.di_atime.tv_sec = dp->di_atime;
	nbi.di_mtime.tv_sec = dp->di_mtime;
	nbi.di_ctime.tv_sec = dp->di_ctime;
	memmove(&nbi.di_db, &dp->di_db, (NDADDR + NIADDR) * sizeof(daddr_t));
	nbi.di_flags = dp->di_flags;
	nbi.di_blocks = dp->di_blocks;
	nbi.di_gen = dp->di_gen;
	nbi.di_uid = (((int32_t)dp->di_uidhigh) << 16) | dp->di_uid;
	nbi.di_gid = (((int32_t)dp->di_gidhigh) << 16) | dp->di_gid;
	memmove(&spcl.c_dinode, &nbi, sizeof(nbi));
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

	indexer->addInode(dp, ino, 0);

	(void)free(cdc.buf);
	dump_xattr(ino, dp);
}
#endif	/* __linux__ */

#ifndef	__linux__
/*
 * Read indirect blocks, and pass the data blocks to be dumped.
 */
static void
dmpindir(dump_ino_t ino, daddr_t blk, int ind_level, uint64_t *size)
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
	    (fs->flags & EXT2_FLAG_SWAP_BYTES_READ))
#endif
	{
		max = sblock->fs_bsize >> 2;
		swapme = (blk_t *) idblk;
		for (i = 0; i < max; i++, swapme++)
			*swapme = ext2fs_swab32(*swapme);
	}
#endif /* __linux__ */
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
blksout(blk_t *blkp, int frags, dump_ino_t ino)
{
	blk_t *bp;
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
dumpmap(char *map, int type, dump_ino_t ino)
{
	int i;
	char *cp;

	spcl.c_type = type;
	spcl.c_count = howmany(mapsize * sizeof(char), TP_BSIZE);
	spcl.c_dinode.di_size = mapsize;
	writeheader(ino);
	for (i = 0, cp = map; i < spcl.c_count; i++, cp += TP_BSIZE)
		writerec(cp, 0);
}

#if defined(__linux__) && !defined(int32_t)
#define int32_t __s32
#endif

/* 
 * Compute and fill in checksum information.
 */
void
mkchecksum(union u_spcl *tmpspcl) 
{
	int32_t sum, cnt, *lp;

	tmpspcl->s_spcl.c_checksum = 0;
	lp = (int32_t *)&tmpspcl->s_spcl;
	sum = 0;
	cnt = sizeof(union u_spcl) / (4 * sizeof(int32_t));
	while (--cnt >= 0) {
		sum += *lp++;
		sum += *lp++;
		sum += *lp++;
		sum += *lp++;
	}
	tmpspcl->s_spcl.c_checksum = CHECKSUM - sum;
}

/*
 * Write a header record to the dump tape.
 */
void
writeheader(dump_ino_t ino)
{
	char *state; /* need to have some place to put this! */
	spcl.c_inumber = ino;
	spcl.c_magic = NFS_MAGIC;
	mkchecksum((union u_spcl *)&spcl);
	writerec((char *)&spcl, 1);
}

#ifdef	__linux__
struct dinode *
getino(dump_ino_t inum)
{
	static struct dinode dinode;
	errcode_t err;

	curino = inum;
#ifdef HAVE_EXT2FS_READ_INODE_FULL
	err = ext2fs_read_inode_full(fs, (ext2_ino_t)inum, (struct ext2_inode *) &dinode, sizeof(struct dinode));
#else
	err = ext2fs_read_inode(fs, (ext2_ino_t)inum, (struct ext2_inode *) &dinode);
#endif
	if (err) {
		com_err(disk, err, "while reading inode #%ld\n", (long)inum);
		exit(X_ABORT);
	}
	return &dinode;
}
#else	/* __linux__ */
struct dinode *
getino(dump_ino_t inum)
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

void
bread(ext2_loff_t blkno, char *buf, int size)
{
	int cnt, i;

loop:
#ifdef	__linux__
	if (ext2fs_llseek(diskfd, (blkno << dev_bshift), 0) !=
			(blkno << dev_bshift))
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
		msg("read error from %s: %s: [block %d, ext2blk %d]: count=%d\n",
			disk, strerror(errno), blkno, 
			dbtofsb(sblock, blkno), size);
	else
		msg("short read error from %s: [block %d, ext2blk %d]: count=%d, got=%d\n",
			disk, blkno, dbtofsb(sblock, blkno), size, cnt);
	if (breademax && ++breaderrors > breademax) {
		msg("More than %d block read errors from %d\n",
			breademax, disk);
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
		if (ext2fs_llseek(diskfd, (blkno << dev_bshift), 0) !=
				(blkno << dev_bshift))
#else
		if (lseek(diskfd, ((off_t)blkno << dev_bshift), 0) !=
						((off_t)blkno << dev_bshift))
#endif
			msg("bread: lseek2 fails!\n");
		if ((cnt = read(diskfd, buf, (int)dev_bsize)) == dev_bsize)
			continue;
		if (cnt == -1) {
			msg("read error from %s: %s: [sector %d, ext2blk %d]: count=%d\n",
				disk, strerror(errno), blkno, 
				dbtofsb(sblock, blkno), dev_bsize);
			continue;
		}
		msg("short read error from %s: [sector %d, ext2blk %d]: count=%d, got=%d\n",
			disk, blkno, dbtofsb(sblock, blkno), dev_bsize, cnt);
	}
}
