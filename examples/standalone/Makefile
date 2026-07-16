# SPDX-License-Identifier: MIT
#
# Lynx Game Development SDK example, (c) 2026 the lynxcc authors.
# Provided under the MIT License; copy it into your own projects freely.
# See the LICENSE file in the examples directory.

# Standalone Makefile for a minimal Lynx Game Development SDK project.
#
# This example is deliberately NOT built by the shared examples/ Makefile: it is
# a self-contained project that carries its own build rule, so you can copy the
# directory out of the SDK tree and it still builds on its own.
#
# Requires GNU make and the lynxcc SDK: either put its bin/ on your PATH, or set
# CC65_HOME to an installed SDK root (the dir holding bin/ include/ lib/ ...).
#
# Every build product -- the object files, the .map and the final .lnx -- is
# written under BUILDDIR (build/), so the source tree stays clean and "make
# clean" is just a directory removal. Building is split into an explicit
# compile-to-object step and a link step so the intermediates have a home in
# build/; cl65 still resolves the SDK's optional subsystem libraries
# automatically (design/LYNX_CL65_AUTOLIBS_DESIGN.md), so no -l flags are
# needed. cl65 already targets the Lynx by default.

PROGRAM  = hello
BUILDDIR = build
SOURCES  = src/main.c
OBJECTS  = $(patsubst src/%.c,$(BUILDDIR)/%.o,$(SOURCES))

ifdef CC65_HOME
  CL = $(CC65_HOME)/bin/cl65
else
  CL = cl65
endif

CFLAGS  = -Ors --codesize 500
LDFLAGS =
LIBS    =

TARGET = $(BUILDDIR)/$(PROGRAM).lnx

# Link the objects into a ready-to-run .lnx plus a .map, both in build/.
$(TARGET): $(OBJECTS)
	$(CL) $(LDFLAGS) -o $@ -m $(BUILDDIR)/$(PROGRAM).map $(OBJECTS) $(LIBS)

# Compile+assemble each source to an object in build/ (cl65 -c cleans up its
# own intermediate .s, so nothing is left beside the sources).
$(BUILDDIR)/%.o: src/%.c | $(BUILDDIR)
	$(CL) $(CFLAGS) -c -o $@ $<

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

.PHONY: all clean
all: $(TARGET)

clean:
	$(RM) -r $(BUILDDIR)
