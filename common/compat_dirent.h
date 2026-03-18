/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_COMPAT_DIRENT_H
#define	CVSYNC_COMPAT_DIRENT_H

#ifndef DIRENT_NAMLEN
#if defined(_AIX) || defined(__APPLE__) || defined(__DragonFly__) || defined(__FreeBSD__) || \
    defined(__INTERIX) || defined(__NetBSD__) || defined(__OpenBSD__)
#define	DIRENT_NAMLEN(p)	((p)->d_namlen)
#else /* _AIX || __APPLE__ || __DragonFly__ || __FreeBSD__ || __INTERIX || __NetBSD__ ||
	 __OpenBSD__ */
#define	DIRENT_NAMLEN(p)	(strlen((p)->d_name))
#endif /* _AIX || __APPLE__ || __DragonFly__ || __FreeBSD__ || __INTERIX || __NetBSD__ ||
	  __OpenBSD__ */
#endif /* DIRENT_NAMLEN */

#endif /* CVSYNC_COMPAT_DIRENT_H */
