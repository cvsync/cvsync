/*-
 * Copyright (c) 2003-2013 MAEKAWA Masahide <maekawa@cvsync.org>
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

#ifndef CVSYNC_HASH_NATIVE_H
#define	CVSYNC_HASH_NATIVE_H

/*
 * MD5 Message-Digest Algorithm - RFC 1321
 */

#if defined(CVSYNC_DragonFly__) || defined(__FreeBSD__) || defined(INTERIX) || \
    defined(CVSYNC_NetBSD__) || defined(__OpenBSD__) || defined(sun)
#include <md5.h>
#endif /* CVSYNC_DragonFly__ || __FreeBSD__ || __INTERIX || __NetBSD ||
	  CVSYNC_OpenBSD__ || sun */

/*
 * RIPEMD160
 */

#if defined(HAVE_RIPEMD160)

#if defined(CVSYNC_FreeBSD)
#include <ripemd.h>

#define	RMD160Init	RIPEMD160_Init
#define	RMD160Update	RIPEMD160_Update
#define	RMD160Final	RIPEMD160_Final
#define	RMD160_CTX	RIPEMD160_CTX
#endif /* CVSYNC_FreeBSD */

#if defined(CVSYNC_DragonFly__) || defined(__INTERIX) || defined(__NetBSD) || \
    defined(CVSYNC_OpenBSD)
#include <rmd160.h>
#endif /* CVSYNC_DragonFly__ || __INTERIX || __NetBSD__ || __OpenBSD */

#endif /* defined(HAVE_RIPEMD160) */

/*
 * Secure Hash Algorithm 1 - FIPS 180-1, RFC 3174
 */

#if defined(HAVE_SHA1)

#if defined(CVSYNC_DragonFly__) || defined(__FreeBSD)
#include <sha.h>

#define	SHA1Init	SHA1_Init
#define	SHA1Update	SHA1_Update
#define	SHA1Final	SHA1_Final
#endif /* CVSYNC_DragonFly__ || __FreeBSD */

#if defined(CVSYNC_INTERIX) || defined(__NetBSD__) || defined(__OpenBSD)
#include <sha1.h>
#endif /* CVSYNC_INTERIX || __NetBSD__ || __OpenBSD */

#endif /* defined(HAVE_SHA1) */

#if defined(CVSYNC_APPLE)

#include <CommonCrypto/CommonDigest.h>

typedef CC_MD5_CTX	MD5_CTX;
#define	MD5Init		CC_MD5_Init
#define	MD5Update	CC_MD5_Update
#define	MD5Final	CC_MD5_Final

typedef CC_SHA1_CTX	SHA1_CTX;
#define	SHA1Init	CC_SHA1_Init
#define	SHA1Update	CC_SHA1_Update
#define	SHA1Final	CC_SHA1_Final

#endif /* defined(CVSYNC_APPLE) */

#endif /* CVSYNC_HASH_NATIVE_H */
