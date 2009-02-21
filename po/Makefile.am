##  Makefile.am -- an automake template for Makefile.in file
##  Copyright (C) 2009 Chris Bagwell and Sane Developers.
##
##  This file is part of the "Sane" build infra-structure.  See
##  included LICENSE file for license information.

# See po/README for documentation.

MSGFMT = @MSGFMT@
XGETTEXT = @XGETTEXT@
MSGMERGE = @MSGMERGE@ -w75

TMP_FILE_DIR = .tmp

ALL_LINGUAS = bg cs da de eo es fi fr it nl nb pl pt ru sv 

EXTRA_DIST = README template.po

PO_FILES = sane-backends.bg.po sane-backends.cs.po sane-backends.da.po \
  sane-backends.de.po sane-backends.eo.po sane-backends.es.po \
  sane-backends.fi.po sane-backends.fr.po sane-backends.it.po \
  sane-backends.nb.po sane-backends.nl.po sane-backends.pl.po \
  sane-backends.pt.po sane-backends.ru.po sane-backends.sv.po

EXTRA_DIST += $(PO_FILES)

MO_FILES = sane-backends.bg.mo sane-backends.cs.mo sane-backends.da.mo \
  sane-backends.de.mo sane-backends.eo.mo sane-backends.es.mo \
  sane-backends.fi.mo sane-backends.fr.mo sane-backends.it.mo \
  sane-backends.nb.mo sane-backends.nl.mo sane-backends.pl.mo \
  sane-backends.pt.mo sane-backends.ru.mo sane-backends.sv.mo

if ENABLE_TRANSLATIONS
INSTALL_TRANSLATIONS = install-translations
UNINSTALL_TRANSLATIONS = uninstall-translations
ALL = all-mos
else
INSTALL_TRANSLATIONS = 
UNINSTALL_TRANSLATIONS = 
ALL = 
endif

all: $(ALL)

all-mos: $(MO_FILES)

.po.mo:
	@echo "generating $@ from $^"
	@$(MSGFMT) -o $@ $^

install-translations:
	@for lang in $(ALL_LINGUAS) ; do \
	  dir=$(localedir)/$${lang}/LC_MESSAGES ; \
	  echo "installing sane-backends.$${lang}.mo to $${dir}/sane-backends.mo..." ; \
	 $(mkinstalldirs) $(DESTDIR)$${dir} && \
	 $(INSTALL_DATA) sane-backends.$${lang}.mo $(DESTDIR)$${dir}/sane-backends.mo ; \
	 done

install-data-local: $(INSTALL_TRANSLATIONS)

uninstall-translations:
	@for lang in $(ALL_LINGUAS) ; do \
	   dir=$(localedir)/$${lang}/LC_MESSAGES ; \
	   echo removing $${dir}/sane-*.mo ; \
	   rm -f $(DESTDIR)$${dir}/sane-*.mo ; \
	 done

uninstall-local: $(UNINSTALL_TRANSLATIONS)

clean-local:
	rm -f *.mo
	rm -f *.old
	rm -f *.pot

distclean-local:
	rm -rf $(TMP_FILE_DIR)

SRC_FILES = ../include/sane/saneopts.h ../backend/sane_strstatus.c \
  ../backend/artec_eplus48u.c ../backend/avision.h \
  ../backend/canon630u.c ../backend/canon.c ../backend/canon-sane.c \
  ../backend/epson.c ../backend/epson.h \
  ../backend/epson2.c ../backend/epson2.h \
  ../backend/genesys.c \
  ../backend/gt68xx.c ../backend/gt68xx_low.h \
  ../backend/hp3500.c ../backend/hp3900_sane.c ../backend/hp5400_sane.c \
  ../backend/hp5590.c \
  ../backend/hp-option.c ../backend/hp-option.h \
  ../backend/leo.c ../backend/leo.h ../backend/lexmark.c \
  ../backend/ma1509.c \
  ../backend/matsushita.c ../backend/matsushita.h \
  ../backend/microtek2.c ../backend/microtek2.h \
  ../backend/mustek.c ../backend/mustek_usb.c ../backend/mustek_usb2.c \
  ../backend/niash.c \
  ../backend/pixma.c ../backend/pixma_sane_options.c \
  ../backend/plustek.c ../backend/plustek_pp.c \
  ../backend/pnm.c ../backend/rts8891.c \
  ../backend/sceptre.c ../backend/sceptre.h  ../backend/sm3840.c \
  ../backend/snapscan.c ../backend/snapscan-options.c \
  ../backend/stv680.c ../backend/stv680.h \
  ../backend/teco1.c ../backend/teco1.h \
  ../backend/teco2.c ../backend/teco2.h \
  ../backend/teco3.c ../backend/teco3.h \
  ../backend/test.c ../backend/u12.c ../backend/umax1220u.c \
  ../backend/umax.c ../backend/umax_pp.c

sane-backends.pot: $(SRC_FILES)
	@echo "creating $@ from $^"
	@rm -rf $(TMP_FILE_DIR)
	@mkdir $(TMP_FILE_DIR) && \
	 for file in $^ ; do \
	   sed < $${file} -e 's/#define//g' \
	     > $(TMP_FILE_DIR)/`basename $${file}` ; \
	 done
	@$(XGETTEXT) -dsane-backends -kSANE_I18N --flag=SANE_I18N:1:no-c-format $(TMP_FILE_DIR)/*.*
	@mv sane-backends.po sane-backends.pot

update: sane-backends.pot
	@for lang in $(ALL_LINGUAS) ; do \
	   pofile=sane-backends.$${lang}.po ; \
	   if test ! -f $${pofile} ; then \
	     echo "creating $${pofile}" ; \
	     cp template.po $${pofile} ; \
	   fi ; \
	   echo "updating $${pofile}" ; \
	   cp $${pofile} $${pofile}.old  ; \
	   $(MSGMERGE) $${pofile}.old $< -o $${pofile} ; \
	   rm -f $${pofile}.old ; \
	 done 
