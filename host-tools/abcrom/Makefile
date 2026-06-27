# SPDX-License-Identifier: MIT
# abcrom - tune test-ROM utility (host tool, built outside the cc65 build).
CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra
PREFIX  ?= /usr/local

all: abcrom

abcrom: abcrom.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f abcrom

install: abcrom
	install -d $(PREFIX)/bin
	install -m 755 abcrom $(PREFIX)/bin/abcrom

.PHONY: all clean install
