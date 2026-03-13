/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_COMPAT_STDIO_H
#define	CVSYNC_COMPAT_STDIO_H

#if defined(NO_PROTOTYPE_FDOPEN)
FILE *fdopen(int, const char *);
#endif /* defined(NO_PROTOTYPE_FDOPEN) */

#if defined(NO_PROTOTYPE_SNPRINTF)
int snprintf(char *, size_t, const char *, ...);
#endif /* defined(NO_PROTOTYPE_SNPRINTF) */

#if defined(NO_PROTOTYPE_VSNPRINTF)
int vsnprintf(char *, size_t, const char *, va_list);
#endif /* defined(NO_PROTOTYPE_VSNPRINTF) */

#endif /* CVSYNC_COMPAT_STDIO_H */
