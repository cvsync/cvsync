#
# This software is released under the BSD License, see LICENSE.
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
ifeq ($(shell ${TEST} ${OSVER} -lt 15 && ${ECHO} yes), yes) # OS X El Capitan (10.11)
HASH_TYPE      ?= openssl
endif # OS X El Capitan (10.11)
ifeq ($(shell ${TEST} ${OSVER} -ge 15 && ${ECHO} yes), yes) # OS X El Capitan (10.11)
HASH_TYPE      ?= native
endif # OS X El Capitan (10.11)
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
HAVE_SHA256	= no
LIBS   += -lmd
endif # DragonFly

ifeq (${HOST_OS}, FreeBSD)
ifeq ($(shell ${TEST} ${OSVER} -lt 400000 && ${ECHO} yes), yes) # 4.0-RELEASE
HAVE_SHA1	= no
HAVE_SHA256	= no
endif # 4.0-RELEASE
LIBS   += -lmd
endif # FreeBSD

ifeq (${HOST_OS}, Interix)
HAVE_SHA256	= no
LIBS   += -lcrypt
endif # Interix

ifeq (${HOST_OS}, NetBSD)
ifeq ($(shell ${TEST} ${OSVER} -lt 106010000 && ${ECHO} yes), yes) # 1.6A
HAVE_SHA1	= no
HAVE_SHA256	= no
endif # 1.6A
endif # NetBSD

ifeq (${HOST_OS}, OpenBSD)
ifeq ($(shell ${TEST} ${OSVER} -lt 200311 && ${ECHO} yes), yes) # 3.4
HAVE_SHA1	= no
HAVE_SHA256	= no
endif # 3.4
endif # OpenBSD

ifeq (${HOST_OS}, SunOS)
HAVE_SHA1	= no
HAVE_SHA256	= no
LIBS   += -lmd5
endif # SunOS

SRCS   += hash_native.c
endif # native

ifeq (${HASH_TYPE}, libgcrypt) # libgcrypt
HASH_SRCS	= hash_libgcrypt.c
HASH_LIBS      ?= -lgcrypt
endif # libgcrypt

ifeq (${HASH_TYPE}, openssl) # OpenSSL
HASH_SRCS	= hash_openssl.c
HASH_LIBS      ?= -lcrypto

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

HAVE_SHA1      ?= yes
HAVE_SHA256    ?= yes

ifeq (${HAVE_SHA1}, yes)
CFLAGS += -DHAVE_SHA1
endif # HAVE_SHA1

ifeq (${HAVE_SHA256}, yes)
CFLAGS += -DHAVE_SHA256
endif # HAVE_SHA256

ifeq (${HASH_TYPE}, none) # no hash support
hash-error:
	@${ECHO} "WARNING! Please specify the HASH_TYPE."
	@${ECHO} "  libgcrypt"
	@${ECHO} "  native (default)"
	@${ECHO} "  openssl"
	@exit 1;
endif # none
