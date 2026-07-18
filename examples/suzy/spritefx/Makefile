# SPDX-License-Identifier: MIT
#
# Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
# Provided under the MIT License; copy it into your own projects freely.
# See the LICENSE file in the examples directory.

#
# Makefile for the spritefx transform-tour example.
#
# Unlike the flat suzy/ examples (one .c -> one .lnx via the top-level examples
# Makefile), spritefx generates a large fan of build-time sprite headers. Five
# source PCX images (four 16x16 shapes + a 5x5 marker) are drawn by
# spritefx_shapes.py and committed; sp65 compacts each shape FIVE times, once
# per action point (ax,ay), because the reference point is baked into the sprite
# data, not the SCB. That yields 20 shape headers plus the marker. The
# action-point coordinates below match the ap_name[] order the program tables
# them in.
#
# See suzy/spritefx/spritefx.c and doc/sprites.html sec. 6, sec. 6.1.
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

TARGET = spritefx.lnx

SHAPES = cube crystal star ball
HDRS   = $(foreach s,$(SHAPES),$(s)_tl.h $(s)_tr.h $(s)_c.h $(s)_bl.h $(s)_br.h) \
         marker.h

.PHONY: all clean
all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) $<
	$(AS) $(<:.c=.s)

# One sp65 run per shape emits its five action-point variants. Each -c converts
# the whole loaded image (no --slice, so no --pop): only the ax,ay action point
# differs between them, which is the whole point of the demo.
%_tl.h %_tr.h %_c.h %_bl.h %_br.h: %.pcx
	$(SP65) -r $< \
	  -c lynx-sprite,mode=packed,ax=0,ay=0   -w $*_tl.h,format=c,ident=$*_tl \
	  -c lynx-sprite,mode=packed,ax=15,ay=0  -w $*_tr.h,format=c,ident=$*_tr \
	  -c lynx-sprite,mode=packed,ax=8,ay=8   -w $*_c.h,format=c,ident=$*_c   \
	  -c lynx-sprite,mode=packed,ax=0,ay=15  -w $*_bl.h,format=c,ident=$*_bl \
	  -c lynx-sprite,mode=packed,ax=15,ay=15 -w $*_br.h,format=c,ident=$*_br

marker.h: marker.pcx
	$(SP65) -r $< -c lynx-sprite,mode=packed,ax=8,ay=8 -w $@,format=c,ident=marker

spritefx.o: spritefx.c $(HDRS)

$(TARGET): spritefx.o
	$(CL) -o $@ -m $(@:.lnx=.map) spritefx.o

clean:
	$(RM) spritefx.o spritefx.s $(HDRS) spritefx.map $(TARGET)
