/*-
 * This software is released under the BSD License, see LICENSE.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <limits.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"
#include "compat_limits.h"
#include "basedef.h"

#include "attribute.h"
#include "cvsync.h"
#include "cvsync_attr.h"

size_t
attr_rcs_encode_dir(uint8_t *buffer, size_t bufsize, uint16_t mode)
{
	if (bufsize < RCS_ATTRLEN_DIR)
		return (0);

	SetWord(buffer, mode);

	return (RCS_ATTRLEN_DIR);
}

size_t
attr_rcs_encode_file(uint8_t *buffer, size_t bufsize, time_t mtime, off_t size, uint16_t mode)
{
	if (bufsize < RCS_ATTRLEN_FILE)
		return (0);

	SetDDWord(buffer, mtime);
	SetDDWord(&buffer[8], size);
	SetWord(&buffer[16], mode);

	return (RCS_ATTRLEN_FILE);
}

size_t
attr_rcs_encode_rcs(uint8_t *buffer, size_t bufsize, time_t mtime, uint16_t mode)
{
	if (bufsize < RCS_ATTRLEN_RCS)
		return (0);

	SetDDWord(buffer, mtime);
	SetWord(&buffer[8], mode);

	return (RCS_ATTRLEN_RCS);
}

bool
attr_rcs_decode_dir(uint8_t *buffer, size_t bufsize, struct cvsync_attr *cap)
{
	if (bufsize < RCS_ATTRLEN_DIR)
		return (false);

	cap->ca_mode = GetWord(buffer);

	return (true);
}

bool
attr_rcs_decode_file(uint8_t *buffer, size_t bufsize, struct cvsync_attr *cap)
{
	if (bufsize < RCS_ATTRLEN_FILE)
		return (false);

	cap->ca_mtime = (int64_t)GetDDWord(buffer);
	cap->ca_size = GetDDWord(&buffer[8]);
	cap->ca_mode = GetWord(&buffer[16]);

	return (true);
}

bool
attr_rcs_decode_rcs(uint8_t *buffer, size_t bufsize, struct cvsync_attr *cap)
{
	if (bufsize < RCS_ATTRLEN_RCS)
		return (false);

	cap->ca_mtime = (int64_t)GetDDWord(buffer);
	cap->ca_mode = GetWord(&buffer[8]);

	return (true);
}
