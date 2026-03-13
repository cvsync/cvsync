#
# This software is released under the BSD License, see LICENSE.
#

CC_TYPE        ?= default

ifeq (${CC_TYPE}, forte) # Sun Microsystems Forte C
CC_PREFIX ?= /opt/SUNWspro
CC	= ${CC_PREFIX}/bin/cc
ifeq (${OSVER}, 5.10)
CFLAGS += -xc99
endif # 5.10
CFLAGS += -errwarn
endif # Sun Microsystems Forte C

ifeq (${CC_TYPE}, gnu) # GNU Compiler Collection
CC	= gcc
CFLAGS += -W -Wall -Wbad-function-cast -Wcast-align -Wchar-subscripts -Werror
CFLAGS += -Winline -Wmissing-prototypes -Wnested-externs -Wpointer-arith
CFLAGS += -Wshadow -Wstrict-prototypes
endif # GNU Compiler Collection

ifeq (${CC_TYPE}, intel) # Intel(R) C++ Compiler
CC_CPUTYPE?= ia32
CC_PREFIX ?= ${PREFIX}/intel/compiler70/${CC_CPUTYPE}
ifeq (${CC_CPUTYPE}, ia32)
CC	= ${CC_PREFIX}/bin/icc
endif # ia32
ifeq (${CC_CPUTYPE}, ia64)
CC	= ${CC_PREFIX}/bin/ecc
endif # ia64
CFLAGS += -Wall -Werror
endif # Intel(R) C++ Compiler

ifeq (${CC_TYPE}, tendra) # TenDRA
CC_PREFIX ?= ${PREFIX}
CC	= ${PREFIX}/bin/tcc
CFLAGS += -Ysystem
LDFLAGS+= -Ytdf_ext
endif # TenDRA

ifeq (${CC_TYPE}, clang) # Clang
CC	= clang
CFLAGS += -Werror -Weverything
CFLAGS += -Wno-format-nonliteral
CFLAGS += -Wno-padded
CFLAGS += -Wno-shorten-64-to-32
CFLAGS += -Wno-unsafe-buffer-usage
endif # Clang
