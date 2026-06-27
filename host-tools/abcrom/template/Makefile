# SPDX-License-Identifier: MIT
#
# Build the abcrom template ROM once with the lynxcc toolchain.  This IS part of
# the player project (it embeds a specific engine build + stream format
# version), not part of abcrom itself (design sec. 9.2/9.5): rebuild it whenever
# the engine or stream format changes.
#
# Produces ../template.lnx (the bundled default) and ../template.lnx.map, whose
# exported region symbols abcrom reads to resolve envelope pointers.

# Repo root is four levels up: host-tools/abcrom/template -> .
ROOT     := ../../..
export CC65_HOME := $(abspath $(ROOT))

CL65 := $(if $(wildcard $(ROOT)/bin/cl65*),$(ROOT)/bin/cl65,cl65)

OUT  := ../template.lnx
MAP  := ../template.lnx.map

all: $(OUT)

$(OUT): template.c regions.s
	$(CL65) -t lynx -O -m $(MAP) -o $(OUT) template.c regions.s

clean:
	rm -f $(OUT) $(MAP) template.o regions.o

.PHONY: all clean
