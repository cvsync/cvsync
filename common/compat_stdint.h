/*-
 * Copyright (c) 2000-2012 MAEKAWA Masahide <maekawa@cvsync.org>
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

#ifndef __COMPAT_STDINT_H__
#define	__COMPAT_STDINT_H__

#if defined(__APPLE__) || defined(__NetBSD__) || defined(__QNX__) || \
    defined(__sun)
#if !defined(NO_STDINT_H)
#include <stdint.h>
#endif /* defined(NO_STDINT_H) */
#endif /* __APPLE__ || __NetBSD__ || __QNX__ || __sun */

#ifndef INT64_MAX
#define	INT64_MAX	(9223372036854775807LL)
#endif /* INT64_MAX */

#ifndef UINT8_MAX
#define	UINT8_MAX	(255U)
#endif /* UINT8_MAX */

#ifndef UINT16_MAX
#define	UINT16_MAX	(65535U)
#endif /* UINT16_MAX */

#ifndef UINT64_MAX
#define	UINT64_MAX	(18446744073709551615ULL)
#endif /* UINT64_MAX */

#ifndef SIZE_MAX
#define	SIZE_MAX	((size_t)-1)
#endif /* SIZE_MAX */

#endif /* __COMPAT_STDINT_H__ */
