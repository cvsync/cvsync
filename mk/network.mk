#
# Copyright (c) 2003-2012 MAEKAWA Masahide <maekawa@cvsync.org>
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

ifeq (${HOST_OS}, AIX)
ifeq ($(shell ${TEST} ${OSVER} -le 4 && ${ECHO} yes), yes) # 4L
CFLAGS += -DNO_SOCKADDR_LEN -DNO_SOCKADDR_STORAGE
CFLAGS += -DNO_PROTOTYPE_FREEADDRINFO -DNO_PROTOTYPE_GAI_STRERROR
CFLAGS += -DNO_PROTOTYPE_GETADDRINFO -DNO_PROTOTYPE_GETNAMEINFO
CFLAGS += -DNO_PROTOTYPE_INET_PTON
endif # 4L
endif # AIX

ifeq (${HOST_OS}, BSD/OS)
USE_POLL       ?= no
endif # BSD/OS

ifeq (${HOST_OS}, CYGWIN)
USE_INET6      ?= no
CFLAGS += -DNO_PROTOTYPE_INET_PTON
SRCS   += inet_pton.c
endif # CYGWIN

ifeq (${HOST_OS}, Darwin)
HAVE_SOCKLEN_T ?= no
ifeq ($(shell ${TEST} ${OSVER} = "12.0.0" && ${ECHO} yes), yes) # OS X 10.8
HAVE_SOCKLEN_T	= yes
endif # OS X 10.8
ifeq ($(shell ${TEST} ${OSVER} = "12.3.0" && ${ECHO} yes), yes) # OS X 10.8.2
HAVE_SOCKLEN_T	= yes
endif # OS X 10.8.2
ifeq ($(shell ${TEST} ${OSVER} = "12.4.0" && ${ECHO} yes), yes) # OS X 10.8.4
HAVE_SOCKLEN_T	= yes
endif # OS X 10.8.4
ifeq ($(shell ${TEST} ${OSVER} = "12.5.0" && ${ECHO} yes), yes) # OS X 10.8.5
HAVE_SOCKLEN_T	= yes
endif # OS X 10.8.5
ifeq ($(shell ${TEST} ${OSVER} = "13.0.0" && ${ECHO} yes), yes) # OS X 10.9
HAVE_SOCKLEN_T	= yes
endif # OS X 10.9
ifeq (${HAVE_SOCKLEN_T}, no)
CFLAGS += -Dsocklen_t=int
endif # HAVE_SOCKLEN_T
USE_POLL       ?= no
endif # Darwin

ifeq (${HOST_OS}, FreeBSD)
ifeq ($(shell ${TEST} ${OSVER} -lt 350000 && ${ECHO} yes), yes) # 3.5-RELEASE
CFLAGS += -Dsocklen_t=int
endif # 3.5-RELEASE
ifeq ($(shell ${TEST} ${OSVER} -lt 400000 && ${ECHO} yes), yes) # 4.0-RELEASE
USE_INET6      ?= no
SRCS   += inet_pton.c
endif # 4.0-RELEASE
ifeq ($(shell ${TEST} ${OSVER} -lt 450000 && ${ECHO} yes), yes) # 4.5-RELEASE
CFLAGS += -Din_addr_t=uint32_t -Din_port_t=uint16_t
endif # 4.5-RELEASE
endif # FreeBSD

ifeq (${HOST_OS}, Interix)
USE_INET6      ?= no
CFLAGS += -DNO_PROTOTYPE_INET_PTON
CFLAGS += -Dsocklen_t=int
SRCS   += inet_pton.c
endif # Interix

ifeq (${HOST_OS}, NetBSD)
ifeq ($(shell ${TEST} ${OSVER} -lt 103100000 && ${ECHO} yes), yes) # 1.3J
CFLAGS += -Dsocklen_t=int
endif # 1.3J
ifeq ($(shell ${TEST} ${OSVER} -lt 104050000 && ${ECHO} yes), yes) # 1.4E
USE_INET6      ?= no
SRCS   += inet_pton.c
endif # 1.4E
ifeq ($(shell ${TEST} ${OSVER} -lt 104120000 && ${ECHO} yes), yes) # 1.4L
CFLAGS += -Din_addr_t=uint32_t -Din_port_t=uint16_t
endif # 1.4L
endif # NetBSD

ifeq (${HOST_OS}, OpenBSD)
ifeq ($(shell ${TEST} ${OSVER} -lt 199806 && ${ECHO} yes), yes) # 2.3
CFLAGS += -Din_addr_t=uint32_t -Din_port_t=uint16_t
endif # 2.3
ifeq ($(shell ${TEST} ${OSVER} -lt 199905 && ${ECHO} yes), yes) # 2.5
USE_INET6      ?= no
SRCS   += inet_pton.c
CFLAGS += -Dsocklen_t=int
endif # 2.5
endif # OpenBSD

ifeq (${HOST_OS}, SunOS)
LIBS   += -lsocket -lnsl
ifeq (${OSVER}, 5.7)
USE_INET6      ?= no
LIBS   += -lresolv
endif # 5.7
endif # SunOS

USE_INET6      ?= yes
USE_POLL       ?= yes

ifeq ($(patsubst NO,no,${USE_INET6}), no)
SRCS   += network_ipv4.c
else # USE_INET6
SRCS   += network_ai.c
endif # USE_INET6

ifeq ($(patsubst NO,no,${USE_POLL}), no)
SRCS   += network_select.c
else # USE_POLL
SRCS   += network_poll.c

ifeq (${HOST_OS}, CYGWIN)
CFLAGS += -Dnfds_t='unsigned int'
endif # CYGWIN

ifeq (${HOST_OS}, DragonFly)
ifeq ($(shell ${TEST} ${OSVER} -lt 197500 && ${ECHO} yes), yes) # 1.11
CFLAGS += -Dnfds_t='unsigned int'
endif # 1.11
endif # DragonFly

ifeq (${HOST_OS}, FreeBSD)
ifeq ($(shell ${TEST} ${OSVER} -lt 500000 && ${ECHO} yes), yes) # 5.0-RELEASE
CFLAGS += -Dnfds_t='unsigned int'
endif # 5.0-RELEASE
endif # FreeBSD

ifeq (${HOST_OS}, OpenBSD)
ifeq ($(shell ${TEST} ${OSVER} -le 200311 && ${ECHO} yes), yes) # 3.4
CFLAGS += -Dnfds_t=int
endif # 3.4
endif # OpenBSD
endif # USE_POLL

SOCKS5_TYPE     ?= none

ifneq (${SOCKS5_TYPE}, none)
SOCKS5_PREFIX   ?= ${PREFIX}
SOCKS5_CFLAGS   ?= -I${SOCKS5_PREFIX}/include
SOCKS5_LDFLAGS  ?= -L${SOCKS5_PREFIX}/lib

CFLAGS += -DUSE_SOCKS5
CFLAGS += ${SOCKS5_CFLAGS}
LDFLAGS+= ${SOCKS5_LDFLAGS}
endif # !none

ifeq (${SOCKS5_TYPE}, dante)
CFLAGS += -DSOCKS5_init=SOCKSinit -DSOCKS5_connect=Rconnect
CFLAGS += -DSOCKS5_gethostbyname=Rgethostbyname
LIBS   += -lsocks
endif # Dante

ifeq (${SOCKS5_TYPE}, nec)
CFLAGS += -DSOCKS5_init=SOCKSinit -DSOCKS5_connect=SOCKSconnect
CFLAGS += -DSOCKS5_gethostbyname=SOCKSgethostbyname
LIBS   += -lsocks5
endif # NEC
