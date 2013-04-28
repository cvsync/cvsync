/*-
 * Copyright (c) 2000-2005 MAEKAWA Masahide <maekawa@cvsync.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
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
attr_rcs_encode_file(uint8_t *buffer, size_t bufsize, time_t mtime, off_t size,
		 uint16_t mode)
{
	if (bufsize < RCS_ATTRLEN_FILE)
		return (0);

	SetDDWord(buffer, mtime);
	SetDDWord(&buffer[8], size);
	SetWord(&buffer[16], mode);

	return (RCS_ATTRLEN_FILE);
}

size_t
attr_rcs_encode_rcs(uint8_t *buffer, size_t bufsize, time_t mtime,
		    uint16_t mode)
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
