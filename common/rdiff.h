/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_RDIFF_H
#define	CVSYNC_RDIFF_H

struct hash_args;
struct mux;

#define	RDIFF_MIN_BLOCKSIZE	(512)
#define	RDIFF_MAX_BLOCKSIZE	(65536)
#define	RDIFF_NBLOCKS		(128)

#define	RDIFF_CMD_EOF		(0x00)
#define	RDIFF_CMD_COPY		(0x01)
#define	RDIFF_CMD_DATA		(0x02)

#define	RDIFF_MAXCMDLEN		(13)

#define	RDIFF_WEAK_LOW(x)	((uint16_t)(x))
#define	RDIFF_WEAK_HIGH(x)	((uint16_t)((x) >> 16))

uint32_t rdiff_weak(const uint8_t *, size_t);
uint8_t *rdiff_search(uint8_t *, uint8_t *, uint32_t, size_t, uint32_t,
			    uint8_t *, const struct hash_args *);

bool rdiff_copy(struct mux *, uint8_t, off_t, size_t);
bool rdiff_data(struct mux *, uint8_t, const void *, size_t);
bool rdiff_eof(struct mux *, uint8_t);

#endif /* CVSYNC_RDIFF_H */
