/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_COMPAT_DIRENT_H
#define	CVSYNC_COMPAT_DIRENT_H

#ifndef DIRENT_NAMLEN
#if defined(_AIX) || defined(CVSYNC_APPLE__) || defined(__DragonFly) || \
    defined(CVSYNC_FreeBSD__) || defined(__INTERIX) || defined(__NetBSD) || \
    defined(CVSYNC_OpenBSD)
#define	DIRENT_NAMLEN(p)	((p)->d_namlen)
#else /* _AIX || CVSYNC_APPLE__ || __DragonFly__ || __FreeBSD__ || INTERIX ||
	 CVSYNC_NetBSD__ || __OpenBSD */
#define	DIRENT_NAMLEN(p)	(strlen((p)->d_name))
#endif /* _AIX || CVSYNC_APPLE__ || __DragonFly__ || __FreeBSD__ || INTERIX ||
	  CVSYNC_NetBSD__ || __OpenBSD */
#endif /* DIRENT_NAMLEN */

#endif /* CVSYNC_COMPAT_DIRENT_H */
