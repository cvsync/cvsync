/*-
 * Copyright (c) 2002-2005 MAEKAWA Masahide <maekawa@cvsync.org>
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

#ifndef __RDIFF_H__
#define	__RDIFF_H__

struct hash_args;
struct mux;

#define	RDIFF_MIN_BLOCKSIZE	512
#define	RDIFF_MAX_BLOCKSIZE	65536
#define	RDIFF_NBLOCKS		128

#define	RDIFF_CMD_EOF		0x00
#define	RDIFF_CMD_COPY		0x01
#define	RDIFF_CMD_DATA		0x02

#define	RDIFF_MAXCMDLEN		13

#define	RDIFF_WEAK_LOW(x)	((uint16_t)(x))
#define	RDIFF_WEAK_HIGH(x)	((uint16_t)((x) >> 16))

uint32_t rdiff_weak(const uint8_t *, size_t);
uint8_t *rdiff_search(uint8_t *, uint8_t *, uint32_t, size_t, uint32_t,
			    uint8_t *, const struct hash_args *);

bool rdiff_copy(struct mux *, uint8_t, off_t, size_t);
bool rdiff_data(struct mux *, uint8_t, const void *, size_t);
bool rdiff_eof(struct mux *, uint8_t);

#endif /* __RDIFF_H__ */
