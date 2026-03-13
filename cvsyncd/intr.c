/*-
 * This software is released under the BSD License, see LICENSE.
 */

#include <sys/types.h>

#include "compat_stdbool.h"

#include "cvsync.h"

extern bool cvsync_is_interrupted_internal(void);
extern bool cvsync_is_terminated_internal(void);

bool
cvsync_is_interrupted(void)
{
	return (cvsync_is_interrupted_internal());
}

bool
cvsync_is_terminated(void)
{
	return (cvsync_is_terminated_internal());
}
