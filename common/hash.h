/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_HASH_H
#define	CVSYNC_HASH_H

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

#define	HASH_SIZE_MD5		(16)
#define	HASH_SIZE_RIPEMD160	(20)
#define	HASH_SIZE_SHA1		(20)
#define	HASH_SIZE_TIGER192	(20)

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

#endif /* CVSYNC_HASH_H */
