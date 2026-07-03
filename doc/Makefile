#
# Makefile for the cc65 documentation (Atari Lynx tree).
#
# The documentation is maintained directly as modern HTML in this directory
# (converted from the original LinuxDoc SGML).  The pages ship as source; the
# only generated artefact is the quick-jump search index embedded in doc.js,
# which is refreshed from the page headings by `make doc-search-index` (see
# gen-search-index.py and design/DOC_SEARCH_DESIGN.md).  Whenever a section
# heading is added, renamed or removed, re-run that target; CI can enforce it
# with `make doc-search-index-check`.
#

.PHONY: all mostlyclean clean doc html info install zip \
        doc-search-index doc-search-index-check

htmldir = $(PREFIX)/share/doc/cc65$(DESTPACKAGE_SUFFIX)/html

INSTALL = install
PYTHON ?= python3

HTML = $(wildcard *.html)
ASSETS = doc.css doc.js logo.svg

# Regenerate the search index embedded in doc.js from the page headings.
doc-search-index:
	@$(PYTHON) gen-search-index.py

# Fail if the embedded search index is stale (for CI).
doc-search-index-check:
	@$(PYTHON) gen-search-index.py --check

# Nothing else to generate: the HTML ships as source.
all mostlyclean clean doc html info:

install:
	$(if $(PREFIX),,$(error variable "PREFIX" must be set))
	$(INSTALL) -d $(DESTDIR)$(htmldir)
	$(INSTALL) -m0644 $(HTML) $(ASSETS) $(DESTDIR)$(htmldir)

zip:
	@zip -j cc65-doc.zip $(HTML) $(ASSETS)
