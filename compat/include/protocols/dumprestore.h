/*
 *	Ported to Linux's Second Extended File System as part of the
 *	dump and restore backup suit
 *	Remy Card <card@Linux.EU.Org>, 1994-1997
 *	Stelian Pop <stelian@popies.net>, 1999-2000
 *	Stelian Pop <stelian@popies.net> - Alcôve <www.alcove.com>, 2000-2002
 *
 *	$Id: dumprestore.h,v 1.18 2003/03/30 15:40:34 stelian Exp $
 */

/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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

#ifndef _PROTOCOLS_DUMPRESTORE_H_
#define _PROTOCOLS_DUMPRESTORE_H_

#include <config.h>

/*
 * TP_BSIZE is the size of file blocks on the dump tapes.
 * Note that TP_BSIZE must be a multiple of DEV_BSIZE.
 *
 * NTREC is the number of TP_BSIZE blocks that are written
 * in each tape record. HIGHDENSITYTREC is the number of
 * TP_BSIZE blocks that are written in each tape record on
 * 6250 BPI or higher density tapes.
 *
 * TP_NINDIR is the number of indirect pointers in a TS_INODE
 * or TS_ADDR record. Note that it must be a power of two.
 */
#define TP_BSIZE	1024
#define NTREC   	10
#define HIGHDENSITYTREC	32
#define TP_NINDIR	(TP_BSIZE/2)
#define TP_NINOS	(TP_NINDIR / sizeof (int32_t))
#define LBLSIZE		16
#define NAMELEN		64

#define OFS_MAGIC   	(int)60011
#define NFS_MAGIC   	(int)60012
#define CHECKSUM	(int)84446

typedef u_int32_t	dump_ino_t;

union u_data {
	char	s_addrs[TP_NINDIR];	/* 1 => data; 0 => hole in inode */
	int32_t	s_inos[TP_NINOS];	/* table of first inode on each volume */
} u_data;

union u_spcl {
	char dummy[TP_BSIZE];
	struct	s_spcl {
		int32_t	c_type;		    /* record type (see below) */
		int32_t	c_date;		    /* date of this dump */
		int32_t	c_ddate;	    /* date of previous dump */
		int32_t	c_volume;	    /* dump volume number */
		daddr_t	c_tapea;	    /* logical block of this record */
		dump_ino_t c_inumber;	    /* number of inode */
		int32_t	c_magic;	    /* magic number (see above) */
		int32_t	c_checksum;	    /* record checksum */
#ifdef	__linux__
		struct	new_bsd_inode c_dinode;
#else
		struct	dinode	c_dinode;   /* ownership and mode of inode */
#endif
		int32_t	c_count;	    /* number of valid c_addr entries */
		union u_data c_data;	    /* see above */
		char	c_label[LBLSIZE];   /* dump label */
		int32_t	c_level;	    /* level of this dump */
		char	c_filesys[NAMELEN]; /* name of dumpped file system */
		char	c_dev[NAMELEN];	    /* name of dumpped device */
		char	c_host[NAMELEN];    /* name of dumpped host */
		int32_t	c_flags;	    /* additional information */
		int32_t	c_firstrec;	    /* first record on volume */
		int32_t	c_ntrec;	    /* blocksize on volume */
		int32_t	c_spare[31];	    /* reserved for future uses */
	} s_spcl;
} u_spcl;
#define spcl u_spcl.s_spcl
#define c_addr c_data.s_addrs
#define c_inos c_data.s_inos

/*
 * special record types
 */
#define TS_TAPE 	1	/* dump tape header */
#define TS_INODE	2	/* beginning of file record */
#define TS_ADDR 	4	/* continuation of file record */
#define TS_BITS 	3	/* map of inodes on tape */
#define TS_CLRI 	6	/* map of inodes deleted since last dump */
#define TS_END  	5	/* end of volume marker */

/*
 * flag values
 */
#define DR_NEWHEADER	0x0001	/* new format tape header */
#define DR_NEWINODEFMT	0x0002	/* new format inodes on tape */
#define DR_COMPRESSED	0x0080	/* dump tape is compressed */
#define DR_METAONLY	0x0100	/* only the metadata of the inode has
				   been dumped */
#define DR_INODEINFO	0x0002	/* TS_END header contains c_inos information */


/*
 * compression flags for the tapebuf header.
 */
#define COMPRESS_ZLIB	0
#define COMPRESS_BZLIB	1

/* used for compressed dump tapes */
struct tapebuf {
	unsigned int	compressed:1;
	unsigned int	flags:3;
	unsigned int	length:28;
	char		buf[0];	/* the data */
};

#endif /* !_DUMPRESTORE_H_ */
