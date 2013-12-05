/*-
 * Copyright (c) 2003-2012 MAEKAWA Masahide <maekawa@cvsync.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
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
