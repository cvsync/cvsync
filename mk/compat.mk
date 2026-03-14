#
# This software is released under the BSD License, see LICENSE.
#

CFLAGS += -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600

ifeq (${HOST_OS}, AIX)
CFLAGS += -D_UNIX98
CFLAGS += -DNO_STDBOOL_H
endif # AIX

ifeq (${HOST_OS}, BSD/OS)
CFLAGS += -DNO_STDBOOL_H
CFLAGS += -Duint8_t=u_int8_t -Duint16_t=u_int16_t -Duint32_t=u_int32_t
CFLAGS += -Duint64_t=u_int64_t
endif # BSD/OS

ifeq (${HOST_OS}, CYGWIN)
CFLAGS += -DNO_STDBOOL_H
CFLAGS += -DNO_PROTOTYPE_FDOPEN -DNO_PROTOTYPE_MKSTEMP -DNO_PROTOTYPE_REALPATH
CFLAGS += -DNO_PROTOTYPE_SNPRINTF -DNO_PROTOTYPE_STRCASECMP
endif # CYGWIN

ifeq (${HOST_OS}, FreeBSD)
ifeq ($(shell ${TEST} ${OSVER} -lt 411000 && ${ECHO} yes), yes) # 4.1.1-RELEASE
CFLAGS += -DNO_STDBOOL_H
endif # 4.1.1-RELEASE
CFLAGS += -DNO_PROTOTYPE_FCHMOD
endif # FreeBSD

ifeq (${HOST_OS}, Interix)
CFLAGS += -DNO_STDINT_H
CFLAGS += -DNO_PROTOTYPE_FCHMOD -DNO_PROTOTYPE_READLINK -DNO_PROTOTYPE_REALPATH
CFLAGS += -DNO_PROTOTYPE_SNPRINTF -DNO_PROTOTYPE_STRCASECMP
CFLAGS += -DNO_PROTOTYPE_STRNCASECMP -DNO_PROTOTYPE_SYMLINK
CFLAGS += -DNO_PROTOTYPE_VSNPRINTF
CFLAGS += -Du_char='unsigned char' -Du_int='unsigned int'
CFLAGS += -Du_long='unsigned long'
CFLAGS += -Duint8_t='unsigned char' -Duint16_t='unsigned short'
CFLAGS += -Duint32_t='unsigned long' -Duint64_t='unsigned long long'
endif # Interix

ifeq (${HOST_OS}, NetBSD)
ifeq ($(shell ${TEST} ${OSVER} -lt 105220000 && ${ECHO} yes), yes) # 1.5V
CFLAGS +=-DNO_STDINT_H
endif # 1.5V
ifeq ($(shell ${TEST} ${OSVER} -lt 106190000 && ${ECHO} yes), yes) # 1.6S
CFLAGS += -DNO_PROTOTYPE_SETEGID -DNO_PROTOTYPE_SETEUID
endif # 1.6S
ifeq ($(shell ${TEST} ${OSVER} -lt 106230000 && ${ECHO} yes), yes) # 1.6W
CFLAGS +=-DNO_STDBOOL_H
endif # 1.6W
CFLAGS += -Du_char='unsigned char' -Du_int='unsigned int'
CFLAGS += -Du_long='unsigned long'
endif # NetBSD

ifeq (${HOST_OS}, OpenBSD)
ifeq ($(shell ${TEST} ${OSVER} -lt 199912 && ${ECHO} yes), yes) # 2.6
CFLAGS += -DNO_STDBOOL_H
endif # 2.6
CFLAGS += -Du_char='unsigned char' -Du_short='unsigned short'
CFLAGS += -Du_int='unsigned int'
endif # OpenBSD

ifeq (${HOST_OS}, SunOS)
CFLAGS += -D__EXTENSIONS__
ifneq (${OSVER}, 5.10)
CFLAGS += -DNO_STDINT_H
endif # != 5.10
endif # SunOS
