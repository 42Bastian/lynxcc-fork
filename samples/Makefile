#
# Makefile for cc65 samples (Atari Lynx)
#
# This Makefile requires GNU make
#

SYS ?= lynx

ifneq ($(shell echo),)
  CMD_EXE = 1
endif

ifdef CMD_EXE
  NULLDEV = nul:
  DEL = -del /f
  RMDIR = rmdir /s /q
else
  NULLDEV = /dev/null
  DEL = $(RM)
  RMDIR = $(RM) -r
endif

ifdef CC65_HOME
  AS = $(CC65_HOME)/bin/ca65
  CC = $(CC65_HOME)/bin/cc65
  CL = $(CC65_HOME)/bin/cl65
  LD = $(CC65_HOME)/bin/ld65
else
  AS := $(if $(wildcard ../bin/ca65*),../bin/ca65,ca65)
  CC := $(if $(wildcard ../bin/cc65*),../bin/cc65,cc65)
  CL := $(if $(wildcard ../bin/cl65*),../bin/cl65,cl65)
  LD := $(if $(wildcard ../bin/ld65*),../bin/ld65,ld65)
endif

# --------------------------------------------------------------------------
# Generic rules

.PHONY: all mostlyclean clean install zip samples

%: %.c
%: %.s

.c.o:
	$(CC) $(CFLAGS) -Ors --codesize 500 -T -g $<
	$(AS) $(<:.c=.s)

.s.o:
	$(AS) $(ASFLAGS) $<

.PRECIOUS: %.o

%.lnx: %.o
	$(LD) $(LDFLAGS_$(@F)_$(SYS)) $(LDFLAGS) -o $@ -m $(@:.lnx=.map) $^ $(SYS).lib

# --------------------------------------------------------------------------
# Samples that build for the Lynx

EXELIST_lynx = \
        lynxdemo.lnx \
        suzybench.lnx \
        suzyasync.lnx \
        suzyasyncbench.lnx \
        breakout.lnx \
        invaders.lnx \
        raycaster.lnx \
        sybil.lnx \
        setbpp.lnx \
        comlynx.lnx \
        fonttest.lnx \
        heaptest.lnx \
        zeropage.lnx

samples: $(EXELIST_$(SYS))

# The top-level Makefile invokes "make -C samples all". Always rebuild the
# samples from scratch here so that a changed toolchain, library or config is
# guaranteed to be re-exercised (the .lnx files carry no dependency on the
# library, so an incremental build would otherwise leave them stale).
all:
	@$(MAKE) --no-print-directory mostlyclean
	@$(MAKE) --no-print-directory samples

# --------------------------------------------------------------------------
# Installation rules

INSTALL = install
samplesdir = $(PREFIX)/share/cc65/samples

install:
	$(if $(PREFIX),,$(error variable "PREFIX" must be set))
	$(INSTALL) -d $(DESTDIR)$(samplesdir)
	$(INSTALL) -m0644 *.* $(DESTDIR)$(samplesdir)
	$(INSTALL) -m0644 Makefile $(DESTDIR)$(samplesdir)

# --------------------------------------------------------------------------
# Packaging rules

zip:
	@cd .. && zip -r cc65 samples/

# --------------------------------------------------------------------------
# Clean-up rules

mostlyclean:
	@$(DEL) *.map *.o *.s 2>$(NULLDEV)

clean: mostlyclean
	@$(DEL) $(EXELIST_$(SYS)) samples.* 2>$(NULLDEV)
