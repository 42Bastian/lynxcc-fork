.PHONY: all mostlyclean clean install zip avail unavail bin lib doc html info examples

.SUFFIXES:

all mostlyclean clean install zip:
	@$(MAKE) -C compiler        --no-print-directory $@
	@$(MAKE) -f libraries.mk    --no-print-directory $@
	@$(MAKE) -C doc             --no-print-directory $@
	@$(MAKE) -C examples        --no-print-directory $@

avail unavail bin:
	@$(MAKE) -C compiler     --no-print-directory $@

lib:
	@$(MAKE) -f libraries.mk --no-print-directory $@

doc html info:
	@$(MAKE) -C doc     --no-print-directory $@

examples:
	@$(MAKE) -C examples --no-print-directory $@

%65:
	@$(MAKE) -C compiler     --no-print-directory $@

%:
	@$(MAKE) -f libraries.mk --no-print-directory $@
