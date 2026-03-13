/*-
 * This software is released under the BSD License, see LICENSE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <stdlib.h>

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <pthread.h>
#include <pwd.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"
#include "compat_limits.h"
#include "compat_paths.h"
#include "compat_unistd.h"

#include "cvsync.h"
#include "logmsg.h"
#include "network.h"
#include "pid.h"

#include "defs.h"

bool
privilege_drop(const char *uidname, const char *gidname)
{
	struct group *grp;
	struct passwd *pwd;
	gid_t gid = (gid_t)-1;
	uid_t uid = (uid_t)-1;

	if (gidname != NULL) {
		if ((grp = getgrnam(gidname)) == NULL) {
			logmsg_err("GID %s: %s", gidname, strerror(errno));
			return (false);
		}
		if (getgid() != (gid_t)grp->gr_gid)
			gid = grp->gr_gid;
	}
	if (uidname != NULL) {
		if ((pwd = getpwnam(uidname)) == NULL) {
			logmsg_err("UID %s: %s", uidname, strerror(errno));
			return (false);
		}
		if (getuid() != (uid_t)pwd->pw_uid)
			uid = pwd->pw_uid;
	}
	if ((gid != (gid_t)-1) || (uid != (uid_t)-1)) {
		if ((gid != (gid_t)-1) && (setgid(gid) == -1)) {
			logmsg_err("%s", strerror(errno));
			return (false);
		}
		if ((uid != (uid_t)-1) && (setuid(uid) == -1)) {
			logmsg_err("%s", strerror(errno));
			return (false);
		}
	}

	return (true);
}

bool
daemonize(const char *pidfile, const char *uidname, const char *gidname)
{
	const char *devnull = CVSYNC_PATH_DEV_NULL;
	struct sigaction act;
	int fd;

	switch (fork()) {
	case -1:
		logmsg_err("daemonize: %s", strerror(errno));
		return (false);
	case 0:
		break;
	default:
		_exit(EXIT_SUCCESS);
		/* NOTREACHED */
	}

	if (setsid() == -1) {
		logmsg_err("daemonize: %s", strerror(errno));
		return (false);
	}
	if (sigaction(SIGHUP, NULL, &act) == -1) {
		logmsg_err("daemonize: %s", strerror(errno));
		return (false);
	}
	act.sa_handler = cvsync_signal;
	if (sigaction(SIGHUP, &act, NULL) == -1) {
		logmsg_err("daemonize: %s", strerror(errno));
		return (false);
	}

	switch (fork()) {
	case -1:
		logmsg_err("daemonize: %s", strerror(errno));
		return (false);
	case 0:
		break;
	default:
		_exit(EXIT_SUCCESS);
		/* NOTREACHED */
	}

	if (devnull != NULL) {
		if ((fd = open(devnull, O_RDWR, 0)) == -1) {
			logmsg_err("daemonize: %s: %s", devnull,
				   strerror(errno));
			return (false);
		}
		if (dup2(fd, STDIN_FILENO) == -1) {
			logmsg_err("daemonize: %s", strerror(errno));
			return (false);
		}
		if (dup2(fd, STDOUT_FILENO) == -1) {
			logmsg_err("daemonize: %s", strerror(errno));
			return (false);
		}
		if (dup2(fd, STDERR_FILENO) == -1) {
			logmsg_err("daemonize: %s", strerror(errno));
			return (false);
		}
		if (close(fd) == -1) {
			logmsg_err("daemonize: %s", strerror(errno));
			return (false);
		}
	} else {
		(void)close(STDIN_FILENO);
		(void)close(STDOUT_FILENO);
		(void)close(STDERR_FILENO);
	}

	if (!privilege_drop(uidname, gidname))
		return (false);

	if (pid_create(pidfile) == NULL)
		return (false);

	if (chdir("/") == -1) {
		logmsg_err("daemonize: %s", strerror(errno));
		pid_remove();
		return (false);
	}
	(void)umask(S_IWGRP|S_IWOTH);

	return (true);
}
