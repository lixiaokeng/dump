/*
 *	Ported to Linux's Second Extended File System as part of the
 *	dump and restore backup suit
 *	Remy Card <card@Linux.EU.Org>, 1994-1997
 *	Stelian Pop <stelian@popies.net>, 1999-2000
 *	Stelian Pop <stelian@popies.net> - Alcôve <www.alcove.com>, 2000-2002
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
#ifdef DUMP_MACOSX
#include "darwin.h"
#endif

struct entry	*addentry (char *, dump_ino_t, int);
long		 addfile (char *, dump_ino_t, int);
int		 addwhiteout (char *);
void		 badentry (struct entry *, const char *);
void	 	 canon (char *, char *, int);
void		 checkrestore (void);
void		 closemt (void);
void		 cleanup (void);
void		 comparefile (char *);
void		 compareleaves (void);
void		 createfiles (void);
void		 createleaves (char *);
void		 createlinks (void);
long		 deletefile (char *, dump_ino_t, int);
void		 deleteino (dump_ino_t);
void		 delwhiteout (struct entry *);
dump_ino_t	 dirlookup (const char *);
void		 dumpsymtable (char *, long);
void	 	 extractdirs (int);
int		 extractfile (struct entry *, int);
void		 findunreflinks (void);
char		*flagvalues (struct entry *);
void		 freeentry (struct entry *);
void		 freename (char *);
int	 	 genliteraldir (char *, dump_ino_t);
char		*gentempname (struct entry *);
void		 getfile (void (*)(char *, size_t), void (*)(char *, size_t));
void		 getvol (long);
void		 initsymtable (char *);
int	 	 inodetype (dump_ino_t);
int		 linkit (char *, char *, int);
struct entry	*lookupino (dump_ino_t);
struct entry	*lookupname (char *);
long		 listfile (char *, dump_ino_t, int);
dump_ino_t	 lowerbnd (dump_ino_t);
void		 mktempname (struct entry *);
void		 moveentry (struct entry *, char *);
void		 msg (const char *, ...);
char		*myname (struct entry *);
void		 newnode (struct entry *);
void		 newtapebuf (long);
long		 nodeupdates (char *, dump_ino_t, int);
void	 	 onintr (int);
void		 panic (const char *, ...);
void		 pathcheck (char *);
struct direct	*pathsearch (const char *);
void		 printdumpinfo (void);
void		 printvolinfo (void);
void		 removeleaf (struct entry *);
void		 removenode (struct entry *);
void		 removeoldleaves (void);
void		 removeoldnodes (void);
void		 renameit (char *, char *);
int		 reply (const char *);
void		 resizemaps (dump_ino_t, dump_ino_t);
RST_DIR		*rst_opendir (const char *);
struct direct	*rst_readdir (RST_DIR *);
void		 rst_closedir (RST_DIR *dirp);
void	 	 runcmdshell (void);
char		*savename (char *);
void	 	 setdirmodes (int);
void		 comparedirmodes (void);
void		 setinput (char *);
void		 setup (void);
void	 	 skipdirs (void);
void		 skipfile (void);
void		 skipmaps (void);
void		 swabst (u_char *, u_char *);
void	 	 treescan (char *, dump_ino_t, long (*)(char *, dump_ino_t, int));
dump_ino_t	 upperbnd (dump_ino_t);
long		 verifyfile (char *, dump_ino_t, int);
void		 xtrnull (char *, size_t);

/* From ../dump/dumprmt.c */
void		rmtclose (void);
int		rmthost (const char *);
int		rmtioctl (int, int);
int		rmtopen (const char *, const int);
int		rmtread (const char *, int);
off_t		rmtseek (off_t, int);

/* From e2fsprogs */
int fsetflags (const char *, unsigned long);
int fgetflags (const char *, unsigned long *);
int setflags (int, unsigned long);

int lsetflags (const char *, unsigned long);
int lgetflags (const char *, unsigned long *);

#ifdef USE_QFA
int	Inode2Tapepos (dump_ino_t, long *, long long *, int);
int	GetTapePos (long long *);
int	GotoTapePos (long long);
void	ReReadFromTape (void);
void	ReReadInodeFromTape (dump_ino_t);
void    GetPathFile (char *, char *, char *);

#ifdef sunos
int		GetSCSIIDFromPath (char *, long *);
int		OpenSMTCmt(char *);
#endif
#endif
void	RequestVol (long);

#ifdef DUMP_MACOSX
int	extractfinderinfoufs (char *);
int	extractresourceufs (char *);
int	CreateAppleDoubleFileRes (char *, FndrFileInfo *, mode_t, int, struct timeval *, u_int32_t, u_int32_t);
#endif

void	skipxattr (void);
int	readxattr (char *);
int	xattr_compare (char *, char *);
int	xattr_extract (char *, char *);
