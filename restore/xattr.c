/*
 * Copyright (c) 1999-2004
 *	Stelian Pop <stelian@popies.net>, 1999-2004
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
	"$Id: xattr.c,v 1.1 2005/05/02 15:10:47 stelian Exp $";
#endif /* not lint */

#include <config.h>
#include <compatlfs.h>
#include <compaterr.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <bsdcompat.h>
#include <protocols/dumprestore.h>
#include "restore.h"
#include "extern.h"
#include "pathnames.h"

/*
 * Data structures below taken from the kernel
 */

/* Maximum number of references to one attribute block */
#define EXT2_XATTR_REFCOUNT_MAX		1024

/* Name indexes */
#define EXT2_XATTR_INDEX_MAX			10
#define EXT2_XATTR_INDEX_USER			1
#define EXT2_XATTR_INDEX_POSIX_ACL_ACCESS	2
#define EXT2_XATTR_INDEX_POSIX_ACL_DEFAULT	3
#define EXT2_XATTR_INDEX_TRUSTED		4
#define	EXT2_XATTR_INDEX_LUSTRE			5
#define EXT2_XATTR_INDEX_SECURITY	        6

struct ext2_xattr_header {
	u_int32_t	h_magic;	/* magic number for identification */
	u_int32_t	h_refcount;	/* reference count */
	u_int32_t	h_blocks;	/* number of disk blocks used */
	u_int32_t	h_hash;		/* hash value of all attributes */
	u_int32_t	h_reserved[4];	/* zero right now */
};

struct ext3_xattr_ibody_header {
	u_int32_t	h_magic;	/* magic number for identification */
};

struct ext2_xattr_entry {
	u_char		e_name_len;	/* length of name */
	u_char		e_name_index;	/* attribute name index */
	u_int16_t	e_value_offs;	/* offset in disk block of value */
	u_int32_t	e_value_block;	/* disk block attribute is stored on (n/i) */
	u_int32_t	e_value_size;	/* size of attribute value */
	u_int32_t	e_hash;		/* hash value of name and value */
	char		e_name[0];	/* attribute name */
};

#define EXT2_XATTR_PAD_BITS		2
#define EXT2_XATTR_PAD		(1<<EXT2_XATTR_PAD_BITS)
#define EXT2_XATTR_ROUND		(EXT2_XATTR_PAD-1)
#define EXT2_XATTR_LEN(name_len) \
	(((name_len) + EXT2_XATTR_ROUND + \
	sizeof(struct ext2_xattr_entry)) & ~EXT2_XATTR_ROUND)
#define EXT2_XATTR_NEXT(entry) \
	( (struct ext2_xattr_entry *)( \
	  (char *)(entry) + EXT2_XATTR_LEN((entry)->e_name_len)) )
#define EXT3_XATTR_SIZE(size) \
	(((size) + EXT2_XATTR_ROUND) & ~EXT2_XATTR_ROUND)

#define HDR(buffer) ((struct ext2_xattr_header *)(buffer))
#define ENTRY(ptr) ((struct ext2_xattr_entry *)(ptr))
#define IS_LAST_ENTRY(entry) (*(__u32 *)(entry) == 0)

#define BFIRST(buffer) ENTRY(HDR(buffer)+1)
#define IFIRST(buffer) ENTRY(((struct ext3_xattr_ibody_header *)(buffer))+1)

#define FIRST_ENTRY(buffer) \
	((HDR(buffer)->h_magic == EXT2_XATTR_MAGIC2) ? \
		IFIRST(buffer) : \
		BFIRST(buffer))

/*
 * On-block xattr value offsets start at the beginning of the block, but
 * on-inode xattr value offsets start after the initial header 
 * (ext3_xattr_ibody_header).
 */
#define VALUE_OFFSET(buffer, entry) \
	(((HDR(buffer)->h_magic == EXT2_XATTR_MAGIC2) ? \
		(entry)->e_value_offs + sizeof(struct ext3_xattr_ibody_header) : \
		(entry)->e_value_offs))
	
/*
 * xattr syscalls do not exist yet in libc, get our own copy here,
 * taken from libattr.
 */
#if defined (__i386__)
# define HAVE_XATTR_SYSCALLS 1
# define __NR_lsetxattr		227
# define __NR_lgetxattr		230
# define __NR_llistxattr	233
#elif defined (__sparc__)
# define HAVE_XATTR_SYSCALLS 1
# define __NR_lsetxattr		170
# define __NR_lgetxattr		173
# define __NR_llistxattr	179
#elif defined (__ia64__)
# define HAVE_XATTR_SYSCALLS 1
# define __NR_lsetxattr		1218
# define __NR_lgetxattr		1221
# define __NR_llistxattr	1224
#elif defined (__powerpc__)
# define HAVE_XATTR_SYSCALLS 1
# define __NR_lsetxattr		210
# define __NR_lgetxattr		213
# define __NR_llistxattr	216
#elif defined (__x86_64__)
# define HAVE_XATTR_SYSCALLS 1
# define __NR_lsetxattr		189
# define __NR_lgetxattr		192
# define __NR_llistxattr	195
#elif defined (__s390__)
# define HAVE_XATTR_SYSCALLS 1
# define __NR_lsetxattr		225
# define __NR_lgetxattr		228
# define __NR_llistxattr	231
#elif defined (__arm__)
# define HAVE_XATTR_SYSCALLS 1
# define __NR_SYSCALL_BASE 0x900000
# define __NR_lsetxattr		(__NR_SYSCALL_BASE+227)
# define __NR_lgetxattr		(__NR_SYSCALL_BASE+230)
# define __NR_llistxattr	(__NR_SYSCALL_BASE+233)
#elif defined (__mips64__)
# define HAVE_XATTR_SYSCALLS 1
# define __NR_Linux 5000
# define __NR_lsetxattr		(__NR_Linux + 218)
# define __NR_lgetxattr		(__NR_Linux + 221)
# define __NR_llistxattr	(__NR_Linux + 224)
#elif defined (__mips__)
# define HAVE_XATTR_SYSCALLS 1
# define __NR_Linux 4000
# define __NR_lsetxattr		(__NR_Linux + 225)
# define __NR_lgetxattr		(__NR_Linux + 228)
# define __NR_llistxattr	(__NR_Linux + 231)
#elif defined (__alpha__)
# define HAVE_XATTR_SYSCALLS 1
# define __NR_lsetxattr		383
# define __NR_lgetxattr		386
# define __NR_llistxattr	389
#elif defined (__mc68000__)
# define HAVE_XATTR_SYSCALLS 1
# define __NR_lsetxattr		224
# define __NR_lgetxattr		227
# define __NR_llistxattr	230
#else
# warning "Extended attribute syscalls undefined for this architecture"
# define HAVE_XATTR_SYSCALLS 0
#endif

#if HAVE_XATTR_SYSCALLS
# define SYSCALL(args...)	syscall(args)
#else
# define SYSCALL(args...)	( errno = ENOSYS, -1 )
#endif

static int lsetxattr __P((const char *, const char *, void *, size_t, int));
static ssize_t lgetxattr __P((const char *, const char *, void *, size_t));
static ssize_t llistxattr __P((const char *, char *, size_t));
static int xattr_cb_list __P((char *, char *, int, void *));
static int xattr_cb_set __P((char *, char *, int, void *));
static int xattr_cb_compare __P((char *, char *, int, void *));
static int xattr_verify __P((char *));
static int xattr_count __P((char *, int *));
static int xattr_walk __P((char *, int (*)(char *, char *, int, void *), void *));

static int
lsetxattr(const char *path, const char *name, void *value, size_t size, int flags)
{
	return SYSCALL(__NR_lsetxattr, path, name, value, size, flags);
}

static ssize_t
lgetxattr(const char *path, const char *name, void *value, size_t size)
{
	return SYSCALL(__NR_lgetxattr, path, name, value, size);
}

static ssize_t
llistxattr(const char *path, char *list, size_t size)
{
	return SYSCALL(__NR_llistxattr, path, list, size);
}

/*
 * Dump code starts here :)
 */

static int
xattr_cb_list(char *name, char *value, int valuelen, void *private)
{
	value[valuelen] = '\0';
	printf("EA: %s:%s\n", name, value);

	return GOOD;
}

static int
xattr_cb_set(char *name, char *value, int valuelen, void *private)
{
	char *path = (char *)private;

	if (lsetxattr(path, name, value, valuelen, 0) < 0) {
		warn("lsetxattr %s failed", path);
		return FAIL;
	}
	return GOOD;
}

static int
xattr_cb_compare(char *name, char *value, int valuelen, void *private)
{
	char *path = (char *)private;
	char valuef[XATTR_MAXSIZE];
	int valuesz;

	valuesz = lgetxattr(path, name, valuef, XATTR_MAXSIZE);
	if (valuesz < 0) {
		warn("%s: lgetxattr failed\n", path);
		return FAIL;
	}

	if (valuesz != valuelen) {
		fprintf(stderr, "%s: EA %s value changed\n", path, value);
		return FAIL;
	}

	if (memcmp(value, valuef, valuelen)) {
		fprintf(stderr, "%s: EA %s value changed\n", path, value);
		return FAIL;
	}

	return GOOD;
}

static int
xattr_verify(char *buffer)
{
	struct ext2_xattr_entry *entry;
	char *end;

	end = buffer + XATTR_MAXSIZE;

	if (Bcvt)
		swabst("4i", (u_char *)buffer);

	if (HDR(buffer)->h_magic != EXT2_XATTR_MAGIC &&
	    HDR(buffer)->h_magic != EXT2_XATTR_MAGIC2) {
		fprintf(stderr, "error in EA block 1\n");
		fprintf(stderr, "magic = %x\n", HDR(buffer)->h_magic);
		
		return FAIL;
	}

	/* check the on-disk data structure */
	entry = FIRST_ENTRY(buffer);
	if (Bcvt) 
		swabst("2b1s3i", (u_char *)entry);
	while (!IS_LAST_ENTRY(entry)) {
		struct ext2_xattr_entry *next = EXT2_XATTR_NEXT(entry);

		if ((char *)next >= end) {
			fprintf(stderr, "error in EA block\n");
			return FAIL;
		}
		entry = next;
		if (Bcvt) 
			swabst("2b1s3i", (u_char *)entry);
	}
	return GOOD;
}

static int
xattr_count(char *buffer, int *count)
{
	struct ext2_xattr_entry *entry;
	int result = 0;

	/* list the attribute names */
	for (entry = FIRST_ENTRY(buffer); !IS_LAST_ENTRY(entry);
	     entry = EXT2_XATTR_NEXT(entry))
		result++;

	*count = result;
	return GOOD;
}

static int
xattr_walk(char *buffer, int (*xattr_cb)(char *, char *, int, void *), void *private)
{
	struct ext2_xattr_entry *entry;

	/* list the attribute names */
	for (entry = FIRST_ENTRY(buffer); !IS_LAST_ENTRY(entry);
	     entry = EXT2_XATTR_NEXT(entry)) {
	     	char name[XATTR_MAXSIZE], value[XATTR_MAXSIZE];
		int off;

		switch (entry->e_name_index) {
		case EXT2_XATTR_INDEX_USER:
			strcpy(name, "user.");
			break;
		case EXT2_XATTR_INDEX_POSIX_ACL_ACCESS:
		case EXT2_XATTR_INDEX_POSIX_ACL_DEFAULT:
			strcpy(name, "system.");
			break;
		case EXT2_XATTR_INDEX_TRUSTED:
			strcpy(name, "trusted.");
			break;
		case EXT2_XATTR_INDEX_LUSTRE:
			strcpy(name, "lustre.");
			break;
		case EXT2_XATTR_INDEX_SECURITY:
			strcpy(name, "security.");
			break;
		default:
			fprintf(stderr, "Unknown EA index\n");
			return FAIL;
		}

		off = strlen(name);
		memcpy(name + off, entry->e_name, entry->e_name_len);
		name[off + entry->e_name_len] = '\0';


		memcpy(value, buffer + VALUE_OFFSET(buffer, entry), entry->e_value_size);

		if (xattr_cb(name, value, entry->e_value_size, private) != GOOD)
			return FAIL;
	}

	return GOOD;
}

int
xattr_compare(char *path, char *buffer)
{
	int countf, countt;
	char *names = NULL, *end_names, *name;

	countf = llistxattr(path, NULL, 0);
	if (countf < 0) {
		warn("%s: llistxattr failed", path);
		return FAIL;
	}

	names = malloc(countf + 1);
	if (!names) {
		warn("%s: llistxattr failed", path);
		return FAIL;
	}

	countf = llistxattr(path, names, countf);
	if (countf < 0) {
		warn("%s: llistxattr failed", path);
		free(names);
		return FAIL;
	}

	names[countf] = '\0';
	end_names = names + countf;

	countf = 0;
	for (name = names; name != end_names; name = strchr(name, '\0') + 1) {
		if (!*name)
			continue;
		countf++;
	}

	free(names);

	if (buffer) {
		if (xattr_verify(buffer) == FAIL)
			return FAIL;

		if (xattr_count(buffer, &countt) == FAIL)
			return FAIL;
	}
	else
		countt = 0;

	if (countf != countt) {
		fprintf(stderr, "%s: EA count changed from %d to %d\n", path, countt, countf);
		return FAIL;
	}

	if (!buffer)
		return GOOD;

	return xattr_walk(buffer, xattr_cb_compare, path);
}

int
xattr_extract(char *path, char *buffer)
{
	if (dflag) {
		fprintf(stderr, "xattr_extract(%s)\n", path);
		xattr_walk(buffer, xattr_cb_list, NULL);
	}

	if (xattr_verify(buffer) == FAIL)
		return FAIL;

	return xattr_walk(buffer, xattr_cb_set, path);
}

