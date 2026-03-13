/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_RECEIVER_H
#define	CVSYNC_RECEIVER_H

void *receiver(void *);
bool receiver_close(struct mux *, uint8_t);
bool receiver_reset(struct mux *, uint8_t);

bool receiver_data_raw(struct mux *, uint8_t);
bool receiver_data_zlib(struct mux *, uint8_t);

#endif /* CVSYNC_RECEIVER_H */
