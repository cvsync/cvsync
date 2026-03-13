#
# This software is released under the BSD License, see LICENSE.
#

ifeq (${HOST_OS}, AIX)
ZLIB_PREFIX    ?= /usr/local
endif # AIX

ifeq (${HOST_OS}, Interix)
ZLIB_PREFIX    ?= /usr/local
endif # Interix

ifeq (${HOST_OS}, SunOS)
ifeq (${OSVER}, 5.7)
ZLIB_PREFIX    ?= /usr/local
endif # 5.7
endif # SunOS

ZLIB_PREFIX    ?= none

ifneq (${ZLIB_PREFIX}, none)
CFLAGS += -I${ZLIB_PREFIX}/include
LDFLAGS+= -L${ZLIB_PREFIX}/lib

ifeq (${HOST_OS}, SunOS)
ifeq (${OSVER}, 5.7)
LDFLAGS+= -R${ZLIB_PREFIX}/lib
endif # 5.7
endif # SunOS
endif # ZLIB_PREFIX

LIBS   += -lz
