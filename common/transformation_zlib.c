#include <stdio.h>
#include <config.h>

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif /* HAVE_ZLIB */

#include "transformation.h"

/*
 * Initialize
 */
static int
zlib_initialize(Transformation *xform)
{
	return 0;
}

/*
 * Shut down.
 */
static int
zlib_shutdown(Transformation *xform)
{
	return 0;
}

/*
 * Handle forks.
 */
static int
zlib_startNewTape(Transformation *xform, struct tapebuf *bin,
	unsigned long *destlen)
{
	return 0;
}

/*
 * Start slave process
 */
static int
zlib_startDiskIOProcess(Transformation *xform)
{
	return 0;
}

/*
 * End slave process
 */
static int
zlib_endDiskIOProcess(Transformation *xform)
{
	if (xform != NULL) {
		free(xform);
	}

	return 0;
}

struct req {
	ext2_loff_t dblk;
	int count;
};

/*
 * Compress a buffer.
 */
static int
zlib_compress(Transformation *xform, struct tapebuf *tpbin,
		unsigned long *destlen, const char *src, int srclen)
{
#ifdef HAVE_ZLIB
	int compresult;
	compresult = compress2(tpbin->buf, destlen, src, srclen, xform->state.zlib.complvl);
	return compresult == Z_OK ? 1 : 0;
#else
	return 1;
#endif /* HAVE_ZLIB */
}

/*
 * Decompress a buffer.
 */
static int
zlib_decompress(Transformation *xform, struct tapebuf *tpbin,
	unsigned long *destlen, const char *src, int srclen, char **reason)
{
#ifdef HAVE_ZLIB
	int cresult;
	cresult = uncompress(tpbin->buf, destlen, src, srclen);
	switch (cresult) {
		case Z_OK:
			*reason = "";
			break;
		case Z_MEM_ERROR:
			*reason = "not enough memory";
			break;
		case Z_BUF_ERROR:
			*reason = "buffer too small";
			break;
		case Z_DATA_ERROR:
			*reason = "data error";
			break;
		default:
			*reason = "unknown";
	}
	return (cresult == Z_OK) ? 1 : 0;
#else
	return 1;
#endif /* HAVE_ZLIB */
}


/*
 * Factory
 */
Transformation
*transformation_zlib_factory(int enc, int complvl)
{
	Transformation *t = (Transformation *) malloc(sizeof (Transformation));

	t->enc = enc;
#ifdef HAVE_ZLIB
	t->state.zlib.complvl = complvl;
#endif

	t->name = "zlib";
	t->mandatory = 0;
	t->initialize = &zlib_initialize;
	t->shutdown = &zlib_shutdown;
	t->startNewTape = &zlib_startNewTape;
	t->startDiskIOProcess = &zlib_startDiskIOProcess;
	t->endDiskIOProcess = &zlib_endDiskIOProcess;
	t->compress = &zlib_compress;
	t->decompress = &zlib_decompress;

	return t;
}
