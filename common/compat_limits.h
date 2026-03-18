/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_COMPAT_LIMITS_H
#define	CVSYNC_COMPAT_LIMITS_H

#ifndef CVSYNC_NAME_MAX
#if defined(__APPLE__) || defined(__DragonFly__) || defined(__FreeBSD__) || defined(__INTERIX) || \
    defined(__NetBSD__) || defined(__OpenBSD__)
#define	CVSYNC_NAME_MAX		NAME_MAX
#else /* __APPLE__ || __DragonFly__ || __FreeBSD__ || __INTERIX || __NetBSD__ || __OpenBSD__ */
#define	CVSYNC_NAME_MAX		(255)
#endif /* __APPLE__ || __DragonFly__ || __FreeBSD__ || __INTERIX || __NetBSD__ || __OpenBSD__ */
#endif /* CVSYNC_NAME_MAX */

#endif /* CVSYNC_COMPAT_LIMITS_H */
