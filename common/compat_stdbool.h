/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_COMPAT_STDBOOL_H
#define	CVSYNC_COMPAT_STDBOOL_H

#if !defined(NO_STDBOOL_H)
#include <stdbool.h>
#else /* !defined(NO_STDBOOL_H) */
#define	_Bool	int
#define	bool	_Bool
#define	true	(1)
#define	false	(0)

#define	__bool_true_false_are_defined	(1)
#endif /* defined(NO_STDBOOL_H) */

#endif /* CVSYNC_COMPAT_STDBOOL_H */
