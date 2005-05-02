/*
 *	Ported to Linux's Second Extended File System as part of the
 *	dump and restore backup suit
 *	Remy Card <card@Linux.EU.Org>, 1994-1997
 *	Stelian Pop <stelian@popies.net>, 1999-2000
 *	Stelian Pop <stelian@popies.net> - Alc�ve <www.alcove.com>, 2000-2002
 *
 *	$Id: extern.h,v 1.25 2005/05/02 15:10:46 stelian Exp $
 */

/*-
 * Copyright (c) 1992, 1993
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
#include <compatlfs.h>
#ifndef __P
#include <sys/cdefs.h>
#endif
#ifdef DUMP_MACOSX
#include "darwin.h"
#endif

struct entry	*addentry __P((char *, dump_ino_t, int));
long		 addfile __P((char *, dump_ino_t, int));
int		 addwhiteout __P((char *));
void		 badentry __P((struct entry *, const char *));
void	 	 canon __P((char *, char *, int));
void		 checkrestore __P((void));
void		 closemt __P((void));
void		 cleanup __P((void));
void		 comparefile __P((char *));
void		 compareleaves __P((void));
void		 createfiles __P((void));
void		 createleaves __P((char *));
void		 createlinks __P((void));
long		 deletefile __P((char *, dump_ino_t, int));
void		 deleteino __P((dump_ino_t));
void		 delwhiteout __P((struct entry *));
dump_ino_t	 dirlookup __P((const char *));
void		 dumpsymtable __P((char *, long));
void	 	 extractdirs __P((int));
int		 extractfile __P((struct entry *, int));
void		 findunreflinks __P((void));
char		*flagvalues __P((struct entry *));
void		 freeentry __P((struct entry *));
void		 freename __P((char *));
int	 	 genliteraldir __P((char *, dump_ino_t));
char		*gentempname __P((struct entry *));
void		 getfile __P((void (*)(char *, size_t), void (*)(char *, size_t)));
void		 getvol __P((long));
void		 initsymtable __P((char *));
int	 	 inodetype __P((dump_ino_t));
int		 linkit __P((char *, char *, int));
struct entry	*lookupino __P((dump_ino_t));
struct entry	*lookupname __P((char *));
long		 listfile __P((char *, dump_ino_t, int));
dump_ino_t	 lowerbnd __P((dump_ino_t));
void		 mktempname __P((struct entry *));
void		 moveentry __P((struct entry *, char *));
void		 msg __P((const char *, ...));
char		*myname __P((struct entry *));
void		 newnode __P((struct entry *));
void		 newtapebuf __P((long));
long		 nodeupdates __P((char *, dump_ino_t, int));
void	 	 onintr __P((int));
void		 panic __P((const char *, ...));
void		 pathcheck __P((char *));
struct direct	*pathsearch __P((const char *));
void		 printdumpinfo __P((void));
void		 printvolinfo __P((void));
void		 removeleaf __P((struct entry *));
void		 removenode __P((struct entry *));
void		 removeoldleaves __P((void));
void		 removeoldnodes __P((void));
void		 renameit __P((char *, char *));
int		 reply __P((const char *));
void		 resizemaps __P((dump_ino_t, dump_ino_t));
RST_DIR		*rst_opendir __P((const char *));
struct direct	*rst_readdir __P((RST_DIR *));
void		 rst_closedir __P((RST_DIR *dirp));
void	 	 runcmdshell __P((void));
char		*savename __P((char *));
void	 	 setdirmodes __P((int));
void		 comparedirmodes __P((void));
void		 setinput __P((char *));
void		 setup __P((void));
void	 	 skipdirs __P((void));
void		 skipfile __P((void));
void		 skipmaps __P((void));
void		 swabst __P((u_char *, u_char *));
void	 	 treescan __P((char *, dump_ino_t, long (*)(char *, dump_ino_t, int)));
dump_ino_t	 upperbnd __P((dump_ino_t));
long		 verifyfile __P((char *, dump_ino_t, int));
void		 xtrnull __P((char *, size_t));

/* From ../dump/dumprmt.c */
void		rmtclose __P((void));
int		rmthost __P((const char *));
int		rmtioctl __P((int, int));
int		rmtopen __P((const char *, const int));
int		rmtread __P((const char *, int));
OFF_T		rmtseek __P((OFF_T, int));

/* From e2fsprogs */
int fsetflags __P((const char *, unsigned long));
int fgetflags __P((const char *, unsigned long *));
int setflags __P((int, unsigned long));

int lsetflags __P((const char *, unsigned long));
int lgetflags __P((const char *, unsigned long *));

#ifdef USE_QFA
int	Inode2Tapepos __P((dump_ino_t, long *, long long *, int));
int	GetTapePos __P((long long *));
int	GotoTapePos __P((long long));
void	ReReadFromTape __P((void));
void	ReReadInodeFromTape __P((dump_ino_t));
void    GetPathFile __P((char *, char *, char *));

#ifdef sunos
int		GetSCSIIDFromPath __P((char *, long *));
int		OpenSMTCmt(char *);
#endif
#endif
void	RequestVol __P((long));

#ifdef DUMP_MACOSX
int	extractfinderinfoufs __P((char *));
int	extractresourceufs __P((char *));
int	CreateAppleDoubleFileRes __P((char *, FndrFileInfo *, mode_t, int, struct timeval *, u_int32_t, u_int32_t));
#endif

void	skipxattr __P((void));
int	readxattr __P((char *));
int	xattr_compare __P((char *, char *));
int	xattr_extract __P((char *, char *));
