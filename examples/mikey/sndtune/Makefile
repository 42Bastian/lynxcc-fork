# SPDX-License-Identifier: MIT
#
# Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
# Provided under the MIT License; copy it into your own projects freely.
# See the LICENSE file in the examples directory.

#
# Makefile for the sndtune snd-engine example.
#
# Unlike the flat mikey/ examples (one .c -> one .lnx via the top-level examples
# Makefile), sndtune links the example against a compiled ABC tune stream.
# theme.s is the committed output of "abccc -f s -l theme_music -o theme.s
# theme.abc" (abccc lives under tools/abccc/, host-compiled into bin/, outside
# this build); regenerate it by hand when theme.abc changes.
#
# See mikey/sndtune/sndtune.c and doc/sound.html.
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

TARGET = sndtune.lnx

.PHONY: all clean
all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) $<
	$(AS) $(<:.c=.s)

%.o: %.s
	$(AS) $<

$(TARGET): sndtune.o theme.o
	$(CL) -o $@ -m $(@:.lnx=.map) sndtune.o theme.o

clean:
	$(RM) sndtune.o sndtune.s theme.o sndtune.map $(TARGET)
