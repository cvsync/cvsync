/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_HASH_NATIVE_H
#define	CVSYNC_HASH_NATIVE_H

/*
 * MD5 Message-Digest Algorithm - RFC 1321
 */

#if defined(__DragonFly__) || defined(__FreeBSD__) || defined(__INTERIX) || \
    defined(__NetBSD__) || defined(__OpenBSD__) || defined(__sun)
#include <md5.h>
#endif /* __DragonFly__ || __FreeBSD__ || __INTERIX || __NetBSD__ ||
	  __OpenBSD__ || __sun */

/*
 * RIPEMD160
 */

#if defined(HAVE_RIPEMD160)

#if defined(__FreeBSD__)
#include <ripemd.h>

#define	RMD160Init	RIPEMD160_Init
#define	RMD160Update	RIPEMD160_Update
#define	RMD160Final	RIPEMD160_Final
#define	RMD160_CTX	RIPEMD160_CTX
#endif /* __FreeBSD__ */

#if defined(__DragonFly__) || defined(__INTERIX) || defined(__NetBSD__) || \
    defined(__OpenBSD__)
#include <rmd160.h>
#endif /* __DragonFly__ || __INTERIX || __NetBSD__ || __OpenBSD__ */

#endif /* defined(HAVE_RIPEMD160) */

/*
 * Secure Hash Algorithm 1 - FIPS 180-1, RFC 3174
 */

#if defined(HAVE_SHA1)

#if defined(__DragonFly__) || defined(__FreeBSD__)
#include <sha.h>

#define	SHA1Init	SHA1_Init
#define	SHA1Update	SHA1_Update
#define	SHA1Final	SHA1_Final
#endif /* __DragonFly__ || __FreeBSD__ */

#if defined(__INTERIX) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sha1.h>
#endif /* __INTERIX || __NetBSD__ || __OpenBSD__ */

#endif /* defined(HAVE_SHA1) */

/*
 * Secure Hash Standard (SHS) - FIPS 180-2, RFC 4634
 */

#if defined(HAVE_SHA256)

#if defined(__FreeBSD__)
#include <sha256.h>
#endif /* __FreeBSD__ */

#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sha2.h>
#endif /* __NetBSD__ || __OpenBSD__ */

#if defined(__OpenBSD__)
typedef	SHA2_CTX	SHA256_CTX;
#endif /* __OpenBSD__ */

#if defined(__FreeBSD__) || defined(__NetBSD__)
#define	SHA256Init	SHA256_Init
#define	SHA256Update	SHA256_Update
#define	SHA256Final	SHA256_Final
#endif /* __FreeBSD__ || __NetBSD__ */

#endif /* defined(HAVE_SHA256) */

#if defined(__APPLE__)

#include <CommonCrypto/CommonDigest.h>

typedef CC_MD5_CTX	MD5_CTX;
#define	MD5Init		CC_MD5_Init
#define	MD5Update	CC_MD5_Update
#define	MD5Final	CC_MD5_Final

typedef CC_SHA1_CTX	SHA1_CTX;
#define	SHA1Init	CC_SHA1_Init
#define	SHA1Update	CC_SHA1_Update
#define	SHA1Final	CC_SHA1_Final

#endif /* defined(__APPLE__) */

#endif /* CVSYNC_HASH_NATIVE_H */
