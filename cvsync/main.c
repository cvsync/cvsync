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
#include <sys/time.h>

#include <stdlib.h>

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "compat_stdbool.h"
#include "compat_stdint.h"
#include "compat_inttypes.h"
#include "compat_limits.h"

#include "collection.h"
#include "cvsync.h"
#include "cvsync_attr.h"
#include "hash.h"
#include "logmsg.h"
#include "mdirent.h"
#include "mux.h"
#include "network.h"
#include "pid.h"
#include "scanfile.h"
#include "version.h"

#include "receiver.h"
#include "dirscan.h"
#include "filescan.h"
#include "updater.h"

#include "defs.h"

bool client(struct config *);
void usage(void);
void version(void);

static pthread_attr_t attr;

int
main(int argc, char *argv[])
{
	struct collection *cl;
	struct config *cfs = NULL, *cf, *prev;
	struct timeval tic, toc;
	const char *confname = NULL, *pidfile = NULL;
	int af = AF_UNSPEC, compression = CVSYNC_COMPRESS_UNSPEC;
	int status = EXIT_SUCCESS, ch, i;
	bool loose = false, log_flag = false;

	network_init(argv[0]);

	while ((ch = getopt(argc, argv, "46LVZc:hp:qvz")) != -1) {
		switch (ch) {
		case '4':
			if (af != AF_UNSPEC) {
				usage();
				/* NOTREACHED */
			}
			af = AF_INET;
			break;
		case '6':
#if defined(AF_INET6)
			if (af == AF_UNSPEC) {
				af = AF_INET6;
				break;
			}
#else /* defined(AF_INET6) */
			logmsg_err("%s", strerror(EAFNOSUPPORT));
#endif /* defined(AF_INET6) */
			usage();
			/* NOTREACHED */
		case 'L':
			if (loose) {
				usage();
				/* NOTREACHED */
			}
			loose = true;
			break;
		case 'V':
			version();
			/* NOTREACHED */
		case 'Z':
			if (compression != CVSYNC_COMPRESS_UNSPEC) {
				usage();
				/* NOTREACHED */
			}
			compression = CVSYNC_COMPRESS_NO;
			break;
		case 'c':
			if (confname != NULL) {
				usage();
				/* NOTREACHED */
			}
			confname = optarg;
			break;
		case 'h':
			usage();
			/* NOTREACHED */
		case 'p':
			if (pidfile != NULL) {
				usage();
				/* NOTREACHED */
			}
			pidfile = optarg;
			break;
		case 'q':
			if (log_flag) {
				usage();
				/* NOTREACHED */
			}
			logmsg_quiet = true;
			log_flag = true;
			break;
		case 'v':
			if (log_flag) {
				usage();
				/* NOTREACHED */
			}
			logmsg_detail = true;
			log_flag = true;
			break;
		case 'z':
			if (compression != CVSYNC_COMPRESS_UNSPEC) {
				usage();
				/* NOTREACHED */
			}
			compression = CVSYNC_COMPRESS_ZLIB;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (pid_create(pidfile) == NULL)
		exit(EXIT_FAILURE);

	if (!cvsync_init()) {
		pid_remove();
		exit(EXIT_FAILURE);
	}

	if (pthread_attr_init(&attr) != 0) {
		pid_remove();
		exit(EXIT_FAILURE);
	}
	if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE) != 0) {
		pthread_attr_destroy(&attr);
		pid_remove();
		exit(EXIT_FAILURE);
	}

	if ((argc == 0) && (confname == NULL))
		confname = CVSYNC_DEFAULT_CONFIG;
	if (confname != NULL) {
		if ((cfs = config_load_file(confname)) == NULL) {
			pthread_attr_destroy(&attr);
			pid_remove();
			exit(EXIT_FAILURE);
		}
	}
	for (i = 0 ; i < argc ; i++) {
		if ((cf = config_load_uri(argv[i])) == NULL) {
			config_destroy(cfs);
			pthread_attr_destroy(&attr);
			pid_remove();
			exit(EXIT_FAILURE);
		}
		if ((prev = cfs) == NULL) {
			cfs = cf;
		} else {
			while (prev->cf_next != NULL)
				prev = prev->cf_next;
			prev->cf_next = cf;
		}
	}
	if (cfs == NULL) {
		logmsg_err("No configurations");
		pthread_attr_destroy(&attr);
		pid_remove();
		exit(EXIT_FAILURE);
	}

	for (cf = cfs ; cf != NULL ; cf = cf->cf_next) {
		gettimeofday(&tic, NULL);

		if (af != AF_UNSPEC)
			cf->cf_family = af;
		if (compression != CVSYNC_COMPRESS_UNSPEC)
			cf->cf_compress = compression;
		if (loose) {
			for (cl = cf->cf_collections ;
			     cl != NULL ;
			     cl = cl->cl_next) {
				cl->cl_errormode = CVSYNC_ERRORMODE_FIXUP;
			}
		}
		if (!client(cf)) {
			status = EXIT_FAILURE;
			break;
		}

		gettimeofday(&toc, NULL);

		toc.tv_sec -= tic.tv_sec;
		toc.tv_usec -= tic.tv_usec;
		if (toc.tv_usec < 0) {
			toc.tv_sec--;
			toc.tv_usec += 1000000;
		}
		toc.tv_usec /= 1000;

		logmsg_verbose("Total time: %ld.%03d sec", toc.tv_sec,
			       toc.tv_usec);
	}

	config_destroy(cfs);

	if (pthread_attr_destroy(&attr) != 0) {
		pid_remove();
		exit(EXIT_FAILURE);
	}

	if (!pid_remove())
		exit(EXIT_FAILURE);

	exit(status);
	/* NOTREACHED */
}

bool
client(struct config *cf)
{
	struct dirscan_args *dsa;
	struct filescan_args *fsa;
	struct updater_args *uda;
	struct mux *mx;
	void *status;
	int sock;
	bool rv;

	if ((sock = sock_connect(cf->cf_family, cf->cf_host,
				 cf->cf_serv)) < 0) {
		logmsg_err("service is not available at %s port %s",
			   cf->cf_host, cf->cf_serv);
		return (false);
	}

	if (!protocol_exchange(sock, cf)) {
		sock_close(sock);
		return (false);
	}
	if (!hash_exchange(sock, cf)) {
		logmsg_err("Hash Exchange Error");
		sock_close(sock);
		return (false);
	}
	if (!collectionlist_exchange(sock, cf)) {
		logmsg_err("CollectionList Exchange Error");
		sock_close(sock);
		return (false);
	}
	if (!compress_exchange(sock, cf)) {
		logmsg_err("Compression Negotiation Error");
		sock_close(sock);
		return (false);
	}
	if ((mx = channel_establish(sock, cf)) == NULL) {
		logmsg_err("Channel Initialization Error");
		sock_close(sock);
		return (false);
	}

	if (cvsync_isinterrupted()) {
		mux_destroy(mx);
		sock_close(sock);
		return (false);
	}

	logmsg("Running...");

	if ((dsa = dirscan_init(mx, cf->cf_collections,
				cf->cf_proto)) == NULL) {
		mux_destroy(mx);
		sock_close(sock);
		return (false);
	}
	if ((fsa = filescan_init(mx, cf->cf_collections, cf->cf_proto,
				 cf->cf_hash)) == NULL) {
		dirscan_destroy(dsa);
		mux_destroy(mx);
		sock_close(sock);
		return (false);
	}
	if ((uda = updater_init(mx, cf->cf_collections, cf->cf_proto,
				cf->cf_hash)) == NULL) {
		dirscan_destroy(dsa);
		filescan_destroy(fsa);
		mux_destroy(mx);
		sock_close(sock);
		return (false);
	}

	if (pthread_create(&mx->mx_receiver, &attr, receiver, mx) != 0)
		mux_abort(mx);
	if (pthread_create(&dsa->dsa_thread, &attr, dirscan, dsa) != 0)
		mux_abort(mx);
	if (pthread_create(&fsa->fsa_thread, &attr, filescan, fsa) != 0)
		mux_abort(mx);
	if (pthread_create(&uda->uda_thread, &attr, updater, uda) != 0)
		mux_abort(mx);

	if (pthread_join(uda->uda_thread, &uda->uda_status) != 0)
		mux_abort(mx);
	if (pthread_join(fsa->fsa_thread, &fsa->fsa_status) != 0)
		mux_abort(mx);
	if (pthread_join(dsa->dsa_thread, &dsa->dsa_status) != 0)
		mux_abort(mx);
	if (pthread_join(mx->mx_receiver, &status) != 0)
		mux_abort(mx);
	if ((dsa->dsa_status == CVSYNC_THREAD_FAILURE) ||
	    (fsa->fsa_status == CVSYNC_THREAD_FAILURE) ||
	    (uda->uda_status == CVSYNC_THREAD_FAILURE) ||
	    (status == CVSYNC_THREAD_FAILURE)) {
		logmsg("Failed");
		rv = false;
	} else {
		logmsg("Finished successfully");
		rv = true;
	}

	dirscan_destroy(dsa);
	filescan_destroy(fsa);
	updater_destroy(uda);

	mux_destroy(mx);

	sock_close(sock);

	return (rv);
}

void
usage(void)
{
	logmsg_err("Usage: cvsync [-46LVZhqvz] [-c <file>] [-p <file>]");
	logmsg_err("\tThe version: %u.%u.%u", CVSYNC_MAJOR, CVSYNC_MINOR,
		   CVSYNC_PATCHLEVEL);
	logmsg_err("\tThe protocol version: %u.%u", CVSYNC_PROTO_MAJOR,
		   CVSYNC_PROTO_MINOR);
	logmsg_err("\tThe default configuration file: %s",
		   CVSYNC_DEFAULT_CONFIG);
	logmsg_err("\tURL: %s", CVSYNC_URL);
	exit(EXIT_FAILURE);
}

void
version(void)
{
	logmsg_err("cvsync version %u.%u.%u (protocol %u.%u)", CVSYNC_MAJOR,
		   CVSYNC_MINOR, CVSYNC_PATCHLEVEL, CVSYNC_PROTO_MAJOR,
		   CVSYNC_PROTO_MINOR);
	exit(EXIT_FAILURE);
}
