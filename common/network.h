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

#ifndef __NETWORK_H__
#define	__NETWORK_H__

#define	CVSYNC_MAXHOST	(1025)
#define	CVSYNC_MAXSERV	(32)
#define	CVSYNC_TICKS	(100) /* msec */
#define	CVSYNC_TIMEOUT	(30000) /* msec */

#define	CVSYNC_SOCKDIR_OUT	(0)
#define	CVSYNC_SOCKDIR_IN	(1)

#if defined(USE_SOCKS5)
int SOCKS5_init(char *);
int SOCKS5_connect(int, const struct sockaddr *, socklen_t);

#define	network_init(p)		SOCKS5_init((p))
#define	connect(f,a,l)		SOCKS5_connect((f), (a), (l))
#else /* defined(USE_SOCKS5) */
#define	network_init(p)
#endif /* defined(USE_SOCKS5) */

int *sock_listen(const char *, const char *);
void sock_listen_stop(int *);
bool sock_init(int *);
void sock_destroy(void);
bool sock_select(void);
int sock_accept(void);
bool sock_wait(int, int);
int sock_connect(int, const char *, const char *);
void sock_close(int);
bool sock_send(int, const void *, size_t);
bool sock_recv(int, void *, size_t);
bool sock_getpeeraddr(int, int *, void *, size_t);
void sock_resolv_addr(int, const char *, char *, size_t);

#endif /* __NETWORK_H__ */
