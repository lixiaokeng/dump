/*
 *	Ported to Linux's Second Extended File System as part of the
 *	dump and restore backup suit
 *	Remy Card <card@Linux.EU.Org>, 1994-1997
 *	Stelian Pop <pop@cybercable.fr>, 1999-2000
 *	Stelian Pop <pop@cybercable.fr> - Alcôve <www.alcove.fr>, 2000
 *
 *	$Id: compaterr.h,v 1.7 2000/11/10 14:42:24 stelian Exp $
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

#ifndef _COMPATERR_H_
#define	_COMPATERR_H_

#include <config.h>

#if defined(HAVE_ERR) || defined(HAVE_ERRX) || defined(HAVE_VERR) || defined(HAVE_VERRX) || defined(HAVE_VWARN) || defined(HAVE_VWARNX) || defined(HAVE_WARN) || defined(HAVE_WARNX)
#include <err.h>
#endif

#include <sys/cdefs.h>

#include <stdarg.h>

#ifndef	_BSD_VA_LIST_
#define _BSD_VA_LIST_		va_list
#endif

#ifndef	__dead
#define __dead			volatile
#endif

__BEGIN_DECLS
#ifndef HAVE_ERR
__dead void	err __P((int, const char *, ...));
#endif
#ifndef HAVE_VERR
__dead void	verr __P((int, const char *, _BSD_VA_LIST_));
#endif
#ifndef HAVE_ERRX
__dead void	errx __P((int, const char *, ...));
#endif
#ifndef HAVE_VERRX
__dead void	verrx __P((int, const char *, _BSD_VA_LIST_));
#endif
#ifndef HAVE_WARN
void		warn __P((const char *, ...));
#endif
#ifndef HAVE_VWARN
void		vwarn __P((const char *, _BSD_VA_LIST_));
#endif
#ifndef HAVE_WARNX
void		warnx __P((const char *, ...));
#endif
#ifndef HAVE_VWARNX
void		vwarnx __P((const char *, _BSD_VA_LIST_));
#endif
__END_DECLS

#endif /* !_COMPATERR_H_ */
