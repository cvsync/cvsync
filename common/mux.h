/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_MUX_H
#define	CVSYNC_MUX_H

#define	MUX_MAXCHANNELS		(2)
#define	MUX_IN			(0)
#define	MUX_OUT			(1)

#define	MUX_MIN_MSS		(1024)	/* 1KB */
#define	MUX_DEFAULT_MSS		(2048)	/* 2KB */
#define	MUX_MAX_MSS		(4096)	/* 4KB */
#define	MUX_MAX_MSS_ZLIB	(MUX_MAX_MSS * 2)

#define	MUX_MIN_BUFSIZE		(8192)	/* 8KB */
#define	MUX_DEFAULT_BUFSIZE	(16384)	/* 16KB */
#define	MUX_MAX_BUFSIZE		(32768)	/* 32KB */

#define	MUX_DIRCMP		(0)	/* DirScan -> DirCmp */
#define	MUX_FILESCAN_IN		(0)	/* FileScan <- DirCmp */
#define	MUX_FILECMP		(1)	/* FileScan -> FileCmp */
#define	MUX_UPDATER_IN		(1)	/* Updater <- FileCmp */

#define	MUX_DIRCMP_IN		(0)	/* DirCmp <- DirScan */
#define	MUX_FILESCAN		(0)	/* DirCmp -> FileScan */
#define	MUX_FILECMP_IN		(1)	/* FileCmp <- FileScan */
#define	MUX_UPDATER		(1)	/* FileCmp -> Updater */

#define	MUX_CMD_DATA		(0x00)
#define	MUX_CMD_RESET		(0x01)
#define	MUX_CMD_CLOSE		(0x02)

#define	MUX_CMDLEN_DATA		(4)
#define	MUX_CMDLEN_RESET	(6)
#define	MUX_CMDLEN_CLOSE	(2)
#define	MUX_MAXCMDLEN		(6) /* max(MUX_CMDLEN_{DATA,RESET,CLOSE}) */

enum mux_state {
	MUX_STATE_INIT,
	MUX_STATE_RUNNING,
	MUX_STATE_CLOSED,
	MUX_STATE_ERROR
};

struct muxbuf {
	uint8_t		*mxb_buffer;
	uint32_t	mxb_bufsize, mxb_length, mxb_head, mxb_rlength;
	uint16_t	mxb_mss, mxb_size;
	enum mux_state	mxb_state;

	pthread_mutex_t	mxb_lock;
	pthread_cond_t	mxb_wait_in, mxb_wait_out;
};

struct mux {
	int		mx_socket;
	struct muxbuf	mx_buffer[2][MUX_MAXCHANNELS];
	uint8_t		mx_recvcmd[MUX_MAXCMDLEN];
	bool		mx_state[2][MUX_MAXCHANNELS];

	pthread_t	mx_receiver;
	pthread_mutex_t	mx_lock;
	pthread_cond_t	mx_wait;
	bool		mx_isconnected;

	uint64_t	mx_xfer_in, mx_xfer_out;
	int		mx_compress;
	void		*mx_stream;
};

struct mux *mux_init(int, uint16_t, int, int);
void mux_destroy(struct mux *);
bool muxbuf_init(struct muxbuf *, uint16_t, uint32_t, int);
void muxbuf_destroy(struct muxbuf *);

bool mux_send(struct mux *, uint8_t, const void *, size_t);
bool mux_recv(struct mux *, uint8_t, void *, size_t);
bool mux_flush(struct mux *, uint8_t);
bool mux_close_in(struct mux *, uint8_t);
bool mux_close_out(struct mux *, uint8_t);
void mux_abort(struct mux *);

bool mux_send_raw(struct mux *, uint8_t, const void *, size_t);
bool mux_flush_raw(struct mux *, uint8_t);

bool mux_init_zlib(struct mux *, int);
void mux_destroy_zlib(struct mux *);
bool mux_send_zlib(struct mux *, uint8_t, const void *, size_t);
bool mux_flush_zlib(struct mux *, uint8_t);

#endif /* CVSYNC_MUX_H */
