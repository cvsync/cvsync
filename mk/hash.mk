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

ifeq (${HOST_OS}, AIX)
HASH_TYPE      ?= none
endif # AIX

ifeq (${HOST_OS}, BSD/OS)
HASH_TYPE      ?= none
endif # BSD/OS

ifeq (${HOST_OS}, CYGWIN)
HASH_TYPE      ?= none
endif # CYGWIN

ifeq (${HOST_OS}, Darwin)
HASH_TYPE      ?= none
endif # Darwin

ifeq (${HOST_OS}, Linux)
HASH_TYPE      ?= none
endif # Linux

ifeq (${HOST_OS}, SunOS)
ifeq (${OSVER}, 5.7)
HASH_TYPE      ?= none
endif # 5.7
endif # SunOS

HASH_TYPE      ?= native

ifeq (${HASH_TYPE}, native)

ifeq (${HOST_OS}, DragonFly)
LIBS   += -lmd
endif # DragonFly

ifeq (${HOST_OS}, FreeBSD)
ifeq ($(shell ${TEST} ${OSVER} -ge 400002 && ${ECHO} yes), yes) # 4.1-RELEASE
HAVE_RIPEMD160	= yes
endif # 4.1-RELEASE

ifeq ($(shell ${TEST} ${OSVER} -lt 400000 && ${ECHO} yes), yes) # 4.0-RELEASE
HAVE_SHA1	= no
endif # 4.0-RELEASE
LIBS   += -lmd
endif # FreeBSD

ifeq (${HOST_OS}, Interix)
HAVE_RIPEMD160	= yes
LIBS   += -lcrypt
endif # Interix

ifeq (${HOST_OS}, NetBSD)
ifeq ($(shell ${TEST} ${OSVER} -ge 105030000 && ${ECHO} yes), yes) # 1.5C
HAVE_RIPEMD160	= yes
endif # 1.5C

ifeq ($(shell ${TEST} ${OSVER} -lt 106010000 && ${ECHO} yes), yes) # 1.6A
HAVE_SHA1	= no
endif # 1.6A
endif # NetBSD

ifeq (${HOST_OS}, OpenBSD)
ifeq ($(shell ${TEST} ${OSVER} -ge 199711 && ${ECHO} yes), yes) # 2.2
HAVE_RIPEMD160 ?= yes
endif # 2.2

ifeq ($(shell ${TEST} ${OSVER} -lt 200311 && ${ECHO} yes), yes) # 3.4
HAVE_SHA1      ?= no
endif # 3.4
endif # OpenBSD

ifeq (${HOST_OS}, SunOS)
HAVE_SHA1	= no
LIBS   += -lmd5
endif # SunOS

SRCS   += hash_native.c
endif # native

ifeq (${HASH_TYPE}, libgcrypt) # libgcrypt
HASH_SRCS	= hash_libgcrypt.c
HASH_LIBS      ?= -lgcrypt

HAVE_RIPEMD160	= yes
HAVE_TIGER192	= yes
endif # libgcrypt

ifeq (${HASH_TYPE}, mhash) # mhash
HASH_SRCS	= hash_mhash.c
HASH_LIBS      ?= -lmhash

HAVE_RIPEMD160	= yes
HAVE_TIGER192	= yes
endif # mhash

ifeq (${HASH_TYPE}, openssl) # OpenSSL
HASH_SRCS	= hash_openssl.c
HASH_LIBS      ?= -lcrypto

HAVE_RIPEMD160	= yes

ifeq (${HOST_OS}, BSD/OS)
HASH_PREFIX    ?= /usr/contrib
endif # BSD/OS

ifeq (${HOST_OS}, SunOS)
HASH_PREFIX    ?= ${PREFIX}/ssl
endif # SunOS
endif # OpenSSL

ifneq (${HASH_TYPE}, native)
HASH_PREFIX    ?= ${PREFIX}
HASH_CFLAGS    ?= -I${HASH_PREFIX}/include
HASH_LDFLAGS   ?= -L${HASH_PREFIX}/lib

ifeq (${HOST_OS}, SunOS)
HASH_LDFLAGS   += -R${HASH_PREFIX}/lib
endif # SunOS

SRCS   += ${HASH_SRCS}
CFLAGS += ${HASH_CFLAGS}
LDFLAGS+= ${HASH_LDFLAGS}
LIBS   += ${HASH_LIBS}
endif # !native

HAVE_RIPEMD160 ?= no
HAVE_SHA1      ?= yes
HAVE_TIGER192  ?= no

ifeq (${HAVE_RIPEMD160}, yes)
CFLAGS += -DHAVE_RIPEMD160
endif # HAVE_RIPEMD160

ifeq (${HAVE_SHA1}, yes)
CFLAGS += -DHAVE_SHA1
endif # HAVE_SHA1

ifeq (${HAVE_TIGER192}, yes)
CFLAGS += -DHAVE_TIGER192
endif # HAVE_TIGER192

ifeq (${HASH_TYPE}, none) # no hash support
hash-error:
	@${ECHO} "WARNING! Please specify the HASH_TYPE."
	@${ECHO} "  libgcrypt"
	@${ECHO} "  mhash"
	@${ECHO} "  native (default)"
	@${ECHO} "  openssl"
	@exit 1;
endif # none
