#include <stdio.h>
#include <config.h>

#include "transformation.h"

/*
 * Initialize
 */
static int
null_initialize(Transformation *xform, int enc)
{
	return 0;
}

/*
 * Shut down.
 */
static int
null_shutdown(Transformation *xform)
{
	return 0;
}

/*
 * Handle fork.
 */
static int
null_startNewTape(Transformation *xform, struct tapebuf *tpbin,
	unsigned long *destlen)
{
	return 0;
}

/*
 * Start slave process.
 */
static int
null_startDiskIOProcess(Transformation *xform)
{
	return 0;
}

/*
 * End slave process.
 */
static int
null_endDiskIOProcess(Transformation *xform)
{
	return 0;
}

/*
 * Compress a buffer.
 */
static int
null_compress(Transformation *xform, struct tapebuf *tpbin, unsigned long *destlen,
		const char *src, int srclen)
{
	memcpy(tpbin->buf, src, srclen);
	*destlen = srclen;

	return 1;
}

/*
 * Decompress a buffer.
 */
static int
null_decompress(Transformation *xform, struct tapebuf *tpbin, unsigned long *destlen,
		const char *src, int srclen, char **reason)
{
	memcpy(tpbin->buf, src, srclen);
	*destlen = srclen;

	return 1;
}

/*
 *
 */
Transformation transformation_null =
{
	0,
	NULL,
	"null",
	0,
	&null_initialize,
	&null_shutdown,
	&null_startNewTape,
	&null_startDiskIOProcess,
	&null_endDiskIOProcess,
	&null_compress,
	&null_decompress
};
