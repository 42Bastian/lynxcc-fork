#
# Makefile for the cc65 documentation (Atari Lynx tree).
#
# The documentation is now maintained directly as modern HTML in this
# directory (converted from the original LinuxDoc SGML).  There is no longer a
# generation step, so the build targets operate on the committed *.html files
# and the shared doc.css / doc.js assets.
#

.PHONY: all mostlyclean clean doc html info install zip

htmldir = $(PREFIX)/share/doc/cc65$(DESTPACKAGE_SUFFIX)/html

INSTALL = install

HTML = $(wildcard *.html)
ASSETS = doc.css doc.js logo.svg

# Nothing to generate any more: the HTML ships as source.
all mostlyclean clean doc html info:

install:
	$(if $(PREFIX),,$(error variable "PREFIX" must be set))
	$(INSTALL) -d $(DESTDIR)$(htmldir)
	$(INSTALL) -m0644 $(HTML) $(ASSETS) $(DESTDIR)$(htmldir)

zip:
	@zip -j cc65-doc.zip $(HTML) $(ASSETS)
