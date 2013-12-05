/*-
 * Copyright (c) 2000-2012 MAEKAWA Masahide <maekawa@cvsync.org>
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

#include <stdarg.h>
#include <stdio.h>

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "compat_stdbool.h"
#include "compat_stdio.h"

#include "cvsync.h"
#include "logmsg.h"

#define	LINE_MAXLEN	(256)	/* incl. NULL */

bool logmsg_detail = false, logmsg_quiet = false;

static bool logmsg_syslog = false, logmsg_file = false;
static bool logmsg_interrupted = false;
static FILE *logmsg_stdout = NULL, *logmsg_stderr = NULL;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static bool logmsg_do_debug[DEBUG_MAX] = {
	false,	/* DEBUG_BASE */
	false,	/* DEBUG_RDIFF */
	false	/* DEBUG_ZLIB */
};

void __logmsg(int, FILE *, const char *, va_list);

bool
logmsg_open(const char *logname, bool use_syslog)
{
	FILE *fp;
	int fd;

	if ((logname == NULL) || (strlen(logname) == 0)) {
		if (use_syslog) {
			logmsg_syslog = true;
			openlog("cvsyncd", LOG_PID, LOG_USER);
		}
	} else {
		if ((fd = open(logname, O_WRONLY|O_APPEND, 0)) == -1) {
			if (errno != ENOENT) {
				logmsg_err("%s: %s", logname, strerror(errno));
				return (false);
			}
			fd = open(logname, O_CREAT|O_TRUNC|O_WRONLY|O_EXCL,
				  S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
		}
		if (fd == -1) {
			logmsg_err("%s: %s", logname, strerror(errno));
			return (false);
		}
		if ((fp = fdopen(fd, "a")) == NULL) {
			logmsg_err("%s: %s", logname, strerror(errno));
			(void)close(fd);
			return (false);
		}

		logmsg_stdout = fp;
		logmsg_stderr = fp;

		logmsg_file = true;

		tzset();
	}

	return (true);
}

void
logmsg_close(void)
{
	if (logmsg_syslog)
		closelog();
	if (logmsg_file) {
		(void)fclose(logmsg_stdout);
		logmsg_file = false;
	}
}

void
logmsg(const char *message, ...)
{
	va_list ap;

	logmsg_intr();

	if (logmsg_quiet)
		return;

	va_start(ap, message);
	__logmsg(LOG_INFO, logmsg_stdout, message, ap);
	va_end(ap);
}

void
logmsg_debug(int priority, const char *message, ...)
{
	va_list ap;

	logmsg_intr();

	if (!logmsg_do_debug[priority])
		return;

	va_start(ap, message);
	__logmsg(LOG_DEBUG, logmsg_stdout, message, ap);
	va_end(ap);
}

void
logmsg_err(const char *message, ...)
{
	va_list ap;

	logmsg_intr();

	va_start(ap, message);
	__logmsg(LOG_ERR, logmsg_stderr, message, ap);
	va_end(ap);
}

void
logmsg_verbose(const char *message, ...)
{
	va_list ap;

	logmsg_intr();

	if (!logmsg_detail)
		return;
	if (logmsg_quiet)
		return;

	va_start(ap, message);
	__logmsg(LOG_INFO, logmsg_stdout, message, ap);
	va_end(ap);
}

void
logmsg_intr(void)
{
	struct tm tm;
	time_t now;
	char timemsg[LINE_MAXLEN];
	size_t wn;

	if (logmsg_stdout == NULL)
		logmsg_stdout = stdout;
	if (logmsg_stderr == NULL)
		logmsg_stderr = stderr;

	if (!cvsync_isinterrupted() || cvsync_isterminated())
		return;

	if (logmsg_file || logmsg_syslog)
		return;

	pthread_mutex_lock(&mtx);

	if (!logmsg_interrupted) {
		if (!cvsync_isterminated()) {
			(void)fflush(NULL);

			if (!logmsg_file) {
				timemsg[0] = '\0';
				wn = 0;
			} else {
				time(&now);
				wn = strftime(timemsg, sizeof(timemsg),
					      "[%Y/%m/%d %H:%M:%S] ",
					      localtime_r(&now, &tm));
			}
			if (wn > 0) {
				(void)write(fileno(logmsg_stdout), timemsg,
					    wn);
			}
			(void)write(fileno(logmsg_stdout), "Interrupted\n",
				    12);
		}
		logmsg_interrupted = true;
	}

	pthread_mutex_unlock(&mtx);
}

void
__logmsg(int priority, FILE *fp, const char *message, va_list ap)
{
	struct tm tm;
	time_t now;
	char timemsg[LINE_MAXLEN], msg[LINE_MAXLEN];
	size_t wn;

	vsnprintf(msg, sizeof(msg), message, ap);

	if (logmsg_syslog) {
		syslog(priority, "%s", msg);
	} else {
		if (!logmsg_file) {
			timemsg[0] = '\0';
			wn = 0;
		} else {
			time(&now);
			wn = strftime(timemsg, sizeof(timemsg),
				      "[%Y/%m/%d %H:%M:%S] ",
				      localtime_r(&now, &tm));
		}
		pthread_mutex_lock(&mtx);
		if (!logmsg_interrupted || logmsg_do_debug[DEBUG_BASE]) {
			(void)fprintf(fp, "%.*s%s\n", (int)wn, timemsg, msg);
			(void)fflush(fp);
		}
		pthread_mutex_unlock(&mtx);
	}
}
