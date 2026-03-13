/*-
 * This software is released under the BSD License, see LICENSE.
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
	HASH_SIZE_MD5
};

const struct hash_args RIPEMD160_args = {
	cvsync_RIPEMD160_init, cvsync_libgcrypt_update, cvsync_RIPEMD160_final,
	cvsync_libgcrypt_destroy,
	HASH_SIZE_RIPEMD160
};

const struct hash_args SHA1_args = {
	cvsync_SHA1_init, cvsync_libgcrypt_update, cvsync_SHA1_final,
	cvsync_libgcrypt_destroy,
	HASH_SIZE_SHA1
};

const struct hash_args TIGER192_args = {
	cvsync_TIGER192_init, cvsync_libgcrypt_update, cvsync_TIGER192_final,
	cvsync_libgcrypt_destroy,
	HASH_SIZE_TIGER192
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
	(void)memcpy(buffer, res, HASH_SIZE_MD5);
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
	(void)memcpy(buffer, res, HASH_SIZE_RIPEMD160);
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
	(void)memcpy(buffer, res, HASH_SIZE_SHA1);
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
	(void)memcpy(buffer, res, HASH_SIZE_TIGER192);
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
