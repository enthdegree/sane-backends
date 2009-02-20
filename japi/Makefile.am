##  Makefile.am -- an automake template for Makefile.in file
##  Copyright (C) 2009 Chris Bagwell and Sane Developers.
##
##  This file is part of the "Sane" build infra-structure.  See
##  included LICENSE file for license information.

#
#	Makefile for a Java project with native methods.
#

V_MAJOR = @V_MAJOR@
V_MINOR = @V_MINOR@
DLL_PRELOAD = @DLL_PRELOAD@

JAVAROOT=.
CLASSPATH_ENV=.
JAVAH=javah -jni -cp $(CLASSPATH_ENV)

#	We'll need something in 'configure' for the Java includes.
AM_CPPFLAGS = -I. -I$(srcdir) -I$(top_builddir)/include \
	      -I$(top_srcdir)/include -I${JDK_HOME}/include \
	      -I${JDK_HOME}/include/genunix

JAVA_SRC = SaneDevice.java SaneOption.java SaneRange.java\
	   Sane.java SaneParameters.java ScanIt.java \
	   ImageCanvas.java ImageCanvasClient.java \
	   Test.java Jscanimage.java 

dist_noinst_JAVA = $(JAVA_SRC)

LIBSANE = ../backend/libsane.la

EXTRA_DIST = Makefile.in README.JAVA 

lib_LTLIBRARIES = libsanej.la

libsanej_la_SOURCES = Sane.c
libsanej_la_LIBADD = $(LIBSANE)

# Make sure Sane.h gets created before compiling Sane.c
Sane.c: Sane.h

# Make sure that java classes get created before create Sane.h
Sane.h: classdist_noinst.stamp
	$(JAVAH) Sane

clean-local:
	rm -f Sane.h
