#include <config.h>
#include <bsdcompat.h>
#include <protocols/dumprestore.h>

#ifdef HAVE_LZO
#include <lzo/lzo1x.h>
#endif /* HAVE_LZO */

#ifdef HAVE_OPENSSL
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/pkcs12.h>
#endif

#ifndef _transformation_H
#define _transformation_H

/*
 * Compression/encryption hooks.
 *
 * Open questions:
 * 1. should it be a failure if compress/decompress is called and we DON'T have the code included?
 */

typedef struct transformation {
	int enc;
	union {
#ifdef HAVE_LZO
		struct {
			lzo_align_t __LZO_MMODEL *LZO_WorkMem;
		} lzo;
#endif /* HAVE_LZO */
#ifdef HAVE_ZLIB
		struct {
			int complvl;
		} zlib;
#endif /* HAVE_ZLIB */
#ifdef HAVE_BZLIB
		struct {
			int complvl;
		} bzlib;
#endif /* HAVE_ZLIB */
#ifdef HAVE_OPENSSL
		struct {
			int complvl;

			// encryption/decryption key
			unsigned char key[EVP_MAX_KEY_LENGTH];
			unsigned int keylen;

			// crypto
			const EVP_CIPHER *cipher;
			const EVP_MD *digest;
			ENGINE *engine;

			// this assumes we're multi-process but not multi-threaded
			EVP_CIPHER_CTX *dataCtx;
			EVP_CIPHER_CTX *ivCtx;
		} ssl;
#endif
	} state;

	/*
	 * The name of the compression/encryption algorithm, for
	 * display purposes.
	 */
	const char *name;

	/*
	 * Is this mandatory even if the size of the buffer increases?
	 * As a general rule compression is optional * and encryption is
	 * mandatory.
	 */
	int mandatory;

	/*
	 * Initialize the system.
	 * (mode: 1 = compress/encrypt, 0 = decompress/decrypt)
	 */
	int (*initialize) (struct transformation *xform, int mode);

	/*
	 * Shut down the system
	 */
	int (*shutdown) (struct transformation *xform);

	/*
	 * Do anything necessary after forking the process.
	 */
	int (*startNewTape) (struct transformation *xform,
		struct tapebuf *tpbin, unsigned long *destlen);

	/*
	 * The dump process is master-slave with the actual
	 * disk and dump tape access handled by the slave processes.
	 * This method performs any initialization required by
	 * the latter process. (E.g., some encryption libraries
	 * must be reinitialized.)
	 */
	int (*startDiskIOProcess) (struct transformation *xform);

	/*
	 * Clean up everything before the slave process ends.
	 */
	int (*endDiskIOProcess) (struct transformation *xform);

	/*
	 * Compress/encrypt buffer.
	 */
	int (*compress) (struct transformation *xform, struct tapebuf *, unsigned long *destlen,
			const char *src, int srclen);

	/*
	 * Decompress/decrypt buffer.
	 */
	int (*decompress) (struct transformation *xform, struct tapebuf *, unsigned long *destlen,
			const char *src, int srclen, char **reason);

} Transformation;

extern Transformation transformation_null;

#ifdef HAVE_LZO
extern Transformation *transformation_lzo_factory(int enc);
#endif /* HAVE_ZLIB */

#ifdef HAVE_ZLIB
extern Transformation *transformation_zlib_factory(int enc, int complvl);
#endif /* HAVE_ZLIB */

#ifdef HAVE_BZLIB
extern Transformation *transformation_bzlib_factory(int enc, int complvl);
#endif /* HAVE_BZLIB */

#ifdef HAVE_OPENSSL
extern Transformation *transformation_ssl_factory(int enc, int complvl,
		const char *ciphername, const char *digestname);
#endif /* HAVE_OPENSSL */

void quit(const char *fmt, ...);

#endif /* _transformation_H */
