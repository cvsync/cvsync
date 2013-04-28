/*-
 * Copyright (c) 2003-2005 MAEKAWA Masahide <maekawa@cvsync.org>
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

#include <string.h>

#include <gcrypt.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"

#include "hash.h"

bool cvsync_MD5_init(void **);
void cvsync_MD5_final(void *, uint8_t *);
bool cvsync_RIPEMD160_init(void **);
void cvsync_RIPEMD160_final(void *, uint8_t *);
bool cvsync_SHA1_init(void **);
void cvsync_SHA1_final(void *, uint8_t *);
bool cvsync_TIGER192_init(void **);
void cvsync_TIGER192_final(void *, uint8_t *);
void cvsync_libgcrypt_update(void *, const void *, size_t);
void cvsync_libgcrypt_destroy(void *);

const struct hash_args MD5_args = {
	cvsync_MD5_init, cvsync_libgcrypt_update, cvsync_MD5_final,
	cvsync_libgcrypt_destroy,
	16
};

const struct hash_args RIPEMD160_args = {
	cvsync_RIPEMD160_init, cvsync_libgcrypt_update, cvsync_RIPEMD160_final,
	cvsync_libgcrypt_destroy,
	20
};

const struct hash_args SHA1_args = {
	cvsync_SHA1_init, cvsync_libgcrypt_update, cvsync_SHA1_final,
	cvsync_libgcrypt_destroy,
	20
};

const struct hash_args TIGER192_args = {
	cvsync_TIGER192_init, cvsync_libgcrypt_update, cvsync_TIGER192_final,
	cvsync_libgcrypt_destroy,
	20
};

bool
cvsync_MD5_init(void **ctx)
{
	struct gcry_md_handle *libgcryptctx;
#if defined(_GCRY_GCC_ATTR_DEPRECATED)
	gcry_error_t err;
#endif /* defined(_GCRY_GCC_ATTR_DEPRECATED) */

#if defined(_GCRY_GCC_ATTR_DEPRECATED)
	if ((err = gcry_md_open(&libgcryptctx, GCRY_MD_MD5,
				0)) != GPG_ERR_NO_ERROR) {
		return (false);
	}
#else /* defined(_GCRY_GCC_ATTR_DEPRECATED) */
	if ((libgcryptctx = gcry_md_open(GCRY_MD_MD5, 0)) == NULL)
		return (false);
#endif /* defined(_GCRY_GCC_ATTR_DEPRECATED) */

	*ctx = libgcryptctx;

	return (true);
}

void
cvsync_MD5_final(void *ctx, uint8_t *buffer)
{
	void *res;

	res = (void *)gcry_md_read(ctx, GCRY_MD_MD5);
	(void)memcpy(buffer, res, 16);
	gcry_md_close(ctx);
}

bool
cvsync_RIPEMD160_init(void **ctx)
{
	struct gcry_md_handle *libgcryptctx;
#if defined(_GCRY_GCC_ATTR_DEPRECATED)
	gcry_error_t err;
#endif /* defined(_GCRY_GCC_ATTR_DEPRECATED) */

#if defined(_GCRY_GCC_ATTR_DEPRECATED)
	if ((err = gcry_md_open(&libgcryptctx, GCRY_MD_RMD160,
				0)) != GPG_ERR_NO_ERROR) {
		return (false);
	}
#else /* defined(_GCRY_GCC_ATTR_DEPRECATED) */
	if ((libgcryptctx = gcry_md_open(GCRY_MD_RMD160, 0)) == NULL)
		return (false);
#endif /* defined(_GCRY_GCC_ATTR_DEPRECATED) */

	*ctx = libgcryptctx;

	return (true);
}

void
cvsync_RIPEMD160_final(void *ctx, uint8_t *buffer)
{
	void *res;

	res = (void *)gcry_md_read(ctx, GCRY_MD_RMD160);
	(void)memcpy(buffer, res, 20);
	gcry_md_close(ctx);
}

bool
cvsync_SHA1_init(void **ctx)
{
	struct gcry_md_handle *libgcryptctx;
#if defined(_GCRY_GCC_ATTR_DEPRECATED)
	gcry_error_t err;
#endif /* defined(_GCRY_GCC_ATTR_DEPRECATED) */

#if defined(_GCRY_GCC_ATTR_DEPRECATED)
	if ((err = gcry_md_open(&libgcryptctx, GCRY_MD_SHA1,
				0)) != GPG_ERR_NO_ERROR) {
		return (false);
	}
#else /* defined(_GCRY_GCC_ATTR_DEPRECATED) */
	if ((libgcryptctx = gcry_md_open(GCRY_MD_SHA1, 0)) == NULL)
		return (false);
#endif /* defined(_GCRY_GCC_ATTR_DEPRECATED) */

	*ctx = libgcryptctx;

	return (true);
}

void
cvsync_SHA1_final(void *ctx, uint8_t *buffer)
{
	void *res;

	res = (void *)gcry_md_read(ctx, GCRY_MD_SHA1);
	(void)memcpy(buffer, res, 20);
	gcry_md_close(ctx);
}

bool
cvsync_TIGER192_init(void **ctx)
{
	struct gcry_md_handle *libgcryptctx;
#if defined(_GCRY_GCC_ATTR_DEPRECATED)
	gcry_error_t err;
#endif /* defined(_GCRY_GCC_ATTR_DEPRECATED) */

#if defined(_GCRY_GCC_ATTR_DEPRECATED)
	if ((err = gcry_md_open(&libgcryptctx, GCRY_MD_TIGER,
				0)) != GPG_ERR_NO_ERROR) {
		return (false);
	}
#else /* defined(_GCRY_GCC_ATTR_DEPRECATED) */
	if ((libgcryptctx = gcry_md_open(GCRY_MD_TIGER, 0)) == NULL)
		return (false);
#endif /* defined(_GCRY_GCC_ATTR_DEPRECATED) */

	*ctx = libgcryptctx;

	return (true);
}

void
cvsync_TIGER192_final(void *ctx, uint8_t *buffer)
{
	void *res;

	res = (void *)gcry_md_read(ctx, GCRY_MD_TIGER);
	(void)memcpy(buffer, res, 20);
	gcry_md_close(ctx);
}

void
cvsync_libgcrypt_update(void *ctx, const void *buffer, size_t bufsize)
{
	gcry_md_write(ctx, buffer, bufsize);
}

void
cvsync_libgcrypt_destroy(void *ctx)
{
	gcry_md_close(ctx);
}
