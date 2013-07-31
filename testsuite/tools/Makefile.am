##  Makefile.am -- an automake template for Makefile.in file
##  Copyright (C) 2013 Stéphane Voltz and Sane Developers.
##
##  This file is part of the "Sane" build infra-structure.  See
##  included LICENSE file for license information.

all:
	@echo "run 'make check' to run tests"

SANEDESC  = ../../tools/sane-desc$(EXEEXT)
TESTFILE  = $(srcdir)/data/testfile.desc
OUTFILES  = *.res
DEVICE    = test

EXTRA_DIST = data/testfile.desc
CLEANFILES = $(OUTFILES)

all: help

help:
	@echo "Use 'make check' to run the tests."

check: check.local

check.local: 
	@echo "**** Testing $(SANEDESC) with $(TESTFILE)"
	@for mode in ascii html-backends-split html-mfgs xml statistics usermap db udev udev+acl plist hal hal-new; \
	do \
	echo "PASS: sane-desc -m $$mode"; \
	  $(SANEDESC) -m $$mode -s data >$$mode.res ;\
	  if ! diff --ignore-matching-lines="..:..:.. 20[0-9][0-9]" data/$$mode.ref $$mode.res ; \
	  then \
		echo "**** File generated for $$mode modee is different from reference" ;\
		exit 1 ;\
	  fi; \
	done ;\
	echo "================" ;\
	echo "All tests passed" ;\
	echo "================"
