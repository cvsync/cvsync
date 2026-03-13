/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_COMPAT_ARPA_INET_H
#define	CVSYNC_COMPAT_ARPA_INET_H

#if defined(NO_PROTOTYPE_INET_PTON)
int inet_pton(int, const char *, void *);
#endif /* defined(NO_PROTOTYPE_INET_PTON) */

#endif /* CVSYNC_COMPAT_ARPA_INET_H */
