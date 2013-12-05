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
#include <sys/socket.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_stdio.h"
#include "compat_stdlib.h"
#include "compat_inttypes.h"
#include "compat_limits.h"

#include "collection.h"
#include "cvsync.h"
#include "cvsync_attr.h"
#include "hash.h"
#include "logmsg.h"
#include "mux.h"
#include "network.h"
#include "pid.h"
#include "version.h"

#include "receiver.h"
#include "dircmp.h"
#include "filecmp.h"

#include "defs.h"

#ifndef CVSYNCD_DEFAULT_HALTFILE
#define	CVSYNCD_DEFAULT_HALTFILE	"/var/run/cvsyncd.HALT"
#endif /* CVSYNCD_DEFAULT_HALTFILE */

#ifndef CVSYNCD_DEFAULT_PIDFILE
#define	CVSYNCD_DEFAULT_PIDFILE		"/var/run/cvsyncd.pid"
#endif /* CVSYNCD_DEFAULT_PIDFILE */

void *server(void *);
void usage(void);
void version(void);

static pthread_attr_t attr;
static char cvsync_confname[PATH_MAX + CVSYNC_NAME_MAX + 1];
static char cvsync_logname[PATH_MAX + CVSYNC_NAME_MAX + 1];

int
main(int argc, char *argv[])
{
	const char *base_cname = NULL, *base_pname = NULL, *base_lname = NULL;
	const char *workdir = NULL, *uid = NULL, *gid = NULL;
	struct config *cf;
	struct server_args *sa;
	struct stat st;
	char tmpname[PATH_MAX + CVSYNC_NAME_MAX + 1];
	time_t init_tic = time(NULL);
	int *socks, sock, level = CVSYNC_COMPRESS_LEVEL_UNSPEC, flags, ch, wn;
	int status = EXIT_SUCCESS;
	bool log_flag = false, background = true;

	while ((ch = getopt(argc, argv, "Vc:fg:hl:p:qu:vw:z:")) != -1) {
		switch (ch) {
		case 'V':
			version();
			/* NOTREACHED */
		case 'c':
			if (base_cname != NULL) {
				usage();
				/* NOTREACHED */
			}
			base_cname = optarg;
			break;
		case 'f':
			if (!background) {
				usage();
				/* NOTREACHED */
			}
			background = false;
			break;
		case 'g':
			if (gid != NULL) {
				usage();
				/* NOTREACHED */
			}
			gid = optarg;
			break;
		case 'h':
			usage();
			/* NOTREACHED */
		case 'l':
			if (base_lname != NULL) {
				usage();
				/* NOTREACHED */
			}
			base_lname = optarg;
			break;
		case 'p':
			if (base_pname != NULL) {
				usage();
				/* NOTREACHED */
			}
			base_pname = optarg;
			break;
		case 'q':
			if (log_flag) {
				usage();
				/* NOTREACHED */
			}
			logmsg_quiet = true;
			log_flag = true;
			break;
		case 'u':
			if (uid != NULL) {
				usage();
				/* NOTREACHED */
			}
			uid = optarg;
			break;
		case 'v':
			if (log_flag) {
				usage();
				/* NOTREACHED */
			}
			logmsg_detail = true;
			log_flag = true;
			break;
		case 'w':
			if (workdir != NULL) {
				usage();
				/* NOTREACHED */
			}
			workdir = optarg;
			break;
		case 'z':
			if (level != CVSYNC_COMPRESS_LEVEL_UNSPEC) {
				usage();
				/* NOTREACHED */
			}
			if ((strlen(optarg) != 1) || !isdigit((int)*optarg)) {
				usage();
				/* NOTREACHED */
			}
			level = *optarg - '0';
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 0) {
		usage();
		/* NOTREACHED */
	}

	if (base_cname == NULL) {
		wn = snprintf(tmpname, sizeof(tmpname), "%s",
			      CVSYNCD_DEFAULT_CONFIG);
	} else {
		if ((base_cname[0] == '/') || (workdir == NULL)) {
			wn = snprintf(tmpname, sizeof(tmpname), "%s",
				      base_cname);
		} else {
			wn = snprintf(tmpname, sizeof(tmpname), "%s/%s",
				      workdir, base_cname);
		}
	}
	if ((wn <= 0) || ((size_t)wn >= sizeof(tmpname))) {
		logmsg_err("-c <file>: %s", strerror(EINVAL));
		exit(EXIT_FAILURE);
	}
	if (realpath(tmpname, cvsync_confname) == NULL) {
		logmsg_err("%s: %s", tmpname, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if ((cf = config_load(cvsync_confname)) == NULL)
		exit(EXIT_FAILURE);

	if (strlen(cf->cf_halt_name) == 0) {
		if (workdir == NULL) {
			wn = snprintf(cf->cf_halt_name,
				      sizeof(cf->cf_halt_name), "%s",
				      CVSYNCD_DEFAULT_HALTFILE);
		} else {
			wn = snprintf(cf->cf_halt_name,
				      sizeof(cf->cf_halt_name),
				      "%s/cvsyncd.HALT", workdir);
		}
		if ((wn <= 0) || ((size_t)wn >= sizeof(cf->cf_halt_name))) {
			logmsg_err("%s/cvsyncd.HALT: %s", workdir,
				   strerror(EINVAL));
			exit(EXIT_FAILURE);
		}
	}
	if (strlen(cf->cf_pid_name) == 0) {
		if (base_pname == NULL) {
			wn = snprintf(cf->cf_pid_name, sizeof(cf->cf_pid_name),
				      "%s", CVSYNCD_DEFAULT_PIDFILE);
		} else {
			if ((base_pname[0] == '/') || (workdir == NULL)) {
				wn = snprintf(cf->cf_pid_name,
					      sizeof(cf->cf_pid_name), "%s",
					      base_pname);
			} else {
				wn = snprintf(cf->cf_pid_name,
					      sizeof(cf->cf_pid_name), "%s/%s",
					      workdir, base_pname);
			}
		}
		if ((wn <= 0) || ((size_t)wn >= sizeof(cf->cf_pid_name))) {
			logmsg_err("-p <file>: %s", strerror(EINVAL));
			exit(EXIT_FAILURE);
		}
	}

	if (!background) {
		if (!logmsg_open(NULL, false))
			exit(EXIT_FAILURE);
		if (!privilege_drop(uid, gid)) {
			logmsg_close();
			exit(EXIT_FAILURE);
		}
		if (pid_create(cf->cf_pid_name) == NULL) {
			logmsg_close();
			exit(EXIT_FAILURE);
		}
	} else {
		if (base_lname == NULL) {
			if (!logmsg_open(NULL, true))
				exit(EXIT_FAILURE);
		} else {
			if ((base_lname[0] == '/') || (workdir == NULL)) {
				wn = snprintf(tmpname, sizeof(tmpname), "%s",
					      base_lname);
			} else {
				wn = snprintf(tmpname, sizeof(tmpname),
					      "%s/%s", workdir, base_lname);
			}
			if ((wn <= 0) || ((size_t)wn >= sizeof(tmpname))) {
				logmsg_err("-l <file>: %s", strerror(EINVAL));
				exit(EXIT_FAILURE);
			}
			wn = snprintf(cvsync_logname, sizeof(cvsync_logname),
				      "%s", tmpname);
			if ((wn <= 0) ||
			    ((size_t)wn >= sizeof(cvsync_logname))) {
				logmsg_err("-l <file>: %s", strerror(EINVAL));
				exit(EXIT_FAILURE);
			}
			if (!logmsg_open(cvsync_logname, false))
				exit(EXIT_FAILURE);
		}
		if (!daemonize(cf->cf_pid_name, uid, gid)) {
			logmsg_close();
			exit(EXIT_FAILURE);
		}
	}

	logmsg("Starting cvsync server");

	if (level == CVSYNC_COMPRESS_LEVEL_UNSPEC)
		cf->cf_compress_level = CVSYNC_COMPRESS_LEVEL_SPEED;
	else
		cf->cf_compress_level = level;
	if (cf->cf_compress_level == CVSYNC_COMPRESS_LEVEL_NO)
		cf->cf_compress = CVSYNC_COMPRESS_NO;
	else
		cf->cf_compress = CVSYNC_COMPRESS_ZLIB;

	if (!cvsync_init()) {
		pid_remove();
		config_destroy(cf);
		logmsg_close();
		exit(EXIT_FAILURE);
	}

	if (pthread_attr_init(&attr) != 0) {
		pid_remove();
		config_destroy(cf);
		logmsg_close();
		exit(EXIT_FAILURE);
	}
	if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE) != 0) {
		pthread_attr_destroy(&attr);
		pid_remove();
		config_destroy(cf);
		logmsg_close();
		exit(EXIT_FAILURE);
	}

	if (!access_init(cf->cf_maxclients)) {
		pthread_attr_destroy(&attr);
		pid_remove();
		config_destroy(cf);
		logmsg_close();
		exit(EXIT_FAILURE);
	}

	socks = sock_listen((strlen(cf->cf_addr) == 0 ? NULL : cf->cf_addr),
			    cf->cf_serv);
	if (socks == NULL) {
		pthread_attr_destroy(&attr);
		access_destroy();
		pid_remove();
		config_destroy(cf);
		logmsg_close();
		exit(EXIT_FAILURE);
	}

	if (!sock_init(socks)) {
		pthread_attr_destroy(&attr);
		access_destroy();
		pid_remove();
		config_destroy(cf);
		sock_listen_stop(socks);
		logmsg_close();
		exit(EXIT_FAILURE);
	}

	for (;;) {
		if (cvsync_isinterrupted())
			break;

		if ((stat(cf->cf_halt_name, &st) == 0) &&
		    (init_tic < st.st_mtime)) {
			break;
		}

		if (!sock_select())
			continue;

		if (cf == NULL)
			break;

		if (config_ischanged(cf)) {
			struct config *new_cf;

			if ((new_cf = config_load(cvsync_confname)) != NULL) {
				config_revoke(cf);
				cf = new_cf;
				logmsg_verbose("configuration: reloaded");
			} else {
				logmsg_err("configuration: failed to reload");
			}
		}

		if ((sock = sock_accept()) == -1)
			continue;
		if ((flags = fcntl(sock, F_GETFL)) < 0) {
			sock_close(sock);
			continue;
		}
		if (fcntl(sock, F_SETFL, flags & ~O_NONBLOCK) == -1) {
			sock_close(sock);
			continue;
		}

		if ((sa = access_authorize(sock, cf)) == NULL) {
			sock_close(sock);
			continue;
		}

		if (pthread_create(&sa->sa_thread, NULL, server, sa) != 0) {
			access_done(sa);
			continue;
		}
	}

	sock_destroy();
	sock_listen_stop(socks);

	if (!pid_remove())
		status = EXIT_FAILURE;

	access_destroy();
	config_destroy(cf);

	logmsg("Stop cvsync server");

	logmsg_close();

	if (pthread_attr_destroy(&attr) != 0)
		exit(EXIT_FAILURE);

	exit(status);
	/* NOTREACHED */
}

void *
server(void *arg)
{
	struct server_args *sa = arg;
	struct dircmp_args *dca;
	struct filecmp_args *fca;
	struct collection *cls;
	struct mux *mx;
	uint32_t proto;
	void *status;
	int sock = sa->sa_socket, hash;

	if (pthread_detach(pthread_self()) != 0) {
		access_done(sa);
		return (CVSYNC_THREAD_FAILURE);
	}

	if (!protocol_exchange(sock, sa->sa_status, sa->sa_error, &proto)) {
		access_done(sa);
		return (CVSYNC_THREAD_FAILURE);
	}
	if (sa->sa_status == ACL_DENY) {
		access_done(sa);
		return (CVSYNC_THREAD_SUCCESS);
	}

	if ((hash = hash_exchange(sock, sa->sa_config)) == HASH_UNSPEC) {
		access_done(sa);
		return (CVSYNC_THREAD_FAILURE);
	}
	if ((cls = collectionlist_exchange(sock, sa->sa_config)) == NULL) {
		access_done(sa);
		return (CVSYNC_THREAD_FAILURE);
	}
	if ((mx = channel_establish(sock, sa->sa_config, proto)) == NULL) {
		collection_destroy_all(cls);
		access_done(sa);
		return (CVSYNC_THREAD_FAILURE);
	}
	if ((dca = dircmp_init(mx, sa->sa_hostinfo, cls, proto)) == NULL) {
		mux_destroy(mx);
		collection_destroy_all(cls);
		access_done(sa);
		return (CVSYNC_THREAD_FAILURE);
	}
	if ((fca = filecmp_init(mx, sa->sa_config->cf_collections, cls,
				sa->sa_hostinfo, proto, hash)) == NULL) {
		dircmp_destroy(dca);
		mux_destroy(mx);
		collection_destroy_all(cls);
		access_done(sa);
		return (CVSYNC_THREAD_FAILURE);
	}

	if (pthread_create(&mx->mx_receiver, &attr, receiver, mx) != 0)
		mux_abort(mx);
	if (pthread_create(&dca->dca_thread, &attr, dircmp, dca) != 0)
		mux_abort(mx);
	if (pthread_create(&fca->fca_thread, &attr, filecmp, fca) != 0)
		mux_abort(mx);

	if (pthread_join(fca->fca_thread, &fca->fca_status) != 0)
		mux_abort(mx);
	if (pthread_join(dca->dca_thread, &dca->dca_status) != 0)
		mux_abort(mx);
	if (pthread_join(mx->mx_receiver, &status) != 0)
		mux_abort(mx);
	if ((dca->dca_status == CVSYNC_THREAD_FAILURE) ||
	    (fca->fca_status == CVSYNC_THREAD_FAILURE) ||
	    (status == CVSYNC_THREAD_FAILURE)) {
		logmsg_verbose("%s Failed", sa->sa_hostinfo);
	} else {
		logmsg_verbose("%s Finished successfully", sa->sa_hostinfo);
	}

	filecmp_destroy(fca);
	dircmp_destroy(dca);

	collection_destroy_all(cls);

	logmsg("%s in=%" PRIu64 ", out=%" PRIu64 ", time=%ds", sa->sa_hostinfo,
	       mx->mx_xfer_in, mx->mx_xfer_out, time(NULL) - sa->sa_tick);

	mux_destroy(mx);
	access_done(sa);

	return (CVSYNC_THREAD_SUCCESS);
}

void
usage(void)
{
	logmsg_err("Usage: cvsyncd [-Vfhqv] [-c <file>] [-g <group>] "
		   "[-l <file>] [-p <file>] [-u <user>] [-w <directory>] "
		   "[-z <level>]");
	logmsg_err("\tThe version: %u.%u.%u", CVSYNC_MAJOR, CVSYNC_MINOR,
		   CVSYNC_PATCHLEVEL);
	logmsg_err("\tThe protocol version: %u.%u", CVSYNC_PROTO_MAJOR,
		   CVSYNC_PROTO_MINOR);
	logmsg_err("\tThe default configuration file: %s",
		   CVSYNCD_DEFAULT_CONFIG);
	logmsg_err("\tURL: %s", CVSYNC_URL);
	exit(EXIT_FAILURE);
}

void
version(void)
{
	logmsg_err("cvsyncd version %u.%u.%u (protocol %u.%u)",
		   CVSYNC_MAJOR, CVSYNC_MINOR, CVSYNC_PATCHLEVEL,
		   CVSYNC_PROTO_MAJOR, CVSYNC_PROTO_MINOR);
	exit(EXIT_FAILURE);
}
