/*-
 * Copyright (c) 2003-2012 MAEKAWA Masahide <maekawa@cvsync.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
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
