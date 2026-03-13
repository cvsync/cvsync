#
# This software is released under the BSD License, see LICENSE.
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
