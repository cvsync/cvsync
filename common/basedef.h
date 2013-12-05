/*-
 * Copyright (c) 2000-2013 MAEKAWA Masahide <maekawa@cvsync.org>
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

#ifndef __BASEDEF_H__
#define	__BASEDEF_H__

#define	GetWord(w)	((uint16_t)((((uint16_t)((w)[0])) << 8) | \
				    (((uint16_t)((w)[1])) << 0)))

#define	GetDWord(w)	((uint32_t)((((uint32_t)((w)[0])) << 24) | \
				    (((uint32_t)((w)[1])) << 16) | \
				    (((uint32_t)((w)[2])) <<  8) | \
				    (((uint32_t)((w)[3])) <<  0)))

#define	GetDDWord(w)	((uint64_t)((((uint64_t)((w)[0])) << 56) | \
				    (((uint64_t)((w)[1])) << 48) | \
				    (((uint64_t)((w)[2])) << 40) | \
				    (((uint64_t)((w)[3])) << 32) | \
				    (((uint64_t)((w)[4])) << 24) | \
				    (((uint64_t)((w)[5])) << 16) | \
				    (((uint64_t)((w)[6])) <<  8) | \
				    (((uint64_t)((w)[7])) <<  0)))

#define	SetWord(w, v) \
	do { \
		(w)[0] = (uint8_t)((uint16_t)(v) >> 8); \
		(w)[1] = (uint8_t)((uint16_t)(v) >> 0); \
	} while (/* CONSTCOND */ 0)

#define	SetDWord(w, v) \
	do { \
		(w)[0] = (uint8_t)((uint32_t)(v) >> 24); \
		(w)[1] = (uint8_t)((uint32_t)(v) >> 16); \
		(w)[2] = (uint8_t)((uint32_t)(v) >>  8); \
		(w)[3] = (uint8_t)((uint32_t)(v) >>  0); \
	} while (/* CONSTCOND */ 0)

#define	SetDDWord(w, v) \
	do { \
		(w)[0] = (uint8_t)((uint64_t)(v) >> 56); \
		(w)[1] = (uint8_t)((uint64_t)(v) >> 48); \
		(w)[2] = (uint8_t)((uint64_t)(v) >> 40); \
		(w)[3] = (uint8_t)((uint64_t)(v) >> 32); \
		(w)[4] = (uint8_t)((uint64_t)(v) >> 24); \
		(w)[5] = (uint8_t)((uint64_t)(v) >> 16); \
		(w)[6] = (uint8_t)((uint64_t)(v) >>  8); \
		(w)[7] = (uint8_t)((uint64_t)(v) >>  0); \
	} while (/* CONSTCOND */ 0)

#endif /* __BASEDEF_H__ */
