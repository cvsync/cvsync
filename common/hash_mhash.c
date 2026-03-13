/*-
 * This software is released under the BSD License, see LICENSE.
 */

#include <sys/types.h>

#include <mhash.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"

#include "hash.h"

bool cvsync_MD5_init(void **);
bool cvsync_RIPEMD160_init(void **);
bool cvsync_SHA1_init(void **);
bool cvsync_TIGER192_init(void **);
void cvsync_mhash_update(void *, const void *, size_t);
void cvsync_mhash_final(void *, uint8_t *);
void cvsync_mhash_destroy(void *);

const struct hash_args MD5_args = {
	cvsync_MD5_init, cvsync_mhash_update, cvsync_mhash_final,
	cvsync_mhash_destroy,
	16
};

const struct hash_args RIPEMD160_args = {
	cvsync_RIPEMD160_init, cvsync_mhash_update, cvsync_mhash_final,
	cvsync_mhash_destroy,
	20
};

const struct hash_args SHA1_args = {
	cvsync_SHA1_init, cvsync_mhash_update, cvsync_mhash_final,
	cvsync_mhash_destroy,
	20
};

const struct hash_args TIGER192_args = {
	cvsync_TIGER192_init, cvsync_mhash_update, cvsync_mhash_final,
	cvsync_mhash_destroy,
	20
};

bool
cvsync_MD5_init(void **ctx)
{
	MHASH mhashctx;

	if ((mhashctx = mhash_init(MHASH_MD5)) == MHASH_FAILED)
		return (false);

	*ctx = mhashctx;

	return (true);
}

bool
cvsync_RIPEMD160_init(void **ctx)
{
	MHASH mhashctx;

	if ((mhashctx = mhash_init(MHASH_RIPEMD160)) == MHASH_FAILED)
		return (false);

	*ctx = mhashctx;

	return (true);
}

bool
cvsync_SHA1_init(void **ctx)
{
	MHASH mhashctx;

	if ((mhashctx = mhash_init(MHASH_SHA1)) == MHASH_FAILED)
		return (false);

	*ctx = mhashctx;

	return (true);
}

bool
cvsync_TIGER192_init(void **ctx)
{
	MHASH mhashctx;

	if ((mhashctx = mhash_init(MHASH_TIGER)) == MHASH_FAILED)
		return (false);

	*ctx = mhashctx;

	return (true);
}

void
cvsync_mhash_update(void *ctx, const void *buffer, size_t bufsize)
{
	mhash(ctx, buffer, bufsize);
}

void
cvsync_mhash_final(void *ctx, uint8_t *buffer)
{
	mhash_deinit(ctx, buffer);
}

void
cvsync_mhash_destroy(void *ctx)
{
	mhash_end(ctx);
}
