/*-
 * This software is released under the BSD License, see LICENSE.
 */

#include <sys/types.h>

#include <stdlib.h>

#include <openssl/evp.h>

#if defined(OSSL_DEPRECATEDIN_3_0)
#include <openssl/md5.h>
#include <openssl/ripemd.h>
#include <openssl/sha.h>
#endif /* defined(OSSL_DEPRECATEDIN_3_0) */

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"

#include "hash.h"

bool cvsync_MD5_init(void **);
void cvsync_MD5_update(void *, const void *, size_t);
void cvsync_MD5_final(void *, uint8_t *);
bool cvsync_RIPEMD160_init(void **);
void cvsync_RIPEMD160_update(void *, const void *, size_t);
void cvsync_RIPEMD160_final(void *, uint8_t *);
bool cvsync_SHA1_init(void **);
void cvsync_SHA1_update(void *, const void *, size_t);
void cvsync_SHA1_final(void *, uint8_t *);
void cvsync_openssl_destroy(void *);

const struct hash_args MD5_args = {
	cvsync_MD5_init, cvsync_MD5_update, cvsync_MD5_final,
	cvsync_openssl_destroy,
	16
};

const struct hash_args RIPEMD160_args = {
	cvsync_RIPEMD160_init, cvsync_RIPEMD160_update, cvsync_RIPEMD160_final,
	cvsync_openssl_destroy,
	20
};

const struct hash_args SHA1_args = {
	cvsync_SHA1_init, cvsync_SHA1_update, cvsync_SHA1_final,
	cvsync_openssl_destroy,
	20
};

bool
cvsync_MD5_init(void **ctx)
{
#if defined(OSSL_DEPRECATEDIN_3_0)
	EVP_MD_CTX *mdctx;

	if ((mdctx = EVP_MD_CTX_new()) == NULL)
		return (false);
	EVP_DigestInit_ex(mdctx, EVP_md5(), NULL);
#else /* defined(OSSL_DEPRECATEDIN_3_0) */
	MD5_CTX *mdctx;

	if ((mdctx = malloc(sizeof(*mdctx))) == NULL)
		return (false);
	MD5_Init(mdctx);
#endif /* defined(OSSL_DEPRECATEDIN_3_0) */

	*ctx = mdctx;

	return (true);
}

void
cvsync_MD5_update(void *ctx, const void *buffer, size_t bufsize)
{
#if defined(OSSL_DEPRECATEDIN_3_0)
	EVP_DigestUpdate(ctx, buffer, bufsize);
#else /* defined(OSSL_DEPRECATEDIN_3_0) */
	MD5_Update(ctx, buffer, (unsigned long)bufsize);
#endif /* defined(OSSL_DEPRECATEDIN_3_0) */
}

void
cvsync_MD5_final(void *ctx, uint8_t *buffer)
{
#if defined(OSSL_DEPRECATEDIN_3_0)
	unsigned int s = HASH_MAXLEN;
	EVP_DigestFinal_ex(ctx, buffer, &s);
	EVP_MD_CTX_free(ctx);
#else /* defined(OSSL_DEPRECATEDIN_3_0) */
	MD5_Final(buffer, ctx);
	free(ctx);
#endif /* defined(OSSL_DEPRECATEDIN_3_0) */
}

bool
cvsync_RIPEMD160_init(void **ctx)
{
#if defined(OSSL_DEPRECATEDIN_3_0)
	EVP_MD_CTX *mdctx;

	if ((mdctx = EVP_MD_CTX_new()) == NULL)
		return (false);
	EVP_DigestInit_ex(mdctx, EVP_ripemd160(), NULL);
#else /* defined(OSSL_DEPRECATEDIN_3_0) */
	RIPEMD160_CTX *mdctx;

	if ((mdctx = malloc(sizeof(*mdctx))) == NULL)
		return (false);
	RIPEMD160_Init(mdctx);
#endif /* defined(OSSL_DEPRECATEDIN_3_0) */

	*ctx = mdctx;

	return (true);
}

void
cvsync_RIPEMD160_update(void *ctx, const void *buffer, size_t bufsize)
{
#if defined(OSSL_DEPRECATEDIN_3_0)
	EVP_DigestUpdate(ctx, buffer, bufsize);
#else /* defined(OSSL_DEPRECATEDIN_3_0) */
	RIPEMD160_Update(ctx, buffer, (unsigned long)bufsize);
#endif /* defined(OSSL_DEPRECATEDIN_3_0) */
}

void
cvsync_RIPEMD160_final(void *ctx, uint8_t *buffer)
{
#if defined(OSSL_DEPRECATEDIN_3_0)
	unsigned int s = HASH_MAXLEN;
	EVP_DigestFinal_ex(ctx, buffer, &s);
	EVP_MD_CTX_free(ctx);
#else /* defined(OSSL_DEPRECATEDIN_3_0) */
	RIPEMD160_Final(buffer, ctx);
	free(ctx);
#endif /* defined(OSSL_DEPRECATEDIN_3_0) */
}

bool
cvsync_SHA1_init(void **ctx)
{
#if defined(OSSL_DEPRECATEDIN_3_0)
	EVP_MD_CTX *mdctx;

	if ((mdctx = EVP_MD_CTX_new()) == NULL)
		return (false);
	EVP_DigestInit_ex(mdctx, EVP_sha1(), NULL);
#else /* defined(OSSL_DEPRECATEDIN_3_0) */
	SHA_CTX *mdctx;

	if ((mdctx = malloc(sizeof(*mdctx))) == NULL)
		return (false);
	SHA1_Init(mdctx);
#endif /* defined(OSSL_DEPRECATEDIN_3_0) */

	*ctx = mdctx;

	return (true);
}

void
cvsync_SHA1_update(void *ctx, const void *buffer, size_t bufsize)
{
#if defined(OSSL_DEPRECATEDIN_3_0)
	EVP_DigestUpdate(ctx, buffer, bufsize);
#else /* defined(OSSL_DEPRECATEDIN_3_0) */
	SHA1_Update(ctx, buffer, (unsigned long)bufsize);
#endif /* defined(OSSL_DEPRECATEDIN_3_0) */
}

void
cvsync_SHA1_final(void *ctx, uint8_t *buffer)
{
#if defined(OSSL_DEPRECATEDIN_3_0)
	unsigned int s = HASH_MAXLEN;
	EVP_DigestFinal_ex(ctx, buffer, &s);
	EVP_MD_CTX_free(ctx);
#else /* defined(OSSL_DEPRECATEDIN_3_0) */
	SHA1_Final(buffer, ctx);
	free(ctx);
#endif /* defined(OSSL_DEPRECATEDIN_3_0) */
}

void
cvsync_openssl_destroy(void *ctx)
{
	free(ctx);
}
