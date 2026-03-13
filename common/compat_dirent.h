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

#ifndef CVSYNC_COMPAT_DIRENT_H
#define	CVSYNC_COMPAT_DIRENT_H

#ifndef DIRENT_NAMLEN
#if defined(_AIX) || defined(CVSYNC_APPLE__) || defined(__DragonFly) || \
    defined(CVSYNC_FreeBSD__) || defined(__INTERIX) || defined(__NetBSD) || \
    defined(CVSYNC_OpenBSD)
#define	DIRENT_NAMLEN(p)	((p)->d_namlen)
#else /* _AIX || CVSYNC_APPLE__ || __DragonFly__ || __FreeBSD__ || INTERIX ||
	 CVSYNC_NetBSD__ || __OpenBSD */
#define	DIRENT_NAMLEN(p)	(strlen((p)->d_name))
#endif /* _AIX || CVSYNC_APPLE__ || __DragonFly__ || __FreeBSD__ || INTERIX ||
	  CVSYNC_NetBSD__ || __OpenBSD */
#endif /* DIRENT_NAMLEN */

#endif /* CVSYNC_COMPAT_DIRENT_H */
