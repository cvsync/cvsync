/*-
 * Copyright (c) 2000-2013 MAEKAWA Masahide <maekawa@cvsync.org>
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

#ifndef __HASH_H__
#define	__HASH_H__

enum {
	HASH_UNSPEC	= 0,

	HASH_MD5,
#if defined(HAVE_RIPEMD160)
	HASH_RIPEMD160,
#endif /* defined(HAVE_RIPEMD160) */
#if defined(HAVE_SHA1)
	HASH_SHA1,
#endif /* defined(HAVE_SHA1) */
#if defined(HAVE_TIGER192)
	HASH_TIGER192,
#endif /* defined(HAVE_TIGER192) */

	HASH_MAX
};

#define	HASH_DEFAULT_TYPE	HASH_MD5

#define	HASH_MAXLEN	(64)	/* 512bits */

struct hash_args {
	bool	(*init)(void **);
	void	(*update)(void *, const void *, size_t);
	void	(*final)(void *, uint8_t *);
	void	(*destroy)(void *);
	size_t	length;
};

int hash_pton(const char *, size_t);
size_t hash_ntop(int, void *, size_t);
bool hash_set(int, const struct hash_args **);

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

#endif /* __HASH_H__ */
