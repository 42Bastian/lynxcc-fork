# SPDX-License-Identifier: MIT
# abccc - ABC tune cross-compiler (host tool, built outside the cc65 build).
CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra
PREFIX  ?= /usr/local

all: abccc

abccc: abccc.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f abccc

install: abccc
	install -d $(PREFIX)/bin
	install -m 755 abccc $(PREFIX)/bin/abccc

.PHONY: all clean install
