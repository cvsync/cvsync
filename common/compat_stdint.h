/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_COMPAT_STDINT_H
#define	CVSYNC_COMPAT_STDINT_H

#if defined(__APPLE__) || defined(__NetBSD__) || defined(__QNX__) || \
    defined(__sun)
#if !defined(NO_STDINT_H)
#include <stdint.h>
#endif /* defined(NO_STDINT_H) */
#endif /* __APPLE__ || __NetBSD__ || __QNX__ || __sun */

#ifndef INT64_MAX
#define	INT64_MAX	(9223372036854775807LL)
#endif /* INT64_MAX */

#ifndef UINT8_MAX
#define	UINT8_MAX	(255U)
#endif /* UINT8_MAX */

#ifndef UINT16_MAX
#define	UINT16_MAX	(65535U)
#endif /* UINT16_MAX */

#ifndef UINT64_MAX
#define	UINT64_MAX	(18446744073709551615ULL)
#endif /* UINT64_MAX */

#ifndef SIZE_MAX
#define	SIZE_MAX	((size_t)-1)
#endif /* SIZE_MAX */

#endif /* CVSYNC_COMPAT_STDINT_H */
