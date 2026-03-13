/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_COMPAT_SYS_STAT_H
#define	CVSYNC_COMPAT_SYS_STAT_H

#if defined(NO_PROTOTYPE_FCHMOD)
int fchmod(int, mode_t);
#endif /* defined(NO_PROTOTYPE_FCHMOD) */

#endif /* CVSYNC_COMPAT_SYS_STAT_H */
