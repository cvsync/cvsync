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
