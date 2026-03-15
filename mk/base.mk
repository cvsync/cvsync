#
# This software is released under the BSD License, see LICENSE.
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
_OSVER := $(shell /usr/sbin/sysctl -n kern.osrelease | /usr/bin/cut -d . -f 1)
OSVER  ?= ${_OSVER}
BINGRP	= admin
endif # Darwin

ifeq (${HOST_OS}, DragonFly)
_OSVER := $(shell /sbin/sysctl -n kern.osreldate)
OSVER  ?= ${_OSVER}
endif # DragonFly

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
