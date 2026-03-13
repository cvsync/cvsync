/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_COMPAT_STRINGS_H
#define	CVSYNC_COMPAT_STRINGS_H

#if defined(NO_PROTOTYPE_STRCASECMP)
int strcasecmp(const char *, const char *);
#endif /* defined(NO_PROTOTYPE_STRCASECMP) */

#if defined(NO_PROTOTYPE_STRNCASECMP)
int strncasecmp(const char *, const char *, size_t);
#endif /* defined(NO_PROTOTYPE_STRNCASECMP) */

#endif /* CVSYNC_COMPAT_STRINGS_H */
