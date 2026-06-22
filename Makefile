.PHONY: all mostlyclean clean install zip avail unavail bin lib doc html info examples tools tests

.SUFFIXES:

# Build order matters: the compiler suite first, then the standalone tools/
# utilities (they may invoke the toolchain), then the runtime+libraries, then
# everything that consumes them (LYNX_SDK_LAYOUT_DESIGN.md §12).
all mostlyclean clean install zip:
	@$(MAKE) -C compiler        --no-print-directory $@
	@$(MAKE) -C tools           --no-print-directory $@
	@$(MAKE) -f libraries.mk    --no-print-directory $@
	@$(MAKE) -C doc             --no-print-directory $@
	@$(MAKE) -C examples        --no-print-directory $@

avail unavail bin:
	@$(MAKE) -C compiler     --no-print-directory $@
	@$(MAKE) -C tools        --no-print-directory $@

tools:
	@$(MAKE) -C tools --no-print-directory all

lib:
	@$(MAKE) -f libraries.mk --no-print-directory $@

doc html info:
	@$(MAKE) -C doc     --no-print-directory $@

examples:
	@$(MAKE) -C examples --no-print-directory $@

# Run the test suite (host unit tests + GearLynx integration). Build the
# toolchain, libraries and examples first; the integration step skips itself
# when the emulator is absent (see tests/run.sh). Kept out of "all" — it is an
# on-demand / CI gate, not part of a plain build.
tests:
	@tests/run.sh

%65:
	@$(MAKE) -C compiler     --no-print-directory $@

%:
	@$(MAKE) -f libraries.mk --no-print-directory $@
