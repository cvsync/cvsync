/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_DEFS_H
#define	CVSYNC_DEFS_H

uint8_t *cvsup_examine(uint8_t *, uint8_t *);
bool cvsup_decode_dir(struct cvsync_attr *, uint8_t *, uint8_t *);
bool cvsup_decode_file(struct cvsync_attr *, uint8_t *, uint8_t *);

extern uint16_t mode_umask;

#endif /* CVSYNC_DEFS_H */
