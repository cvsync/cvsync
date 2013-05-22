#
# Copyright (c) 2000-2013 MAEKAWA Masahide <maekawa@cvsync.org>
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

VPATH	= ../common

OBJS	= $(SRCS:.c=.o)

CFLAGS += ${CFLAGS_OPTS} -I../common -I. -g
LDFLAGS+= ${LDFLAGS_OPTS}

CFG_MKFILE	= ../mk/defaults.mk
CFG_PARAMS     += CC_TYPE CFLAGS_OPTS LDFLAGS_OPTS
CFG_PARAMS     += PREFIX ZLIB_PREFIX USE_INET6 USE_POLL
CFG_PARAMS     += HASH_TYPE HASH_PREFIX
CFG_PARAMS     += PTHREAD_TYPE PTHREAD_PREFIX
CFG_PARAMS     += SOCKS5_TYPE SOCKS5_PREFIX

all: configure_show hash-error pthread-error ${PROG}

clean:
	$(RM) ${PROG} *.o

cleandir: distclean

configure: configure_generate configure_show

configure_generate:
	$(RM) ${CFG_MKFILE}
	@$(foreach V,${CFG_PARAMS},${ECHO} "${V}=" ${${V}} >> ${CFG_MKFILE};)

configure_show:
	@${ECHO} "*** Build parameters:"
	@$(foreach V,${CFG_PARAMS},${ECHO} "  ${V}=" ${${V}};)

depend: ${SRCS}
	if [ -x ${MKDEP} ]; then ${MKDEP} ${MKDEP_OPT} ${CFLAGS} $^ ; fi

distclean: clean
	$(RM) ${MKDEP_FILE} ../mk/defaults.mk *.core *.d *.stackdump

install: all
	${INSTALL} ${INSTALL_BIN_OPTS} ${PROG} ${BINDIR}/${PROG}
	${INSTALL} ${INSTALL_MAN_OPTS} ${MAN} ${MANDIR}/man1/${MAN}

uninstall:
	$(RM) ${BINDIR}/${PROG} ${MANDIR}/man1/${MAN}

${PROG}: ${OBJS}
	$(CC) ${LDFLAGS} -o ${PROG} ${OBJS} ${LIBS}

ifeq ($(shell ${TEST} -f ./${MKDEP_FILE} && ${ECHO} yes), yes)
include ./${MKDEP_FILE}
endif

HASH_TYPE      ?= unused
ifneq (${HASH_TYPE}, none)
hash-error:
endif

PTHREAD_TYPE   ?= unused
ifneq (${PTHREAD_TYPE}, none)
pthread-error:
endif
