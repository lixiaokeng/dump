#include <stdio.h>
#include <config.h>
#include <sys/mman.h>

#ifdef HAVE_OPENSSL
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/pkcs12.h>
#endif

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif /* HAVE_ZLIB */

#include "transformation.h"

/*
 * IMPORTANT: this is an important piece of the puzzle for native
 * support of encrypted backups but Key management is much more
 * important and much harder. The best encryption algorithms in the
 * world won't help you if you have to reveal your one key to everyone.
 *
 * IMPORTANT: always comply with local laws! Do not modify the source
 * to give you stronger encryption than permitted by local law.
 *
 * -----------------------
 * 30-second cryptanalysis
 * -----------------------
 *
 * This is a known-plaintext problem since many of the encrypted blocks
 * will start with a BSD inode structure. (It will be compressed binary
 * data but still (mostly) known.) This means that it is very important
 * to choose a strong IV.
 *
 * The dump tape/file is written by multiple slave processes so we don't
 * have ready access to an intrinsic value such as tape position. That
 * leaves random salts that are included in the dump buffer. The salt
 * doesn't need to be cryptographically strong since it will be published.
 *
 * The IV is created by calculating the hash of the salt, a function of
 * the salt and session key, and the salt again.
 *
 * The greatest vulnerability is predictable keys (and weak ciphers
 * required by local laws). The key should also be protected in memory
 * while dumping and restoring files but it's not as critical since
 * anyone able to read the key probably has the ability to read the 
 * filesystem directly.
 */

/*
 * Initialize.
 *
 * 1. load policy file, select ciphers based on it.
 * 2. PKCS12 keystore contains certificate (public keys) and
 *    private keys.
 * 3. private keys can be password-protected individually, and
 *    you can also password-protect the entire keystore.
 * 4. PKCS12 keystores can be manipulated by openssl, java,
 *    windows apps, more.
 *
 * questions:
 * 1. how to select which keystore? (command line option passed
 *    in as function argument?
 * 2. how to select which key, if doing restore?
 * 3. how to pass keystore password(s), if doing restore?
 */
static int
ssl_initialize(Transformation *xform, int enc)
{
#ifdef HAVE_OPENSSL
	int keylen;

	OpenSSL_add_all_algorithms(); // see below */
	OpenSSL_add_all_ciphers(); // see below */
	OpenSSL_add_all_digests(); // see below */
	ERR_load_crypto_strings();
	ERR_load_RAND_strings();
	ERR_load_ERR_strings();

	// read policy file and call those two methods to load acceptable
	// ciphers and digests. We always want CBC mode, not ECB mode.

/*
	If we are encrypting we need to
  	    1. create a random session key (good random numbers are critical!)
	    2. read the X509 certificates for everyone who needs to be able to read the dump file -
	        extract the public keys. These certificates may be in a PKCS12 keystore which many
	        applications use. OpenSSL also provides a large number of programs for it.
	    3. encrypt the session key with the public keys and put it into another PKCS12 keystore.
	        This keystore will be written to the tape in startnewtape().
	    4. set up the cipher context.

	If we are decrypting we need to
	    1. open our keystore(PKCS12) containing our private keys. The keystore itself may be
	        password protected, the individual keys definitely should be.
	    2. read the PKCS12 keystore from the tape header.
	    3. look for matching key ids. If we can't find a match tell the user.
	    4. we still need to be able to open the private key so get the password somewhere.
	    5. set up the cipher context.
*/

	// RAND_egd(path);

#endif /* HAVE_OPENSSL */

	return 0;
}

/*
 * Shut down.
 *
 * The OpenSSL library may be using a physical crypto device so
 * we need to make sure we properly release it.
 */
static int
ssl_shutdown(Transformation *xform)
{
#ifdef HAVE_OPENSSL
	OPENSSL_cleanse(xform->state.ssl.key, sizeof xform->state.ssl.key);

	munlock(xform, sizeof(Transformation));
	free(xform);

	/* is this the right method? */
	EVP_cleanup();
	RAND_cleanup();
	free(xform);
#endif /* HAVE_OPENSSL */
	return 0;
}

/*
 * We need to write new crypto header containing the encrypted
 * session key.
 */
static int
ssl_startNewTape(Transformation *xform, struct tapebuf *tpbin,
	unsigned long *destlen)
{
#ifdef HAVE_OPENSSL
	/* write new TS containing PKCS12 containing encrypted session key. */
#endif /* HAVE_OPENSSL */

	return 0;
}
 
/*
 * Start slave process. We need to reinitialize the encryption
 * engine.
 */
static int
ssl_startDiskIOProcess(Transformation *xform)
{
#ifdef HAVE_OPENSSL
	mlock(xform, sizeof(Transformation));

	OpenSSL_add_all_algorithms(); // see below */
	OpenSSL_add_all_ciphers(); // see below */
	OpenSSL_add_all_digests(); // see below */
	ERR_load_crypto_strings();
	ERR_load_RAND_strings();
	ERR_load_ERR_strings();

	// Initialize key information with values we obtained from parent
	// thread's startNewTape().

	xform->state.ssl.dataCtx = EVP_CIPHER_CTX_new();
	xform->state.ssl.ivCtx = EVP_CIPHER_CTX_new();
#endif /* HAVE_OPENSSL */

	return 0;
}

/*
 * End of slave process. Clear encryption keys, etc.
 */
static int
ssl_endDiskIOProcess(Transformation *xform)
{
#ifdef HAVE_OPENSSL
	EVP_CIPHER_CTX_cleanup(xform->state.ssl.dataCtx);
	EVP_CIPHER_CTX_cleanup(xform->state.ssl.ivCtx);
	EVP_CIPHER_CTX_free(xform->state.ssl.dataCtx);
	EVP_CIPHER_CTX_free(xform->state.ssl.ivCtx);

	OPENSSL_cleanse(xform->state.ssl.key, sizeof xform->state.ssl.key);

	munlock(xform, sizeof(Transformation));
	free(xform);

	/* is this the right method? */
	EVP_cleanup();
	RAND_cleanup();
#endif /* HAVE_OPENSSL */
	return 0;
}

/*
 * Method to generate 'random' salt and IV.
 *
 * The generated salt is 16 bytes long. It is duplicated to get a 256-bit value (to support
 * 256-bit block ciphers.)
 *
 */
#ifdef HAVE_OPENSSL
static int
generateIV(Transformation *xform, unsigned char *salt, unsigned int *saltlen,
	unsigned char *iv, unsigned int *ivlen)
{
	unsigned char ivbuffer[64];
	unsigned int buflen, y;

	/* we can use pseudorandom bytes since they're going */
	/* to be exposed to any attacker anyway. */
	*saltlen = 16;
	if (xform->enc == 1) {
		RAND_pseudo_bytes(salt, *saltlen);
	}
	memcpy(ivbuffer, salt, 16);

	/* -decrypt- salt value */
	memset(ivbuffer, 0, sizeof(ivbuffer));
	EVP_CipherInit_ex(xform->state.ssl.ivCtx, xform->state.ssl.cipher, xform->state.ssl.engine, NULL, NULL, 0);
	//EVP_CIPHER_CTX_set_key_length(&ctx, 8);
	EVP_CIPHER_CTX_set_padding(xform->state.ssl.ivCtx, 0);	// -nopad
	EVP_CipherInit_ex(xform->state.ssl.ivCtx, NULL, NULL, xform->state.ssl.key, ivbuffer, 0);
	buflen = 32;
	if (!EVP_CipherUpdate(xform->state.ssl.ivCtx, ivbuffer + 16, &buflen, salt, 16)) {
		y = 32 - buflen;
		if (!EVP_CipherFinal(xform->state.ssl.ivCtx, ivbuffer + 16 + buflen, &y)) {
			buflen += y;
		} else {
			memset(ivbuffer + 16, 0, 32);
		}
	} else {
		memset(ivbuffer + 16, 0, 32);
	}
	memcpy(ivbuffer + 48, salt, 16);

	/* now digest it. */
	EVP_Digest(ivbuffer, 64, iv, ivlen, xform->state.ssl.digest, NULL);

	return 0;
}
#endif


/*
 * Encrypt a single chunk of blocks. Each chunk is encrypted
 * independently so it's critically important to choose a good
 * initial vector (iv).
 *
 * The ciphertext format is:
 *  - 20 bytes of random salt
 *  - encrypted (plaintext . digest(plaintext))
 */
static int
ssl_compress(Transformation *xform, struct tapebuf *tpbin,
	unsigned long *destlen, const char *src, int srclen)
{
#ifdef HAVE_OPENSSL
	unsigned char salt[16], iv[EVP_MAX_MD_SIZE];
	unsigned int saltlen = sizeof(salt);
	unsigned int ivlen = sizeof(iv);
	unsigned char digest[EVP_MAX_MD_SIZE];
	unsigned int digestlen = 0;
	char *buf;
	unsigned int len, len2;

	len = srclen + 1000;
	buf = malloc(len);

	digestlen = sizeof(digest);

	/* generate salt, put it in header */
	generateIV(xform, salt, &saltlen, iv, &ivlen);
	memcpy(tpbin->buf, salt, saltlen);

	/* compress the buffer first - increase the entropy */
	int compresult;
	compresult = compress2(buf, destlen, src, srclen, xform->state.ssl.complvl);
	if (compresult != Z_OK) {
		printf("unable to compress...\n");
		return 0;
	}

	EVP_EncryptInit_ex(xform->state.ssl.dataCtx, xform->state.ssl.cipher,  xform->state.ssl.engine, NULL, NULL);
	//EVP_CIPHER_CTX_set_key_length(&ctx, 8);
	EVP_EncryptInit_ex(xform->state.ssl.dataCtx, NULL, NULL, xform->state.ssl.key, iv);

	// encrypt content.
	if (!EVP_EncryptUpdate(xform->state.ssl.dataCtx, tpbin->buf + saltlen, &len, buf, *destlen)) {
		EVP_CIPHER_CTX_cleanup(xform->state.ssl.dataCtx);
		ERR_print_errors_fp(stdout);
		free(buf);
		return 0;
	}

	// calculate digest, add it.
	EVP_Digest(src, srclen, digest, &digestlen, xform->state.ssl.digest,  xform->state.ssl.engine);

	len2 = *destlen - len - saltlen;
	if (!EVP_EncryptUpdate(xform->state.ssl.dataCtx, tpbin->buf + saltlen + len, &len2, digest, digestlen)) {
		EVP_CIPHER_CTX_cleanup(xform->state.ssl.dataCtx);
		ERR_print_errors_fp(stdout);
		free(buf);
		return 0;
	}

	len += len2;

	// finish up
	len2 = *destlen - len - saltlen;
	if (!EVP_EncryptFinal(xform->state.ssl.dataCtx, tpbin->buf + saltlen + len, &len2)) {
		EVP_CIPHER_CTX_cleanup(xform->state.ssl.dataCtx);
		ERR_print_errors_fp(stdout);
		free(buf);
		return 0;
	}

	*destlen = len + len2 + saltlen;
	free(buf);

	OPENSSL_cleanse(iv, sizeof iv);
#endif /* HAVE_OPENSSL */

	return 1;
}

/*
 *
 */
static int
ssl_decompress(Transformation *xform, struct tapebuf *tpbin,
	unsigned long *destlen, const char *src, int srclen, char **reason)
{
#ifdef HAVE_OPENSSL
	unsigned char salt[16], iv[EVP_MAX_MD_SIZE];
	unsigned int saltlen = sizeof(salt);
	unsigned int ivlen = sizeof(iv);
	unsigned char digest[EVP_MAX_MD_SIZE];
	unsigned int digestlen;
	char *buf;
	unsigned int len, len2;

	digestlen = EVP_MD_size(xform->state.ssl.digest);

	len = *destlen + 1000;
	buf = malloc(len);

	// how to know salt length?
	memcpy(salt, src, saltlen);
	generateIV(xform, salt, &saltlen, iv, &ivlen);

	EVP_DecryptInit_ex(xform->state.ssl.dataCtx, xform->state.ssl.cipher,  xform->state.ssl.engine, NULL, NULL);
	//EVP_CIPHER_CTX_set_key_length(&ctx, 8);
	EVP_DecryptInit_ex(xform->state.ssl.dataCtx, NULL, NULL, xform->state.ssl.key, iv);

	if (!EVP_DecryptUpdate(xform->state.ssl.dataCtx, buf, &len, src+saltlen, srclen-saltlen)) {
		EVP_CIPHER_CTX_cleanup(xform->state.ssl.dataCtx);
		free(buf);
		ERR_print_errors_fp(stdout);
		printf("error 1\n");
		return 0;
	}

	len2 = *destlen + 1000 - len;
	if (!EVP_DecryptFinal(xform->state.ssl.dataCtx, buf + len, &len2)) {
		EVP_CIPHER_CTX_cleanup(xform->state.ssl.dataCtx);
		free(buf);
		ERR_print_errors_fp(stdout);
		printf("error 2\n");
		return 0;
	}
	len += len2;
	len -= digestlen;

	OPENSSL_cleanse(iv, sizeof iv);

	int cresult;
	cresult = uncompress(tpbin->buf, destlen, buf, len);
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
	if (cresult != Z_OK) {
		printf("compression failed: %s\n", *reason);
		free(buf);
		return 0;
	}

	/* verify digest */
	EVP_Digest(tpbin->buf, *destlen, digest, &digestlen, xform->state.ssl.digest,  xform->state.ssl.engine);

	if (memcmp(buf + len, digest, digestlen)) {
		*reason = "digests did not match";
		return 0;
	}

	free(buf);

#endif /* HAVE_OPENSSL */

	return 1;
}

/*
 *
 */
#if 0
static int
ssl_compress_ts_addr(char *state, struct tapebuf *comp_buf,
	unsigned int *worklen, char *data, int writesize, int compressed)
{
#ifdef HAVE_OPENSSL
	SSLState *s = (SSLState *) state;
	unsigned char iv[32];
	unsigned int len;

	EVP_CIPHER_CTX *ctx = (EVP_CIPHER_CTX *) malloc(sizeof(EVP_CIPHER_CTX));

	//EVP_BytesToKey(cipher, EVP_md5(), NULL, buf, strlen(buf), 1, key, iv);

	//EVP_CIPHER_CTX_init(ctx);
	//EVP_CipherInit_ex(ctx, cipher, NULL, key, iv, do_encrypt);
	//blocksize = EVP_CIPHER_CTX_block_size(ctx);

	s->digest = EVP_md_null;
	s->cipher = EVP_enc_null;
	EVP_CIPHER_CTX_init(s->ctx);

	/* calculate 'random' IV. */
	EVP_MD_CTX_init(&md);
	EVP_DigestInit_ex(s->md, s->md, NULL);
	/* EVP_DigestUpdate(s->md, x, len);  <-- use logical record number */
	EVP_DigestUpdate(s->md, s->salt, s->saltlen);
	len = sizeof(iv);
	EVP_DigestFinal(s->md, iv, &len);

	// mlock on state info...

	/* do the actual encryption */
	EVP_CipherInit(s->ctx, s->cipher, s->key, iv);
	EVP_CipherUpdate(s->ctx, out, &n, buf, buflen);
	EVP_CipherFinal(s->ctx, out + n, outlen - y);
	return 1;
#endif /* HAVE_OPENSSL */

	return 1;
}
#endif


/*
 * Factory. The cipher and digestnames should be read from a localized
 * policy file.
 *
 * TODO: indicate error if unknown cipher or digest.
 */
Transformation
*transformation_ssl_factory(int enc, int complvl, const char *ciphername, const char *digestname)
{
	int keylen;
	Transformation *t;

	t = (Transformation *) malloc(sizeof (Transformation));
	mlock(t, sizeof(Transformation));

#ifdef HAVE_OPENSSL
	OpenSSL_add_all_ciphers();
	OpenSSL_add_all_digests();
#endif

	t->enc = enc;
#ifdef HAVE_OPENSSL
	t->state.ssl.complvl = complvl;
	t->state.ssl.cipher = EVP_get_cipherbyname(ciphername);
	t->state.ssl.digest = EVP_get_digestbyname(digestname);
	t->state.ssl.engine = NULL;
#endif

	t->name = "ssl";
	t->mandatory = 1;
	t->initialize = &ssl_initialize;
	t->shutdown = &ssl_shutdown;
	t->startNewTape = &ssl_startNewTape;
	t->startDiskIOProcess = &ssl_startDiskIOProcess;
	t->endDiskIOProcess = &ssl_endDiskIOProcess;
	t->compress = &ssl_compress;
	t->decompress = &ssl_decompress;

#ifdef HAVE_OPENSSL
	// we could use this to generate a key from a passphrase.
	// using this as the actual encryption key has the problems
	// discussed elsewhere.
	//EVP_BytesToKey(cipher, EVP_md5(), NULL, buf, strlen(buf), 1, key, iv);

	if (enc) {
		/* generate random session key */
		//EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
		//EVP_CIPHER_CTX_init(ctx);
		//EVP_CIPHER_CTX_rand_key(ctx, t->state.ssl.key);
		//EVP_CIPHER_CTX_cleanup(ctx);
		//EVP_CIPHER_CTX_free(ctx);
		RAND_bytes(t->state.ssl.key, t->state.ssl.cipher->key_len);
	} else {
		// how do we get keys?
	}
#endif

	return t;
}
