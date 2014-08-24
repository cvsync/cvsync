#
# Copyright (c) 2000-2012 MAEKAWA Masahide <maekawa@cvsync.org>
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

ifeq (${HOST_OS}, NetBSD)
ifeq ($(shell ${TEST} ${OSVER} -lt 106130000 && ${ECHO} yes), yes) # 1.6M
PTHREAD_TYPE   ?= none
endif # 1.6M
endif # NetBSD

PTHREAD_TYPE   ?= native

_PTHREAD_CFLAGS	= -D_REENTRANT -D_THREAD_SAFE

ifeq (${PTHREAD_TYPE}, native) # native POSIX thread

ifeq (${HOST_OS}, AIX)
_PTHREAD_LIBS	= -lpthreads
endif # AIX

ifeq (${HOST_OS}, CYGWIN)
_PTHREAD_LIBS	= -lpthread
endif # CYGWIN

ifeq (${HOST_OS}, DragonFly)
ifeq ($(shell ${TEST} ${OSVER} -lt 200000 && ${ECHO} yes), yes) # 2.0-RELEASE
_PTHREAD_LIBS	= -lc_r
else # 2.0-RELEASE
_PTHREAD_CFLAGS+= -pthread
_PTHREAD_LDFLAGS= -pthread
endif # 2.0-RELEASE
endif # DragonFly

ifeq (${HOST_OS}, FreeBSD)
ifeq ($(shell ${TEST} ${OSVER} -lt 502102 && ${ECHO} yes), yes)
ifeq ($(shell ${TEST} ${OSVER} -lt 500016 && ${ECHO} yes), yes)
ifeq (${CC_TYPE}, default)
_PTHREAD_LDFLAGS= -pthread
endif # default
ifeq (${CC_TYPE}, gnu)
_PTHREAD_LDFLAGS= -pthread
endif # gnu
ifeq (${CC_TYPE}, tendra)
_PTHREAD_LDFLAGS= -b
_PTHREAD_LIBS	= -lc_r
endif # tendra
else # 500016
_PTHREAD_LIBS	= -lc_r
endif # 500016
else # 502102
_PTHREAD_LIBS	= -lpthread
endif # 502102
endif # FreeBSD

ifeq (${HOST_OS}, Linux)
_PTHREAD_LIBS	= -lpthread
endif # Linux

ifeq (${HOST_OS}, NetBSD)
_PTHREAD_LIBS	= -lpthread
endif # NetBSD

ifeq (${HOST_OS}, OpenBSD)
_PTHREAD_CFLAGS+= -pthread
_PTHREAD_LDFLAGS= -pthread
endif # OpenBSD

ifeq (${HOST_OS}, SunOS)
_PTHREAD_LIBS	= -lpthread -lrt
endif # SunOS

PTHREAD_CFLAGS ?= ${_PTHREAD_CFLAGS}
PTHREAD_LDFLAGS?= ${_PTHREAD_LDFLAGS}
PTHREAD_LIBS   ?= ${_PTHREAD_LIBS}

endif # native POSIX thread

PTHREAD_PREFIX ?= ${PREFIX}

ifeq (${PTHREAD_TYPE}, pth) # GNU Pth
PTHREAD_CFLAGS := $(shell ${PTHREAD_PREFIX}/bin/pthread-config --cflags)
PTHREAD_LDFLAGS:= $(shell ${PTHREAD_PREFIX}/bin/pthread-config --ldflags)
PTHREAD_LIBS   := $(shell ${PTHREAD_PREFIX}/bin/pthread-config --libs)

PTHREAD_CFLAGS += ${_PTHREAD_CFLAGS}
endif # GNU Pth

ifeq (${PTHREAD_TYPE}, ptl) # PTL
CC	= ${PTHREAD_PREFIX}/bin/ptlgcc
PTHREAD_CFLAGS	= -I${PTHREAD_PREFIX}/PTL/include
PTHREAD_LDFLAGS	= -L${PTHREAD_PREFIX}/lib
PTHREAD_LIBS	= -lPTL

PTHREAD_CFLAGS += ${_PTHREAD_CFLAGS}
endif # PTL

ifeq (${PTHREAD_TYPE}, unproven) # unproven pthreads
CC	= ${PTHREAD_PREFIX}/pthreads/bin/pgcc
PTHREAD_CFLAGS	= -I${PTHREAD_PREFIX}/pthreads/include
PTHREAD_LDFLAGS	= -L${PTHREAD_PREFIX}/pthreads/lib
PTHREAD_LIBS	= -lpthread

PTHREAD_CFLAGS += ${_PTHREAD_CFLAGS}
endif # unproven pthreads

ifneq (${PTHREAD_TYPE}, native) # !native
PTHREAD_CFLAGS += -DENABLE_SOCK_WAIT=1
endif # !native

CFLAGS += ${PTHREAD_CFLAGS}
LDFLAGS+= ${PTHREAD_LDFLAGS}
LIBS   += ${PTHREAD_LIBS}

ifeq (${PTHREAD_TYPE}, none) # no POSIX threads support
pthread-error:
	@${ECHO} "WARNING! Please specify the PTHREAD_TYPE."
	@${ECHO} "The following types are available:"
	@${ECHO} "  native (default)"
	@${ECHO} "  pth"
	@${ECHO} "  ptl"
	@${ECHO} "  unproven"
	@exit 1;
endif # none
