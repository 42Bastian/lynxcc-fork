# SPDX-License-Identifier: MIT
#
# Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
# Provided under the MIT License; copy it into your own projects freely.
# See the LICENSE file in the examples directory.

#
# Makefile for the HandyMusic engine example.
#
# Unlike the flat mikey/ examples (one .c -> one .lnx), this one drives the
# hmcc script compiler to build the music and SFX data blobs, then links against
# the reserved-region config cfg/lynx-handymusic.cfg. See doc/handymusic.html,
# doc/hmcc.html and design/LYNX_HANDYMUSIC_DESIGN.md.
#
#   1. hmcc $AE00 sfx.txt                 -> sfx.sfx + sfx.equ   (SFX blob)
#   2. sfx.equ                            -> sfx_equ.h           (C defines)
#   3. hmcc $B000 music.mus instr trk1..4 -> music.mus           (song blob)
#   4. cl65 -C lynx-handymusic.cfg ... links handymusic.o + hmdemo.o
#

ifdef CC65_HOME
  CC   = $(CC65_HOME)/bin/cc65
  AS   = $(CC65_HOME)/bin/ca65
  CL   = $(CC65_HOME)/bin/cl65
  HMCC = $(CC65_HOME)/bin/hmcc
else
  CC   := $(if $(wildcard ../../../bin/cc65*),../../../bin/cc65,cc65)
  AS   := $(if $(wildcard ../../../bin/ca65*),../../../bin/ca65,ca65)
  CL   := $(if $(wildcard ../../../bin/cl65*),../../../bin/cl65,cl65)
  HMCC := $(if $(wildcard ../../../bin/hmcc*),../../../bin/hmcc,hmcc)
endif

# Fixed data-region bases handed to hmcc.  These MUST match the __HMSFX_START__
# and __HMMUS_START__ values in cfg/lynx-handymusic.cfg.
SFX_BASE = AE00
MUS_BASE = B000

CFLAGS  = -Ors --codesize 500 -T -g
LDFLAGS = -C lynx-handymusic.cfg

TARGET = handymusic.lnx

.PHONY: all clean
all: $(TARGET)

# 1. Compile the SFX scripts to a blob + equates at the reserved SFX base.
sfx.sfx sfx.equ: sfx.txt
	$(HMCC) '$$$(SFX_BASE)' $<

# 2. Turn the hmcc equates into a C header (NAME EQU val -> #define NAME val).
sfx_equ.h: sfx.equ
	@echo '/* Generated from sfx.equ by the Makefile - do not edit. */' > $@
	@echo '#ifndef HM_SFX_EQU_H' >> $@
	@echo '#define HM_SFX_EQU_H' >> $@
	@awk '$$2=="EQU"{print "#define",$$1,$$3"u"}' $< >> $@
	@echo '#endif' >> $@

# 3. Compile the instrument + four track scripts to the song blob at the
#    reserved music base.
music.mus: instr.txt track1.txt track2.txt track3.txt track4.txt
	$(HMCC) '$$$(MUS_BASE)' $@ instr.txt track1.txt track2.txt track3.txt track4.txt

# 4a. The demo object depends on the generated SFX header.
handymusic.o: handymusic.c sfx_equ.h
	$(CC) $(CFLAGS) handymusic.c
	$(AS) handymusic.s

# 4b. The data glue .incbin's both blobs (found beside it in this dir).
hmdemo.o: hmdemo.s music.mus sfx.sfx
	$(AS) --bin-include-dir . hmdemo.s

$(TARGET): handymusic.o hmdemo.o
	$(CL) $(LDFLAGS) -o $@ -m $(@:.lnx=.map) handymusic.o hmdemo.o

clean:
	$(RM) handymusic.o handymusic.s hmdemo.o \
	      sfx.sfx sfx.equ sfx_equ.h music.mus \
	      handymusic.map $(TARGET)
