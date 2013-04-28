#
# Copyright (c) 2000-2005 MAEKAWA Masahide <maekawa@cvsync.org>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the author nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

PROG	= cvsync
SRCS	= attribute_rcs.c config_common.c cvsync.c cvsync_rcs.c distfile.c \
	  hash.c list.c logmsg.c mdirent.c mdirent_rcs.c mux.c mux_raw.c \
	  mux_zlib.c network.c pid.c rcslib.c rdiff.c receiver.c \
	  receiver_raw.c receiver_zlib.c refuse.c scanfile.c scanfile_rcs.c \
	  token.c \
	  dirscan.c dirscan_rcs.c dirscan_rcs_scanfile.c \
	  filescan.c filescan_generic.c filescan_rcs.c filescan_rdiff.c \
	  updater.c updater_generic.c updater_list.c updater_rcs.c \
	  updater_rcs_scanfile.c updater_rdiff.c \
	  collection.c config.c intr.c proto.c main.c

ifdef CVSYNC_DEFAULT_CONFIG
CFLAGS += -DCVSYNC_DEFAULT_CONFIG="\"${CVSYNC_DEFAULT_CONFIG}\""
else
CFLAGS += -DCVSYNC_DEFAULT_CONFIG="\"${PREFIX}/etc/cvsync.conf\""
endif # defined(CVSYNC_DEFAULT_CONFIG)

include ../mk/base.mk
include ../mk/compress.mk
include ../mk/hash.mk
include ../mk/network.mk
include ../mk/pthread.mk
include ../mk/prog.mk
