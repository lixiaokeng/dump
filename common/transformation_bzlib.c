#include <stdio.h>
#include <config.h>

#ifdef HAVE_BZLIB
#include <bzlib.h>
#endif /* HAVE_BZLIB */

#include "transformation.h"

/*
 * Initialize
 */
static int
bzlib_initialize(Transformation *xform, int enc)
{
	return 0;
}

/*
 * Shut down.
 */
static int
bzlib_shutdown(Transformation *xform)
{
	return 0;
}

/*
 * Handle forks.
 */
static int
bzlib_startNewTape(Transformation *xform, struct tapebuf *tpbin,
	unsigned long *destlen)
{
	return 0;
}

/*
 * Start slave process.
 */
static int
bzlib_startDiskIOProcess(Transformation *xform)
{
	return 0;
}

/*
 * End slave process.
 */
static int
bzlib_endDiskIOProcess(Transformation *xform) {
	if (xform != NULL) {
		free(xform);
	}

	return 0;
}

/*
 * Compress a buffer.
 */
static int
bzlib_compress(Transformation *xform, struct tapebuf *tpbin, unsigned long *destlen,
		const char *src, int srclen) {
#ifdef HAVE_BZLIB
	int compresult;
	unsigned int destlen2 = *destlen;
	compresult = BZ2_bzBuffToBuffCompress(
			tpbin->buf,
			&destlen2,
			(char *) src,
			srclen,
			xform->state.bzlib.complvl,
			0, 30);
	*destlen = destlen2;
	return compresult == BZ_OK ? 1 : 0;
#else
	return 1;
#endif /* HAVE_BZLIB */
}

/*
 * Decompress a buffer.
 */
static int
bzlib_decompress(Transformation *xform, struct tapebuf *tpbin,
		unsigned long *destlen, const char *src, int srclen, char **reason) {
#ifdef HAVE_BZLIB
	int cresult;
	unsigned int destlen2 = *destlen;
	cresult = BZ2_bzBuffToBuffDecompress(tpbin->buf, &destlen2, (char *) src, srclen,
			0, 0);
	*destlen = destlen2;
	switch (cresult) {
	case BZ_OK:
		*reason = "";
		break;
	case BZ_MEM_ERROR:
		*reason = "not enough memory";
		break;
	case BZ_OUTBUFF_FULL:
		*reason = "buffer too small";
		break;
	case BZ_DATA_ERROR:
	case BZ_DATA_ERROR_MAGIC:
	case BZ_UNEXPECTED_EOF:
		*reason = "data error";
		break;
	default:
		*reason = "unknown";
	}
	return (cresult == BZ_OK) ? 1 : 0;
#else
	return 1;
#endif
}

/*
 * Factory
 */
Transformation
*transformation_bzlib_factory(int enc, int complvl)
{
	Transformation *t = (Transformation *) malloc(sizeof (Transformation));

	t->enc = enc;
#ifdef HAVE_BZLIB
	t->state.bzlib.complvl = complvl;
#endif

	t->name = "bzlib";
	t->mandatory = 0;
	t->initialize = &bzlib_initialize;
	t->shutdown = &bzlib_shutdown;
	t->startNewTape = &bzlib_startNewTape;
	t->startDiskIOProcess = &bzlib_startDiskIOProcess;
	t->endDiskIOProcess = &bzlib_endDiskIOProcess;
	t->compress = &bzlib_compress;
	t->decompress = &bzlib_decompress;

	return t;
}
