/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_COMPAT_SIGNAL_H
#define	CVSYNC_COMPAT_SIGNAL_H

#if defined(__bsdi__)
#ifdef SIG_IGN
#undef SIG_IGN
#define	SIG_IGN		((void (*)(int))1)
#endif /* SIG_IGN */
#endif /* __bsdi__ */

#endif /* CVSYNC_COMPAT_SIGNAL_H */
