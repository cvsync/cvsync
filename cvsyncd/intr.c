/*-
 * This software is released under the BSD License, see LICENSE.
 */

#include <sys/types.h>

#include "compat_stdbool.h"

#include "cvsync.h"

extern bool __cvsync_isinterrupted(void);
extern bool __cvsync_isterminated(void);

bool
cvsync_isinterrupted(void)
{
	return (__cvsync_isinterrupted());
}

bool
cvsync_isterminated(void)
{
	return (__cvsync_isterminated());
}
