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

#ifndef __ATTRIBUTE_H__
#define	__ATTRIBUTE_H__

struct cvsync_attr;

#define	MAXATTRLEN		18	/* == max(*_ATTRLEN_*) */

#define	CVSYNC_ALLPERMS		(S_ISUID|S_ISGID|S_ISVTX|\
				 S_IRWXU|S_IRWXG|S_IRWXO)
#define	CVSYNC_UMASK_UNSPEC	((uint16_t)-1)
#define	CVSYNC_UMASK_RCS	(S_IWGRP|S_IWOTH)

#define	RCS_PERMS		(S_IRWXU|S_IRWXG|S_IRWXO)
#define	RCS_MODE(m, u)		((uint16_t)(((m) & ~(u)) & RCS_PERMS))

#define	RCS_ATTRLEN_DIR		2
#define	RCS_ATTRLEN_FILE	18
#define	RCS_ATTRLEN_RCS		10

size_t attr_rcs_encode_dir(uint8_t *, size_t, uint16_t);
size_t attr_rcs_encode_file(uint8_t *, size_t, time_t, off_t, uint16_t);
size_t attr_rcs_encode_rcs(uint8_t *, size_t, time_t, uint16_t);
bool attr_rcs_decode_dir(uint8_t *, size_t, struct cvsync_attr *);
bool attr_rcs_decode_file(uint8_t *, size_t, struct cvsync_attr *);
bool attr_rcs_decode_rcs(uint8_t *, size_t, struct cvsync_attr *);

#endif /* __ATTRIBUTE_H__ */
