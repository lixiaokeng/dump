/*
 *	Ported to Linux's Second Extended File System as part of the
 *	dump and restore backup suit
 *	Stelian Pop <stelian@popies.net> - Alcôve <www.alcove.com>, 2000-2002
 *
 *	$Id: compatlfs.h,v 1.6 2005/07/07 08:47:16 stelian Exp $
 */

/*-
 * Copyright (c) 1993
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

#ifndef _COMPATLFS_H_
#define	_COMPATLFS_H_

#include <config.h>

#ifdef USE_LFS

#define _LARGEFILE64_SOURCE
#define OPEN open64
#define FOPEN fopen64
#define LSEEK lseek64
#define STAT stat64
#define FSTAT fstat64
#define LSTAT lstat64
#define FTRUNCATE ftruncate64
#define OFF_T __off64_t
#define MKSTEMP mkstemp64
#define FTELL ftello64

#else

#define OPEN open
#define FOPEN fopen
#define LSEEK lseek
#define STAT stat
#define FSTAT fstat
#define LSTAT lstat
#define FTRUNCATE ftruncate
#define OFF_T off_t
#define MKSTEMP mkstemp
#define FTELL ftell

#endif /* USE_LFS */

#endif /* !_COMPATLFS_H_ */
