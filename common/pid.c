/*-
 * This software is released under the BSD License, see LICENSE.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include "compat_stdbool.h"
#include "compat_stdio.h"
#include "compat_stdlib.h"
#include "compat_limits.h"

#include "logmsg.h"
#include "pid.h"

static char cvsync_pidfile[PATH_MAX + CVSYNC_NAME_MAX + 1];

char *
pid_create(const char *pidfile)
{
	char pid[64];
	ssize_t len;
	int fd, wn;

	cvsync_pidfile[0] = '\0';

	if (pidfile == NULL)
		return (cvsync_pidfile);

	if (pidfile[0] != '/') {
		if (realpath(pidfile, cvsync_pidfile) == NULL) {
			logmsg_err("%s: %s", pidfile, strerror(errno));
			return (NULL);
		}
	} else {
		wn = snprintf(cvsync_pidfile, sizeof(cvsync_pidfile), "%s",
			      pidfile);
		if ((wn <= 0) || ((size_t)wn >= sizeof(cvsync_pidfile)))
			return (NULL);
	}

	if ((fd = open(cvsync_pidfile, O_WRONLY|O_CREAT|O_EXCL|O_TRUNC,
		       S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0) {
		if (errno == EEXIST)
			logmsg_err("Another cvsync process is running?");
		else
			logmsg_err("%s: %s", cvsync_pidfile, strerror(errno));
		cvsync_pidfile[0] = '\0';
		return (NULL);
	}
	wn = snprintf(pid, sizeof(pid), "%d\n", (signed)getpid());
	if ((wn <= 0) || ((size_t)wn >= sizeof(pid))) {
		(void)unlink(cvsync_pidfile);
		cvsync_pidfile[0] = '\0';
		return (NULL);
	}
	if ((len = write(fd, pid, (size_t)wn)) == -1) {
		(void)unlink(cvsync_pidfile);
		cvsync_pidfile[0] = '\0';
		return (NULL);
	}
	if (len != (ssize_t)wn) {
		(void)unlink(cvsync_pidfile);
		cvsync_pidfile[0] = '\0';
		return (NULL);
	}
	if (close(fd) == -1) {
		logmsg_err("%s: %s", cvsync_pidfile, strerror(errno));
		(void)unlink(cvsync_pidfile);
		cvsync_pidfile[0] = '\0';
		return (NULL);
	}

	return (cvsync_pidfile);
}

bool
pid_remove(void)
{
	char pid[64], *ep;
	ssize_t rn;
	long npid;
	int fd;

	if (strlen(cvsync_pidfile) == 0)
		return (true);

	if ((fd = open(cvsync_pidfile, O_RDONLY, 0)) == -1) {
		if (errno == ENOENT)
			return (true);
		logmsg_err("%s: %s", cvsync_pidfile, strerror(errno));
		return (false);
	}
	if ((rn = read(fd, pid, sizeof(pid) - 1)) == -1) {
		logmsg_err("%s: %s", cvsync_pidfile, strerror(errno));
		(void)close(fd);
		return (false);
	}
	pid[rn] = '\0';
	errno = 0;
	npid = strtol(pid, &ep, 10);
	if ((ep == NULL) || (*ep != '\n') ||
	    ((npid == 0) && (errno == EINVAL)) ||
	    (((npid == LONG_MIN) || (npid == LONG_MAX)) &&
	     (errno == ERANGE))) {
		logmsg_err("%s: %s: %s", cvsync_pidfile, pid,
			   strerror(EINVAL));
		(void)close(fd);
		return (false);
	}
	if (close(fd) == -1) {
		logmsg_err("%s: %s", cvsync_pidfile, strerror(errno));
		return (false);
	}
	if ((pid_t)npid != getpid())
		return (false);

	if (unlink(cvsync_pidfile) == -1) {
		logmsg_err("%s: %s", cvsync_pidfile, strerror(errno));
		return (false);
	}

	return (true);
}
