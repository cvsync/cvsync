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

#ifndef __COMPAT_NETDB_H__
#define	__COMPAT_NETDB_H__

#if defined(_AIX)
#ifndef DECLARE_ADDRINFO
#define	DECLARE_ADDRINFO
struct addrinfo {
	int		ai_flags;
	int		ai_family;
	int		ai_socktype;
	int		ai_protocol;
	size_t		ai_addrlen;
	char		*ai_canonname;
	struct sockaddr	*ai_addr;
	struct addrinfo *ai_next;
};
#endif /* DECLARE_ADDRINFO */

#ifndef AI_PASSIVE
#define	AI_PASSIVE	(0x02)
#endif /* AI_PASSIVE */

#ifndef NI_NUMERICHOST
#define	NI_NUMERICHOST	(0x2)
#endif /* NI_NUMERICHOST */
#ifndef NI_NAMEREQD
#define	NI_NAMEREQD	(0x4)
#endif /* NI_NAMEREQD */
#ifndef NI_NUMERICSERV
#define	NI_NUMERICSERV	(0x8)
#endif /* NI_NUMERICSERV */
#endif /* defined(_AIX) */

#if defined(NO_SOCKADDR_STORAGE)
#define	_SS_MAXSIZE	(128)
#define	_SS_ALIGNSIZE	(sizeof(int64_t))
#if defined(NO_SOCKADDR_LEN)
#define	_SS_PAD1SIZE	(_SS_ALIGNSIZE - sizeof(sa_family_t))
#define	_SS_PAD2SIZE	(_SS_MAXSIZE - (sizeof(sa_family_t) + \
			 _SS_PAD1SIZE + _SS_ALIGNSIZE))
#else /* defined(NO_SOCKADDR_LEN) */
#define	_SS_PAD1SIZE	(_SS_ALIGNSIZE - sizeof(uint8_t) - sizeof(sa_family_t))
#define	_SS_PAD2SIZE	(_SS_MAXSIZE - (sizeof(uint8_t) + \
			 sizeof(sa_family_t) + _SS_PAD1SIZE + _SS_ALIGNSIZE))
#endif /* defined(NO_SOCKADDR_LEN) */

struct sockaddr_storage {
#if !defined(NO_SOCKADDR_LEN)
	uint8_t		ss_len;
#endif /* !defined(NO_SOCKADDR_LEN) */
	sa_family_t	ss_family;
	char		_ss_pad1[_SS_PAD1SIZE];
	int64_t		_ss_align;
	char		_ss_pad2[_SS_PAD2SIZE];
};
#endif /* defined(NO_SOCKADDR_STORAGE) */

#if defined(NO_PROTOTYPE_FREEADDRINFO)
void freeaddrinfo(struct addrinfo *);
#endif /* defined(NO_PROTOTYPE_FREEADDRINFO) */

#if defined(NO_PROTOTYPE_GAI_STRERROR)
const char *gai_strerror(int);
#endif /* defined(NO_PROTOTYPE_GAI_STRERROR) */

#if defined(NO_PROTOTYPE_GETADDRINFO)
int getaddrinfo(const char *, const char *, const struct addrinfo *,
		struct addrinfo **);
#endif /* defined(NO_PROTOTYPE_GETADDRINFO) */

#if defined(NO_PROTOTYPE_GETNAMEINFO)
int getnameinfo(const struct sockaddr *, socklen_t, char *, socklen_t, char *,
		socklen_t, unsigned);
#endif /* defined(NO_PROTOTYPE_GETNAMEINFO) */

#endif /* __COMPAT_NETDB_H__ */
