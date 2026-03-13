/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_COMPAT__COMPILER_H
#define	CVSYNC_COMPAT__COMPILER_H

/*
 * Clang : -Wimplicit-fallthrough
 */

/* Clang */
#ifndef FALLTHROUGH
#if defined(__clang__)
#define	FALLTHROUGH	__attribute__((fallthrough))
#endif /* defined(__clang__) */
#endif /* FALLTHROUGH */

#ifndef FALLTHROUGH
#define	FALLTHROUGH
#endif /* FALLTHROUGH */

#endif /* CVSYNC_COMPAT__COMPILER_H */
