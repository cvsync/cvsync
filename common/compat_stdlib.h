/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_COMPAT_STDLIB_H
#define	CVSYNC_COMPAT_STDLIB_H

#if defined(NO_PROTOTYPE_MKSTEMP)
int mkstemp(char *);
#endif /* defined(NO_PROTOTYPE_MKSTEMP) */

#if defined(NO_PROTOTYPE_REALPATH)
char *realpath(const char *, char *);
#endif /* defined(NO_PROTOTYPE_REALPATH) */

#endif /* CVSYNC_COMPAT_STDLIB_H */
