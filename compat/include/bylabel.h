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
 *	Stelian Pop <stelian@popies.net> - Alcôve <www.alcove.com>, 2000-2002
 *
 *	$Id: bylabel.h,v 1.6 2004/07/05 15:02:36 stelian Exp $
 */

#ifndef _BYLABEL_H_
#define	_BYLABEL_H_

#include <config.h>

#ifdef HAVE_BLKID

#include <blkid/blkid.h>

static inline const char * get_device_name(const char * item) {
	return blkid_get_devname(NULL, item, NULL);
}

static inline const char * get_device_label(const char * spec) {
	return blkid_get_tag_value(NULL, "LABEL", spec);
}
	
#else

const char * get_device_name(const char * item);
const char * get_device_label(const char * spec);

#endif

#endif /* !_BYLABEL_H_ */
