/*-
 * This software is released under the BSD License, see LICENSE.
 */

#include <sys/types.h>

#include <stdlib.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"

#include "hash.h"
#include "hash_native.h"

bool cvsync_MD5_init(void **);
void cvsync_MD5_update(void *, const void *, size_t);
void cvsync_MD5_final(void *, uint8_t *);
#if defined(HAVE_RIPEMD160)
bool cvsync_RIPEMD160_init(void **);
void cvsync_RIPEMD160_update(void *, const void *, size_t);
void cvsync_RIPEMD160_final(void *, uint8_t *);
#endif /* defined(HAVE_RIPEMD160) */
#if defined(HAVE_SHA1)
bool cvsync_SHA1_init(void **);
void cvsync_SHA1_update(void *, const void *, size_t);
void cvsync_SHA1_final(void *, uint8_t *);
#endif /* defined(HAVE_SHA1) */
void cvsync_native_destroy(void *);

const struct hash_args MD5_args = {
	cvsync_MD5_init, cvsync_MD5_update, cvsync_MD5_final,
	cvsync_native_destroy,
	HASH_SIZE_MD5
};

#if defined(HAVE_RIPEMD160)
const struct hash_args RIPEMD160_args = {
	cvsync_RIPEMD160_init, cvsync_RIPEMD160_update, cvsync_RIPEMD160_final,
	cvsync_native_destroy,
	HASH_SIZE_RIPEMD160
};
#endif /* defined(HAVE_RIPEMD160) */

#if defined(HAVE_SHA1)
const struct hash_args SHA1_args = {
	cvsync_SHA1_init, cvsync_SHA1_update, cvsync_SHA1_final,
	cvsync_native_destroy,
	HASH_SIZE_SHA1
};
#endif /* defined(HAVE_SHA1) */

bool
cvsync_MD5_init(void **ctx)
{
	MD5_CTX *md5ctx;

	if ((md5ctx = malloc(sizeof(*md5ctx))) == NULL)
		return (false);
	MD5Init(md5ctx);

	*ctx = md5ctx;

	return (true);
}

void
cvsync_MD5_update(void *ctx, const void *buffer, size_t bufsize)
{
	MD5Update(ctx, buffer, bufsize);
}

void
cvsync_MD5_final(void *ctx, uint8_t *buffer)
{
	MD5Final(buffer, ctx);
	free(ctx);
}

#if defined(HAVE_RIPEMD160)
bool
cvsync_RIPEMD160_init(void **ctx)
{
	RMD160_CTX *ripemd160ctx;

	if ((ripemd160ctx = malloc(sizeof(*ripemd160ctx))) == NULL)
		return (false);
	RMD160Init(ripemd160ctx);

	*ctx = ripemd160ctx;

	return (true);
}

void
cvsync_RIPEMD160_update(void *ctx, const void *buffer, size_t bufsize)
{
	RMD160Update(ctx, buffer, bufsize);
}

void
cvsync_RIPEMD160_final(void *ctx, uint8_t *buffer)
{
	RMD160Final(buffer, ctx);
	free(ctx);
}
#endif /* defined(HAVE_SHA1) */

#if defined(HAVE_SHA1)
bool
cvsync_SHA1_init(void **ctx)
{
	SHA1_CTX *sha1ctx;

	if ((sha1ctx = malloc(sizeof(*sha1ctx))) == NULL)
		return (false);
	SHA1Init(sha1ctx);

	*ctx = sha1ctx;

	return (true);
}

void
cvsync_SHA1_update(void *ctx, const void *buffer, size_t bufsize)
{
	SHA1Update(ctx, buffer, bufsize);
}

void
cvsync_SHA1_final(void *ctx, uint8_t *buffer)
{
	SHA1Final(buffer, ctx);
	free(ctx);
}
#endif /* defined(HAVE_SHA1) */

void
cvsync_native_destroy(void *ctx)
{
	free(ctx);
}
