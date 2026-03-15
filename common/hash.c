/*-
 * This software is released under the BSD License, see LICENSE.
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
#if defined(HAVE_MD5)
	{ "MD5",	3,	HASH_MD5 },
#endif /* defined(HAVE_MD5) */
#if defined(HAVE_SHA1)
	{ "SHA1",	4,	HASH_SHA1 },
#endif /* defined(HAVE_SHA1) */
#if defined(HAVE_SHA256)
	{ "SHA256",	6,	HASH_SHA256 },
#endif /* defined(HAVE_SHA256) */
	{ NULL,		0,	HASH_UNSPEC }
};

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
#if defined(HAVE_MD5)
	case HASH_MD5:
		*args = &MD5_args;
		break;
#endif /* defined(HAVE_MD5) */
#if defined(HAVE_SHA1)
	case HASH_SHA1:
		*args = &SHA1_args;
		break;
#endif /* defined(HAVE_SHA1) */
#if defined(HAVE_SHA256)
	case HASH_SHA256:
		*args = &SHA256_args;
		break;
#endif /* defined(HAVE_SHA256) */
	default:
		return (false);
	}

	return (true);
}
