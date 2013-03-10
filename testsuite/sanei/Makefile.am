##  Makefile.am -- an automake template for Makefile.in file
##  Copyright (C) 2013 Stéphane Voltz and Sane Developers.
##
##  This file is part of the "Sane" build infra-structure.  See
##  included LICENSE file for license information.

MATH_LIB = @MATH_LIB@
USB_LIBS = @USB_LIBS@
PTHREAD_LIBS = @PTHREAD_LIBS@

check_PROGRAMS = sanei_usb_test
TESTS = $(check_PROGRAMS)

AM_CPPFLAGS = -I. -I$(srcdir) -I$(top_builddir)/include -I$(top_srcdir)/include

sanei_usb_test_SOURCES = sanei_usb_test.c
sanei_usb_test_LDADD = ../../sanei/libsanei.la ../../lib/liblib.la ../../lib/libfelib.la $(MATH_LIB) $(USB_LIBS) $(PTHREAD_LIBS)

all:
	@echo "run 'make check' to run tests"
