# SPDX-License-Identifier: MIT
#
# Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
# Provided under the MIT License; copy it into your own projects freely.
# See the LICENSE file in the examples directory.

#
# Makefile for the sfxtest example.
#
# Unlike the flat mikey/ examples (one .c -> one .lnx via the top-level examples
# Makefile), sfxtest links the hand-authored rsound effect streams in sfx.s
# beside the demo object. (The sibling sfxdemo example, which stays flat, needs
# no extra object: it plays the reusable sfx pack whose per-effect members ld65
# pulls out of lib/lynx-audio.lib automatically - see doc/sound.html sec. 6.)
#
# See mikey/sfxtest/sfxtest.c and doc/sound.html.
#

ifdef CC65_HOME
  CC = $(CC65_HOME)/bin/cc65
  AS = $(CC65_HOME)/bin/ca65
  CL = $(CC65_HOME)/bin/cl65
else
  CC := $(if $(wildcard ../../../bin/cc65*),../../../bin/cc65,cc65)
  AS := $(if $(wildcard ../../../bin/ca65*),../../../bin/ca65,ca65)
  CL := $(if $(wildcard ../../../bin/cl65*),../../../bin/cl65,cl65)
endif

CFLAGS = -Ors --codesize 500 -T -g

TARGET = sfxtest.lnx

.PHONY: all clean
all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) $<
	$(AS) $(<:.c=.s)

%.o: %.s
	$(AS) $<

$(TARGET): sfxtest.o sfx.o
	$(CL) -o $@ -m $(@:.lnx=.map) sfxtest.o sfx.o

clean:
	$(RM) sfxtest.o sfxtest.s sfx.o sfxtest.map $(TARGET)
