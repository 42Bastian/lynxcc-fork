# SPDX-License-Identifier: MIT
#
# Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
# Provided under the MIT License; copy it into your own projects freely.
# See the LICENSE file in the examples directory.

#
# Makefile for the lander example.
#
# lander is a lunar-lander physics game. Unlike the flat games/ examples (one .c
# -> one .lnx via the top-level examples Makefile), it needs several build-time
# inputs:
#
#   * sp65 slices two 24-heading rotation sprite sheets out of lander.pcx and
#     flame.pcx (the module and its exhaust), and packs one solid fill sprite
#     from solidsq.pcx (terrain, pads and stars). All three .pcx are emitted by
#     lander.pcx.py.
#   * three background tunes, the committed abccc streams landermenu.s /
#     landergame.s / landerbass.s (regenerate by hand from the matching .abc
#     with abccc; see doc/sound.html), link beside the game object.
#
# See games/lander/lander.c, doc/sp65.html and doc/sound.html.
#

ifdef CC65_HOME
  CC   = $(CC65_HOME)/bin/cc65
  AS   = $(CC65_HOME)/bin/ca65
  CL   = $(CC65_HOME)/bin/cl65
  SP65 = $(CC65_HOME)/bin/sp65
else
  CC   := $(if $(wildcard ../../../bin/cc65*),../../../bin/cc65,cc65)
  AS   := $(if $(wildcard ../../../bin/ca65*),../../../bin/ca65,ca65)
  CL   := $(if $(wildcard ../../../bin/cl65*),../../../bin/cl65,cl65)
  SP65 := $(if $(wildcard ../../../bin/sp65*),../../../bin/sp65,sp65)
endif

CFLAGS = -Ors --codesize 500 -T -g

TARGET = lander.lnx

HDRS = lander_anim.h flame_anim.h solidsq.h

.PHONY: all clean
all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) $<
	$(AS) $(<:.c=.s)

%.o: %.s
	$(AS) $<

lander_anim.h: lander.pcx
	$(SP65) -r $< --sprite-sheet name=$@,ident=lander_anim,fw=24,fh=24,mode=packed

flame_anim.h: flame.pcx
	$(SP65) -r $< --sprite-sheet name=$@,ident=flame_anim,fw=24,fh=24,mode=packed

solidsq.h: solidsq.pcx
	$(SP65) -r $< -c lynx-sprite,mode=packed -w $@,format=c,ident=solidsq

lander.o: lander.c $(HDRS)

$(TARGET): lander.o landermenu.o landergame.o landerbass.o
	$(CL) -o $@ -m $(@:.lnx=.map) lander.o landermenu.o landergame.o landerbass.o

clean:
	$(RM) lander.o lander.s landermenu.o landergame.o landerbass.o \
	      $(HDRS) lander.map $(TARGET)
