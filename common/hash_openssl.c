/*-
 * This software is released under the BSD License, see LICENSE.
 */

#include <sys/types.h>

#include <stdlib.h>

#include <openssl/md5.h>
#include <openssl/ripemd.h>
#include <openssl/sha.h>

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
	MD5_CTX *md5ctx;

	if ((md5ctx = malloc(sizeof(*md5ctx))) == NULL)
		return (false);
	MD5_Init(md5ctx);

	*ctx = md5ctx;

	return (true);
}

void
cvsync_MD5_update(void *ctx, const void *buffer, size_t bufsize)
{
	MD5_Update(ctx, buffer, (unsigned long)bufsize);
}

void
cvsync_MD5_final(void *ctx, uint8_t *buffer)
{
	MD5_Final(buffer, ctx);
	free(ctx);
}

bool
cvsync_RIPEMD160_init(void **ctx)
{
	RIPEMD160_CTX *ripemd160ctx;

	if ((ripemd160ctx = malloc(sizeof(*ripemd160ctx))) == NULL)
		return (false);
	RIPEMD160_Init(ripemd160ctx);

	*ctx = ripemd160ctx;

	return (true);
}

void
cvsync_RIPEMD160_update(void *ctx, const void *buffer, size_t bufsize)
{
	RIPEMD160_Update(ctx, buffer, (unsigned long)bufsize);
}

void
cvsync_RIPEMD160_final(void *ctx, uint8_t *buffer)
{
	RIPEMD160_Final(buffer, ctx);
	free(ctx);
}

bool
cvsync_SHA1_init(void **ctx)
{
	SHA_CTX *sha1ctx;

	if ((sha1ctx = malloc(sizeof(*sha1ctx))) == NULL)
		return (false);
	SHA1_Init(sha1ctx);

	*ctx = sha1ctx;

	return (true);
}

void
cvsync_SHA1_update(void *ctx, const void *buffer, size_t bufsize)
{
	SHA1_Update(ctx, buffer, (unsigned long)bufsize);
}

void
cvsync_SHA1_final(void *ctx, uint8_t *buffer)
{
	SHA1_Final(buffer, ctx);
	free(ctx);
}

void
cvsync_openssl_destroy(void *ctx)
{
	free(ctx);
}
