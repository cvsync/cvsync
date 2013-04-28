#
# Copyright (c) 2003-2005 MAEKAWA Masahide <maekawa@cvsync.org>
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
