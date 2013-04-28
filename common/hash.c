/*-
 * Copyright (c) 2000-2005 MAEKAWA Masahide <maekawa@cvsync.org>
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
#include <strings.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"
#include "compat_strings.h"

#include "hash.h"

struct hashent {
	const char *name;
	size_t namelen;
	int type;
};

static const struct hashent hashents[] = {
	{ "MD5",	3,	HASH_MD5 },
#if defined(HAVE_RIPEMD160)
	{ "RIPEMD160",	9,	HASH_RIPEMD160 },
#endif /* defined(HAVE_RIPEMD160) */
#if defined(HAVE_SHA1)
	{ "SHA1",	4,	HASH_SHA1 },
#endif /* defined(HAVE_SHA1) */
#if defined(HAVE_TIGER192)
	{ "TIGER192",	8,	HASH_TIGER192 },
#endif /* defined(HAVE_TIGER192) */
	{ NULL,		0,	HASH_UNSPEC }
};

extern const struct hash_args MD5_args;
#if defined(HAVE_RIPEMD160)
extern const struct hash_args RIPEMD160_args;
#endif /* defined(HAVE_RIPEMD160) */
#if defined(HAVE_SHA1)
extern const struct hash_args SHA1_args;
#endif /* defined(HAVE_SHA1) */
#if defined(HAVE_TIGER192)
extern const struct hash_args TIGER192_args;
#endif /* defined(HAVE_TIGER192) */

int
hash_pton(const char *name, size_t namelen)
{
	const struct hashent *h;

	for (h = hashents ; h->name != NULL ; h++) {
		if ((h->namelen == namelen) &&
		    (strncasecmp(h->name, name, namelen) == 0)) {
			return (h->type);
		}
	}

	return (HASH_UNSPEC);
}

size_t
hash_ntop(int type, void *buffer, size_t bufsize)
{
	const struct hashent *h;

	for (h = hashents ; h->name != NULL ; h++) {
		if (h->type == type) {
			if (h->namelen >= bufsize)
				break;
			(void)memcpy(buffer, h->name, h->namelen);
			return (h->namelen);
		}
	}

	return (0);
}

bool
hash_set(int type, const struct hash_args **args)
{
	switch (type) {
	case HASH_MD5:
		*args = &MD5_args;
		break;
#if defined(HAVE_RIPEMD160)
	case HASH_RIPEMD160:
		*args = &RIPEMD160_args;
		break;
#endif /* defined(HAVE_RIPEMD160) */
#if defined(HAVE_SHA1)
	case HASH_SHA1:
		*args = &SHA1_args;
		break;
#endif /* defined(HAVE_SHA1) */
#if defined(HAVE_TIGER192)
	case HASH_TIGER192:
		*args = &TIGER192_args;
		break;
#endif /* defined(HAVE_TIGER192) */
	default:
		return (false);
	}

	return (true);
}
