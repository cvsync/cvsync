/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_LOGMSG_H
#define	CVSYNC_LOGMSG_H

enum {
	DEBUG_BASE = 0,
	DEBUG_RDIFF,
	DEBUG_ZLIB,

	DEBUG_MAX
};

bool logmsg_open(const char *, bool);
void logmsg_close(void);

void logmsg(const char *, ...);
void logmsg_debug(int, const char *, ...);
void logmsg_err(const char *, ...);
void logmsg_verbose(const char *, ...);
void logmsg_intr(void);

extern bool logmsg_detail, logmsg_quiet;

#endif /* CVSYNC_LOGMSG_H */
