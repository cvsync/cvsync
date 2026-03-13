/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_MUX_ZLIB_H
#define	CVSYNC_MUX_ZLIB_H

struct mux_stream_zlib {
	z_stream	ms_zstream_in;
	uint8_t		ms_zbuffer_in[MUX_MAX_MSS * 2];
	size_t		ms_zbufsize_in;

	z_stream	ms_zstream_out;
	uint8_t		ms_zbuffer_out[MUX_MAX_MSS * 2];
	size_t		ms_zbufsize_out;
};

#endif /* CVSYNC_MUX_ZLIB_H */
