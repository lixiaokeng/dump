#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <strings.h>
#include <errno.h>
#include <openssl/evp.h>

/*
 * Encrypt or decrypt buf, returning a pointer to the transformed data,
 * or NULL on error.
 * The returned data is the same size as the input data,
 * which means we must turn off cipher padding, and require that buflen
 * be a multiple of the cipher block size (8 for Blowfish).
 * To keep things simple, the return value is a malloc'd
 * buffer that is overwritten on each call.
 *
 * Ken Lalonde <ken@globalremit.com>, 2003
 */
char *
cipher(char *buf, int buflen, int do_encrypt)
{
	static EVP_CIPHER_CTX ctx;
	static char *out = NULL;	/* return value, grown as necessary */
	static int outlen = 0;
	static int init = 0, which, blocksize;
	int n;

	if (!init) {
		static const EVP_CIPHER *cipher;
		unsigned char key[EVP_MAX_KEY_LENGTH], iv[EVP_MAX_IV_LENGTH];
		// Read key from $HOME/.ermt.key
		char *keyfile = ".ermt.key";
		unsigned char buf[128];
		FILE *fp;
		int i;

		openlog("ermt", LOG_PID, LOG_DAEMON);
		// Not needed: OpenSSL_add_all_algorithms();
		// Not needed: ERR_load_crypto_strings();
		cipher = EVP_bf_cbc();	// or: EVP_aes_{128,192,256}_cbc()
		// We want the ability to decrypt the output
		// using "openssl enc -d -kfile K -nopad -nosalt", which
		// means we must read the key file the same way
		// openssl does.  But be careful: if the key contains
		// \0 or \r or \n, the effective size will be reduced.
		if ((fp = fopen(keyfile, "r")) == NULL) {
			syslog(LOG_ERR, "Can't open key file %s: %m", keyfile);
			return NULL;
		}
		buf[0] = '\0';
		fgets(buf, sizeof buf, fp);
		fclose(fp);
		i = strlen(buf);
		if ((i > 0) &&
			((buf[i-1] == '\n') || (buf[i-1] == '\r')))
			buf[--i]='\0';
		if ((i > 0) &&
			((buf[i-1] == '\n') || (buf[i-1] == '\r')))
			buf[--i]='\0';
		if (i < 1) {
			syslog(LOG_ERR, "zero length key");
			errno = EINVAL;
			return NULL;
		}
		EVP_BytesToKey(cipher, EVP_md5(), NULL,
			buf, strlen(buf), 1, key, iv);
		EVP_CIPHER_CTX_init(&ctx);
		EVP_CipherInit_ex(&ctx, cipher, NULL, key, iv, do_encrypt);
		EVP_CIPHER_CTX_set_padding(&ctx, 0);	// -nopad
		OPENSSL_cleanse(buf, sizeof buf);
		OPENSSL_cleanse(key, sizeof key);
		OPENSSL_cleanse(iv, sizeof iv);
		blocksize = EVP_CIPHER_CTX_block_size(&ctx);
		which = do_encrypt;
		init = 1;
	}
	if (which != do_encrypt) {
		syslog(LOG_ERR, "Cannot switch modes");
		errno = EINVAL;
		return NULL;
	}
	if ((buflen % blocksize) != 0) {
		syslog(LOG_ERR, "Buffer size is not a multiple of cipher block size");
		errno = EINVAL;
		return NULL;
	}
	if (outlen < buflen+blocksize) {
		outlen = (buflen+blocksize) * 2;
		out = realloc(out, outlen);
	}
	if (!EVP_CipherUpdate(&ctx, out, &n, buf, buflen)) {
		syslog(LOG_ERR, "EVP_CipherUpdate failed");
		errno = EINVAL;
		return NULL;
	}
	if (n != buflen) {
		syslog(LOG_ERR, "EVP_CipherUpdate: %d != %d", n, buflen);
		errno = EINVAL;
		return NULL;
	}
	// assert(ctx->buf_len == 0);
	return out;
}

/* Decrypt stdin to stdout, and exit */
int
decrypt()
{
	char buf[8*1024], *cp;
	int n;

	while ((n = fread(buf, 1, sizeof buf, stdin)) > 0) {
		if ((cp = cipher(buf, n, 0)) == NULL) {
			fprintf(stderr, "ermt: Error decoding input; see daemon.err syslog\n");
			exit(1);
		}
		fwrite(cp, 1, n, stdout);
	}
	if (ferror(stdin)) {
		perror("ermt: stdin: read");
		exit(1);
	}
	if (fflush(stdout)) {
		perror("ermt: stdout: write");
		exit(1);
	}
	exit(0);
}
