/*
 *	Ported to Linux's Second Extended File System as part of the
 *	dump and restore backup suit
 *	Remy Card <card@Linux.EU.Org>, 1994-1997
 *	Stelian Pop <stelian@popies.net>, 1999-2000
 *	Stelian Pop <stelian@popies.net> - Alcôve <www.alcove.com>, 2000-2002
 *
 *	$Id: bsdcompat.h,v 1.19 2002/07/19 14:57:39 stelian Exp $
 */

#include <config.h>
#include <sys/time.h>
#include <dirent.h>

#define	__dead		volatile
#define UNUSED(x)	x __attribute__ ((unused))

#ifndef	NBBY
#define NBBY		8
#endif

#ifndef	MIN
#define MIN(a,b)	((a < b) ? a : b)
#endif

#define	WINO		1
#define	DEV_BSIZE	512
#define	DEV_BSHIFT	9
#define	MAXBSIZE	EXT2_MAX_BLOCK_SIZE
#define ROOTINO		EXT2_ROOT_INO
#ifdef	EXT2_NODUMP_FL
#define UF_NODUMP	EXT2_NODUMP_FL
#endif

#define howmany(x,y)	(((x)+((y)-1))/(y))
#define roundup(x, y)	((((x)+((y)-1))/(y))*(y))
#define powerof2(x)	((((x)-1)&(x))==0)

#define dbtob(b)	((unsigned)(b) << DEV_BSHIFT)
#define fsbtodb(sb,b)	((int)(((long long)(b) * EXT2_BLOCK_SIZE((sb)->super)) / DEV_BSIZE))
#define dbtofsb(sb,b)	((int)(((long long)(b) * DEV_BSIZE) / EXT2_BLOCK_SIZE((sb)->super)))

#define	sblock		fs
#define fs_fsize	fragsize
#define fs_bsize	blocksize
#define fs_size		super->s_blocks_count

#define	IFMT		S_IFMT
#define IFLNK		S_IFLNK
#define IFREG		S_IFREG
#define IFDIR		S_IFDIR
#define IFCHR		S_IFCHR
#define IFBLK		S_IFBLK
#define IFSOCK		S_IFSOCK
#define IFIFO		S_IFIFO

#if 0
typedef __s64		quad_t;
typedef __u64		u_quad_t;
#endif

/*
 * The BSD dump format reserves 4 bytes for a time_t, but other architectures
 * (notably axp) have larger time_t.  ctime4() is a modified ctime() which
 * always accepts short 4-byte times.
 */
#define ctime4(timep) ({ time_t t = *(timep); ctime(&t); })

/*
 * This is the ext2_inode structure but the fields have been renamed
 * to match 4.4BSD's names
 */
#define	NDADDR		12
#define	NIADDR		 3

#define NINDIR(fs)	EXT2_ADDR_PER_BLOCK(fs->super)

struct dinode {
	__u16	di_mode;
	__u16	di_uid;
	__u32	di_size;
	__u32	di_atime;
	__u32	di_ctime;
	__u32	di_mtime;
	__u32	di_dtime;
	__u16	di_gid;
	__u16	di_nlink;
	__u32	di_blocks;
	__u32	di_flags;
	__u32	di_reserved1;
	daddr_t	di_db[NDADDR];
	daddr_t	di_ib[NIADDR];
	__u32	di_gen;
	__u32	di_file_acl;
	__u32	di_dir_acl;
	__u32	di_faddr;
	__u8	di_frag;
	__u8	di_fsize;
	__u16	di_pad1;
	__u16	di_uidhigh;
	__u16	di_gidhigh;
	__u32	di_spare;
};

#define di_rdev		di_db[0]
/* #define di_ouid		di_uid */
/* #define di_ogid		di_gid */
#define di_size_high	di_dir_acl

/*
 * This is the ext2_dir_entry structure but the fields have been renamed
 * to match 4.4BSD's names
 *
 * This is the 4.4BSD directory entry structure
 */
#define DIRBLKSIZ	DEV_BSIZE
#ifndef MAXNAMLEN
#define MAXNAMLEN	255
#endif

/*
 * For old libc.
 */
#ifndef DT_UNKNOWN
#define DT_UNKNOWN	 0
#define DT_FIFO		 1
#define DT_CHR		 2
#define DT_DIR		 4
#define DT_BLK		 6
#define DT_REG		 8
#define DT_LNK		10
#define DT_SOCK		12
#endif

#ifndef d_fileno
#define d_fileno d_ino
#endif

/*
 * The direct structure used by dump/restore.
 */
struct direct {
	__u32	d_ino;
	__u16	d_reclen;
	__u8	d_type;
	__u8	d_namlen;
	char	d_name[MAXNAMLEN + 1];
};
/*
 * Convert between stat structure types and directory types.
 */
#define IFTODT(mode)	(((mode) & 0170000) >> 12)
#define DTTOIF(dirtype)	((dirtype) << 12)

/*
 * The DIRSIZ macro gives the minimum record length which will hold
 * the directory entry.  This requires the amount of space in struct direct
 * without the d_name field, plus enough space for the name with a terminating
 * null byte (dp->d_namlen+1), rounded up to a 4 byte boundary.
 */
#if	0
#if (BYTE_ORDER == LITTLE_ENDIAN)
#define DIRSIZ(oldfmt, dp) \
    ((oldfmt) ? \
    ((sizeof (struct direct) - (MAXNAMLEN+1)) + (((dp)->d_type+1 + 3) &~ 3)) : \
    ((sizeof (struct direct) - (MAXNAMLEN+1)) + (((dp)->d_namlen+1 + 3) &~ 3)))
#else
#define DIRSIZ(oldfmt, dp) \
    ((sizeof (struct direct) - (MAXNAMLEN+1)) + (((dp)->d_namlen+1 + 3) &~ 3))
#endif
#else

#define DIRSIZ(oldfmt,dp)	EXT2_DIR_REC_LEN(((dp)->d_namlen & 0xff) + 1)

#endif

/*
 * This is the old (Net/2) BSD inode structure
 * copied from the FreeBSD 1.1.5.1 <ufs/dinode.h> include file
 */
#define	MAXFASTLINK	(((NDADDR + NIADDR) * sizeof(unsigned long)) - 1)

struct old_bsd_inode {
	__u16		di_mode;
	__s16		di_nlink;
	__u16		di_uid;
	__u16		di_gid;
#if	1
	union {
		u_quad_t	v;
		__u32		val[2];
	}		di_qsize;
#else
	u_quad_t	di_size;
#endif
	__u32		di_atime;
	__s32		di_atspare;
	__u32		di_mtime;
	__s32		di_mtspare;
	__u32		di_ctime;
	__s32		di_ctspare;
#if	0
	union {
		struct {
			daddr_t	di_udb[NDADDR];
			daddr_t	di_uib[NIADDR];
		} di_addr;
		char	di_usymlink[MAXFASTLINK + 1];
	}		di_un;
#else
	daddr_t		di_db[NDADDR];
	daddr_t		di_ib[NIADDR];
#endif
	__s32		di_flags;
	__s32		di_blocks;
	__s32		di_gen;
	__u32		di_spare[4];
};

struct bsdtimeval {    /* XXX alpha-*-linux is deviant */
	__u32   tv_sec;
	__u32   tv_usec;
};

/*
 * This is the new (4.4) BSD inode structure
 * copied from the FreeBSD 2.0 <ufs/ufs/dinode.h> include file
 */
struct new_bsd_inode {
	__u16		di_mode;
	__s16		di_nlink;
	union {
		__u16		oldids[2];
		__u32		inumber;
	}		di_u;
	u_quad_t	di_size;
	struct bsdtimeval	di_atime;
	struct bsdtimeval	di_mtime;
	struct bsdtimeval	di_ctime;
	daddr_t		di_db[NDADDR];
	daddr_t		di_ib[NIADDR];
	__u32		di_flags;
	__s32		di_blocks;
	__s32		di_gen;
	__u32		di_uid;
	__u32		di_gid;
	__s32		di_spare[2];
};

#define	di_ouid		di_u.oldids[0]
#define	di_ogid		di_u.oldids[1]
