/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_COMPAT_PATHS_H
#define	CVSYNC_COMPAT_PATHS_H

#ifndef CVSYNC_PATH_DEV_NULL
#if defined(CVSYNC_CYGWIN)
#define	CVSYNC_PATH_DEV_NULL	NULL
#endif /* CVSYNC_CYGWIN */
#if defined(__sun)
#define	CVSYNC_PATH_DEV_NULL	"/dev/null"
#endif /* defined(__sun) */
#endif /* CVSYNC_PATH_DEV_NULL */
#ifndef CVSYNC_PATH_DEV_NULL
#include <paths.h>
#define	CVSYNC_PATH_DEV_NULL	_PATH_DEVNULL
#endif /* CVSYNC_PATH_DEV_NULL */

#endif /* CVSYNC_COMPAT_PATHS_H */
