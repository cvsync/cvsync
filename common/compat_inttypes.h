/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_COMPAT_INTTYPES_H
#define	CVSYNC_COMPAT_INTTYPES_H

#if defined(_AIX) || defined(__DragonFly__) || defined(__FreeBSD__) || \
    defined(__NetBSD__) || defined(__OpenBSD__) || defined(__QNX__) || \
    defined(__linux__) || defined(__sun)
#include <inttypes.h>
#endif /* _AIX || __DragonFly__ || __FreeBSD__ || __NetBSD__ || __OpenBSD__ ||
	  __QNX__ || __linux__ || __sun */

#ifndef PRId64
#define	PRId64	"lld"
#endif /* PRId64 */

#ifndef PRIu64
#define	PRIu64	"llu"
#endif /* PRIu64 */

#endif /* CVSYNC_COMPAT_INTTYPES_H */
