/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_CVSYNC_ATTR_H
#define	CVSYNC_CVSYNC_ATTR_H

#define	CVSYNC_MAXAUXLEN	(PATH_MAX + CVSYNC_NAME_MAX)

struct cvsync_attr {
	uint8_t		ca_tag;

	uint8_t		ca_type;
	uint8_t		ca_name[PATH_MAX + CVSYNC_NAME_MAX + 1];
	size_t		ca_namelen;
	int64_t		ca_mtime;
	uint64_t	ca_size;
	uint16_t	ca_mode;
	uint8_t		ca_aux[CVSYNC_MAXAUXLEN];
	size_t		ca_auxlen;
};

#endif /* CVSYNC_CVSYNC_ATTR_H */
