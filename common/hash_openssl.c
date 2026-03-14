/*-
 * This software is released under the BSD License, see LICENSE.
 */

#include <sys/types.h>

#include <stdlib.h>

#include <openssl/evp.h>

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
	HASH_SIZE_MD5
};

const struct hash_args RIPEMD160_args = {
	cvsync_RIPEMD160_init, cvsync_RIPEMD160_update, cvsync_RIPEMD160_final,
	cvsync_openssl_destroy,
	HASH_SIZE_RIPEMD160
};

const struct hash_args SHA1_args = {
	cvsync_SHA1_init, cvsync_SHA1_update, cvsync_SHA1_final,
	cvsync_openssl_destroy,
	HASH_SIZE_SHA1
};

bool
cvsync_MD5_init(void **ctx)
{
	EVP_MD_CTX *mdctx;

	if ((mdctx = EVP_MD_CTX_new()) == NULL)
		return (false);
	EVP_DigestInit_ex(mdctx, EVP_md5(), NULL);

	*ctx = mdctx;

	return (true);
}

void
cvsync_MD5_update(void *ctx, const void *buffer, size_t bufsize)
{
	EVP_DigestUpdate(ctx, buffer, bufsize);
}

void
cvsync_MD5_final(void *ctx, uint8_t *buffer)
{
	unsigned int s = HASH_SIZE_MD5;

	EVP_DigestFinal_ex(ctx, buffer, &s);
	EVP_MD_CTX_free(ctx);
}

bool
cvsync_RIPEMD160_init(void **ctx)
{
	EVP_MD_CTX *mdctx;

	if ((mdctx = EVP_MD_CTX_new()) == NULL)
		return (false);
	EVP_DigestInit_ex(mdctx, EVP_ripemd160(), NULL);

	*ctx = mdctx;

	return (true);
}

void
cvsync_RIPEMD160_update(void *ctx, const void *buffer, size_t bufsize)
{
	EVP_DigestUpdate(ctx, buffer, bufsize);
}

void
cvsync_RIPEMD160_final(void *ctx, uint8_t *buffer)
{
	unsigned int s = HASH_SIZE_RIPEMD160;

	EVP_DigestFinal_ex(ctx, buffer, &s);
	EVP_MD_CTX_free(ctx);
}

bool
cvsync_SHA1_init(void **ctx)
{
	EVP_MD_CTX *mdctx;

	if ((mdctx = EVP_MD_CTX_new()) == NULL)
		return (false);
	EVP_DigestInit_ex(mdctx, EVP_sha1(), NULL);

	*ctx = mdctx;

	return (true);
}

void
cvsync_SHA1_update(void *ctx, const void *buffer, size_t bufsize)
{
	EVP_DigestUpdate(ctx, buffer, bufsize);
}

void
cvsync_SHA1_final(void *ctx, uint8_t *buffer)
{
	unsigned int s = HASH_SIZE_SHA1;

	EVP_DigestFinal_ex(ctx, buffer, &s);
	EVP_MD_CTX_free(ctx);
}

void
cvsync_openssl_destroy(void *ctx)
{
	EVP_MD_CTX_free(ctx);
}
