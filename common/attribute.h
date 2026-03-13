/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_ATTRIBUTE_H
#define	CVSYNC_ATTRIBUTE_H

struct cvsync_attr;

#define	MAXATTRLEN		(18)	/* == max(*_ATTRLEN_*) */

#define	CVSYNC_ALLPERMS		(S_ISUID|S_ISGID|S_ISVTX|\
				 S_IRWXU|S_IRWXG|S_IRWXO)
#define	CVSYNC_UMASK_UNSPEC	((uint16_t)-1)
#define	CVSYNC_UMASK_RCS	(S_IWGRP|S_IWOTH)

#define	RCS_PERMS		(S_IRWXU|S_IRWXG|S_IRWXO)
#define	RCS_MODE(m, u)		((uint16_t)(((uint16_t)(m) & ~(u)) & RCS_PERMS))

#define	RCS_ATTRLEN_DIR		(2)
#define	RCS_ATTRLEN_FILE	(18)
#define	RCS_ATTRLEN_RCS		(10)

size_t attr_rcs_encode_dir(uint8_t *, size_t, uint16_t);
size_t attr_rcs_encode_file(uint8_t *, size_t, time_t, off_t, uint16_t);
size_t attr_rcs_encode_rcs(uint8_t *, size_t, time_t, uint16_t);
bool attr_rcs_decode_dir(uint8_t *, size_t, struct cvsync_attr *);
bool attr_rcs_decode_file(uint8_t *, size_t, struct cvsync_attr *);
bool attr_rcs_decode_rcs(uint8_t *, size_t, struct cvsync_attr *);

#endif /* CVSYNC_ATTRIBUTE_H */
