/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_VERSION_H
#define	CVSYNC_VERSION_H

#define	CVSYNC_MAJOR		(0)
#define	CVSYNC_MINOR		(24)
#define	CVSYNC_PATCHLEVEL	(21)

#define	CVSYNC_PROTO_MAJOR	CVSYNC_MAJOR
#define	CVSYNC_PROTO_MINOR	CVSYNC_MINOR
#define	CVSYNC_PROTO_ERROR	(0xff)

#define	CVSYNC_PROTO(j, n)	((uint32_t)(((j) << 16) | (n)))

#define	CVSYNC_URL	"http://www.cvsync.org/"

#endif /* CVSYNC_VERSION_H */
