#include <stdlib.h>
#include <config.h>

#ifdef HAVE_LZO
#include <lzo/lzo1x.h>
#endif /* HAVE_LZO */

#include "transformation.h"

/*
 * Initialize
 */
static int
lzo_initialize(Transformation *xform, int enc)
{
}

/*
 * Shut down.
 */
static int
lzo_shutdown(Transformation *xform)
{
	return 0;
}

/*
 * Handle forks.
 */
static int
lzo_startNewTape(Transformation *xform, struct tapebuf *tpbin,
	unsigned long *destlen)
{
	return 0;
}

/*
 * Start slave process.
 */
static int
lzo_startDiskIOProcess(Transformation *xform)
{
	return 0;
}

/*
 * End slave process.
 */
static int
lzo_endDiskIOProcess(Transformation *xform)
{
	if (xform != NULL) {
#ifdef HAVE_LZO
		if (xform->state.lzo.LZO_WorkMem != NULL) {
			free(xform->state.lzo.LZO_WorkMem);
		}
#endif /* HAVE_LZO */
		free(xform);
	}

	return 0;
}

/*
 * Compress a buffer.
 */
static int
lzo_compress(Transformation *xform, struct tapebuf *tpbin,
		unsigned long *destlen, const char *src, int srclen)
{
#ifdef HAVE_LZO
	lzo_uint *worklen2 = (lzo_uint *) destlen;
	int compresult;

	compresult = lzo1x_1_compress(src, srclen, tpbin->buf, worklen2,
			xform->state.lzo.LZO_WorkMem);

	return compresult == LZO_E_OK ? 1 : 0;
#else
	return 1;
#endif /* HAVE_LZO */
}

/*
 * Decompress a buffer.
 */
static int
lzo_decompress(Transformation *xform, struct tapebuf *tpbin,
		unsigned long *destlen, const char *src, int srclen, char **reason)
{
#ifdef HAVE_LZO
	int cresult = 0;
	lzo_uint destlen2 = *destlen;
	cresult = lzo1x_decompress(src, srclen, tpbin->buf, &destlen2, NULL);
	*destlen = destlen2;
	switch (cresult) {
	case LZO_E_OK:
		*reason = "";
		break;
	case LZO_E_ERROR:
	case LZO_E_EOF_NOT_FOUND:
		*reason = "data error";
		break;
	default:
		*reason = "unknown";
	}

	return (cresult == LZO_E_OK) ? 1 : 0;
#else
	return 1;
#endif /* HAVE_LZO */
}

/*
 * Factory
 */
Transformation
*transformation_lzo_factory(int enc)
{
	Transformation *t = (Transformation *) malloc(sizeof (Transformation));

#ifdef HAVE_LZO
	t->state.lzo.LZO_WorkMem = malloc(LZO1X_1_MEM_COMPRESS);
	if (!t->state.lzo.LZO_WorkMem)
		quit("couldn't allocate a compress buffer.\n");
#endif /* HAVE_LZO */

	t->name = "lzo";
	t->mandatory = 0;
	t->initialize = &lzo_initialize;
	t->shutdown = &lzo_shutdown;
	t->startNewTape = &lzo_startNewTape;
	t->startDiskIOProcess = &lzo_startDiskIOProcess;
	t->endDiskIOProcess = &lzo_endDiskIOProcess;
	t->compress = &lzo_compress;
	t->decompress = &lzo_decompress;

	return t;
}
