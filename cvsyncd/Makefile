#
# This software is released under the BSD License, see LICENSE.
#

PROG	= cvsyncd
SRCS	= attribute_rcs.c config_common.c cvsync.c cvsync_rcs.c distfile.c \
	  hash.c list.c logmsg.c mdirent.c mdirent_rcs.c mux.c mux_raw.c \
	  mux_zlib.c network.c pid.c rcslib.c rdiff.c receiver.c \
	  receiver_raw.c receiver_zlib.c scanfile.c scanfile_rcs.c token.c \
	  dircmp.c dircmp_rcs.c dircmp_rcs_scanfile.c \
	  filecmp.c filecmp_generic.c filecmp_list.c filecmp_rcs.c \
	  filecmp_rdiff.c \
	  access.c collection.c config.c daemon.c intr.c proto.c main.c

ifdef CVSYNCD_DEFAULT_CONFIG
CFLAGS += -DCVSYNCD_DEFAULT_CONFIG="\"${CVSYNCD_DEFAULT_CONFIG}\""
else
CFLAGS += -DCVSYNCD_DEFAULT_CONFIG="\"${PREFIX}/etc/cvsyncd.conf\""
endif # defined(CVSYNCD_DEFAULT_CONFIG)

ifdef CVSYNCD_DEFAULT_HALTFILE
CFLAGS += -DCVSYNCD_DEFAULT_HALTFILE="\"${CVSYNCD_DEFAULT_HALTFILE}\""
endif # defined(CVSYNCD_DEFAULT_HALTFILE)

ifdef CVSYNCD_DEFAULT_PIDFILE
CFLAGS += -DCVSYNCD_DEFAULT_PIDFILE="\"${CVSYNCD_DEFAULT_PIDFILE}\""
endif # defined(CVSYNCD_DEFAULT_PIDFILE)

include ../mk/base.mk
include ../mk/compress.mk
include ../mk/hash.mk
include ../mk/network.mk
include ../mk/pthread.mk
include ../mk/prog.mk
