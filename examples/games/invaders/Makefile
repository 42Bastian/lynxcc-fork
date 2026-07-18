# SPDX-License-Identifier: MIT
#
# Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
# Provided under the MIT License; copy it into your own projects freely.
# See the LICENSE file in the examples directory.

#
# Makefile for the invaders example.
#
# Unlike the flat games/ examples (one .c -> one .lnx via the top-level examples
# Makefile), invaders plays the Space Invaders theme from a hand-authored
# snd-engine stream, so it links its game object beside a second object:
#
#   invaders.o  the game itself
#   invtheme.o  the committed snd-engine tune stream (a re-encoding of a tune
#               written for an earlier engine; see invtheme.s)
#
# See games/invaders/invaders.c and doc/sound.html.
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

TARGET = invaders.lnx

.PHONY: all clean
all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) $<
	$(AS) $(<:.c=.s)

%.o: %.s
	$(AS) $<

$(TARGET): invaders.o invtheme.o
	$(CL) -o $@ -m $(@:.lnx=.map) invaders.o invtheme.o

clean:
	$(RM) invaders.o invaders.s invtheme.o invaders.map $(TARGET)
