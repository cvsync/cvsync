/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_COMPAT_STDNORETURN_H
#define	CVSYNC_COMPAT_STDNORETURN_H

/* GCC or Clang */
#ifndef NORETURN
#if defined(__GNUC__) || defined(__clang__)
#define	NORETURN	__attribute__((noreturn))
#endif /* defined(__GNUC__) || defined(__clang__) */
#endif /* NORETURN */

#ifndef NORETURN
#define	NORETURN
#endif /* NORETURN */

#endif /* CVSYNC_COMPAT_STDNORETURN_H */
