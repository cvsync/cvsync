/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_NETWORK_H
#define	CVSYNC_NETWORK_H

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

#endif /* CVSYNC_NETWORK_H */
