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

MAN	= ${PROG}.1

HOST_OS:= $(findstring CYGWIN,$(shell uname -s))
ifneq (${HOST_OS}, CYGWIN)
HOST_OS:= $(shell uname -s)
endif # !CYGWIN

ifeq (${HOST_OS}, AIX)
_OSVER := $(shell /usr/bin/uname -v)
OSVER  ?= ${_OSVER}
BINGRP	= system
INSTALL	= /usr/ucb/install
endif # AIX

ifeq (${HOST_OS}, CYGWIN)
MAN    := ${PROG}.1
PROG   := $(PROG:=.exe)
BINOWN	= Administrator
BINGRP	= Administrators
ECHO	= /usr/bin/echo
TEST	= /usr/bin/test
endif # CYGWIN

ifeq (${HOST_OS}, Darwin)
BINGRP	= admin
endif # Darwin

ifeq (${HOST_OS}, FreeBSD)
_OSVER := $(shell /sbin/sysctl -n kern.osreldate)
OSVER  ?= ${_OSVER}
endif # FreeBSD

ifeq (${HOST_OS}, Interix)
BINOWN	= Administrator
BINGRP	= +Administrators
endif # Interix

ifeq (${HOST_OS}, IRIX)
BINGRP	= sys
endif # IRIX

ifeq (${HOST_OS}, Linux)
TEST	= /usr/bin/test
endif # Linux

ifeq (${HOST_OS}, NetBSD)
_OSVER := $(shell /sbin/sysctl -n kern.osrevision)
OSVER  ?= ${_OSVER}
PREFIX ?= /usr/pkg
endif # NetBSD

ifeq (${HOST_OS}, OpenBSD)
_OSVER := $(shell /usr/sbin/sysctl -n kern.osrevision)
OSVER  ?= ${_OSVER}
endif # OpenBSD

ifeq (${HOST_OS}, SunOS)
_OSVER := $(shell /bin/uname -r)
OSVER  ?= ${_OSVER}
BINGRP	= root
INSTALL	= /usr/ucb/install
endif # SunOS

PREFIX ?= /usr/local
BINDIR ?= ${PREFIX}/bin
MANDIR ?= ${PREFIX}/man

BINOWN ?= root
BINGRP ?= wheel
BINMODE?= 755
MANMODE?= 644

INSTALL		?= /usr/bin/install
INSTALL_BIN_OPTS?= -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE}
INSTALL_MAN_OPTS?= -c -o ${BINOWN} -g ${BINGRP} -m ${MANMODE}

CVSYNC_DEFAULT_CONFIG	?=
CVSYNCD_DEFAULT_CONFIG	?=

ECHO	?= /bin/echo
TEST	?= /bin/test

MKDEP		?= /usr/bin/mkdep
MKDEP_FILE	?= .depend
MKDEP_OPT	?= -f ${MKDEP_FILE}
ifeq (${HOST_OS}, NetBSD)
ifeq ($(shell ${TEST} ${OSVER} -ge 300000000 && ${ECHO} yes), yes) # 3.0
MKDEP_OPT	+= --
endif # 3.0
endif # NetBSD

ifeq ($(shell ${TEST} -f ../mk/defaults.mk && ${ECHO} yes), yes)
include ../mk/defaults.mk
endif

include ../mk/cc.mk
include ../mk/compat.mk
