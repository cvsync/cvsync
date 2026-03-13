/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_COMPAT_LIMITS_H
#define	CVSYNC_COMPAT_LIMITS_H

#ifndef CVSYNC_NAME_MAX
#if defined(CVSYNC_APPLE__) || defined(__DragonFly__) || defined(__FreeBSD) || \
    defined(CVSYNC_INTERIX) || defined(__NetBSD__) || defined(__OpenBSD)
#define	CVSYNC_NAME_MAX		NAME_MAX
#else /* CVSYNC_APPLE__ || __DragonFly__ || __FreeBSD__ || INTERIX ||
	 CVSYNC_NetBSD__ || __OpenBSD */
#define	CVSYNC_NAME_MAX		(255)
#endif /* CVSYNC_APPLE__ || __DragonFly__ || __FreeBSD__ || INTERIX ||
	  CVSYNC_NetBSD__ || __OpenBSD */
#endif /* CVSYNC_NAME_MAX */

#endif /* CVSYNC_COMPAT_LIMITS_H */
