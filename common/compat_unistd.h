/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_COMPAT_UNISTD_H
#define	CVSYNC_COMPAT_UNISTD_H

#if defined(NO_PROTOTYPE_READLINK)
ssize_t readlink(const char *, char *, size_t);
#endif /* defined(NO_PROTOTYPE_READLINK) */

#if defined(NO_PROTOTYPE_SYMLINK)
int symlink(const char *, const char *);
#endif /* defined(NO_PROTOTYPE_SYMLINK) */

#endif /* CVSYNC_COMPAT_UNISTD_H */
