#
# This software is released under the BSD License, see LICENSE.
#

SUBDIRS	= cvscan cvsup2cvsync cvsync cvsync2cvsup cvsyncd rcscan rcscmp

all clean cleandir depend distclean install uninstall:
	@for _sub in ${SUBDIRS}; do (cd $${_sub} && ${MAKE} $@) || exit 1; done

configure:
	$(RM) mk/defaults.mk
	(cd mk && ${MAKE} $@) || exit 1;
