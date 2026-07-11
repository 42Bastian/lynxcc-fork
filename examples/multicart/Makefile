# SPDX-License-Identifier: MIT
#
# Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
# Provided under the MIT License; copy it into your own projects freely.
# See the LICENSE file in the examples directory.

#
# Makefile for the multicart example.
#
# A multicart bundles a menu program and several game ROMs into one bootable
# .lnx. Unlike every other example (one .c -> one .lnx via the top-level
# examples Makefile), this one has its own build because it links several BLL
# objects and stitches them together with two SDK tools:
#
#   1. cl65 -C lynx-bll.cfg   builds the menu and each game as a BLL object.
#   2. lnx multicart          writes a lynxdir .mak describing the layout.
#   3. lynxdir                 assembles the .mak into the bootable multicart.lnx.
#
# See design/LYNX_MULTICART_DESIGN.md and doc/multicart.html.
#

ifdef CC65_HOME
  CL      = $(CC65_HOME)/bin/cl65
  LNX     = $(CC65_HOME)/bin/lnx
  LYNXDIR = $(CC65_HOME)/bin/lynxdir
else
  CL      := $(if $(wildcard ../../bin/cl65*),../../bin/cl65,cl65)
  LNX     := $(if $(wildcard ../../bin/lnx*),../../bin/lnx,lnx)
  LYNXDIR := $(if $(wildcard ../../bin/lynxdir*),../../bin/lynxdir,lynxdir)
endif

# The menu and the games are BLL objects: cl65 links them with the BLL cart
# config (10-byte BS93 header, image at $0400) that lynxdir understands.
BLLFLAGS = -C lynx-bll.cfg -Ors --codesize 500

CART  = multicart.lnx
MENU  = menu.o
GAMES = game1.o game2.o game3.o

.PHONY: all clean
all: $(CART)

# Each BLL object is one self-contained program.
%.o: %.c
	$(CL) $(BLLFLAGS) -o $@ -m $(@:.o=.map) $<

# Describe the layout, then let lynxdir build the cart. lynxdir names its output
# after the .mak (multicart.mak -> multicart.lnx + multicart.lyx).
multicart.mak: $(MENU) $(GAMES)
	$(LNX) multicart --menu $(MENU) -o $@ $(GAMES)

$(CART): multicart.mak
	$(LYNXDIR) multicart.mak

clean:
	$(RM) $(MENU) $(GAMES) $(MENU:.o=.map) $(GAMES:.o=.map) \
	      menu.s game1.s game2.s game3.s \
	      multicart.mak multicart.lnx multicart.lyx
