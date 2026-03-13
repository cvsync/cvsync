/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_COMPAT_INTTYPES_H
#define	CVSYNC_COMPAT_INTTYPES_H

#if defined(_AIX) || defined(CVSYNC_DragonFly__) || defined(__FreeBSD) || \
    defined(CVSYNC_NetBSD__) || defined(__OpenBSD__) || defined(__QNX) || \
    defined(CVSYNC_linux__) || defined(sun)
#include <inttypes.h>
#endif /* _AIX || CVSYNC_DragonFly__ || __FreeBSD__ || __NetBSD__ || __OpenBSD ||
	  CVSYNC_QNX__ || __linux__ || sun */

#ifndef PRId64
#define	PRId64	"lld"
#endif /* PRId64 */

#ifndef PRIu64
#define	PRIu64	"llu"
#endif /* PRIu64 */

#endif /* CVSYNC_COMPAT_INTTYPES_H */
