/*
 *	Ported to Linux's Second Extended File System as part of the
 *	dump and restore backup suit
 *	1999-02-22 Arkadiusz Mi¶kiewicz <misiek@misiek.eu.org>
 *	 - added Native Language Support
 *	2000-01-20 James Antill <james@and.org>
 *	 - Added error message if /proc/partitions cannot be opened
 *	2000-05-09 Erik Troan <ewt@redhat.com>
 *	- Added cache for UUID and disk labels
 *	Wed Aug 16 2000 Erik Troan <ewt@redhat.com>
 *	- Ported to dump/restore
 *	Stelian Pop <pop@noos.fr> - Alcôve <www.alcove.fr>, 2000
 *
 *	$Id: bylabel.h,v 1.4 2000/12/21 11:14:53 stelian Exp $
 */

#ifndef _BYLABEL_H_
#define	_BYLABEL_H_

#include <config.h>

const char * get_device_name(const char * item);
const char * get_device_label(const char * spec);

#endif /* !_BYLABEL_H_ */
