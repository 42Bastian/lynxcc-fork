# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
#
# Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.

#
# Host-built unit tests for the Lynx Game Development SDK.
#
# These are ordinary C programs compiled with the HOST compiler (not cc65):
# they validate the arithmetic invariants the on-target runtime relies on, so
# they need no emulator and run anywhere. `make` builds and runs them; any
# failing program returns nonzero and stops the run.
#

HOSTCC ?= cc
HOSTCFLAGS ?= -O2 -Wall -Wextra -std=c99

TESTS = suzymath

BINS = $(addprefix bin/,$(TESTS))

.PHONY: all run clean
all: run

bin/%: %.c | bin
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $<

bin:
	@mkdir -p bin

# Build then execute every unit test; the first nonzero exit aborts (-e).
run: $(BINS)
	@set -e; for t in $(BINS); do echo "== $$t =="; ./$$t; done

clean:
	$(RM) -r bin
