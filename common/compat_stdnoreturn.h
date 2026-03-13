/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_COMPAT_STDNORETURN_H
#define	CVSYNC_COMPAT_STDNORETURN_H

/* C23 */
#ifndef NORETURN
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define	NORETURN	[[noreturn]]
#endif /* defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L */
#endif /* NORETURN */

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
