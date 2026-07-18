# SPDX-License-Identifier: MIT
#
# Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
# Provided under the MIT License; copy it into your own projects freely.
# See the LICENSE file in this directory.

#
# Makefile for the Lynx Game Development SDK examples
#
# This Makefile requires GNU make.
#
# The examples are sorted into subsystem group subdirectories (games/, suzy/,
# mikey/, memory/, network/); the starter demo lynxdemo.c sits at the examples
# root. Single-file examples live as a flat .c directly inside their group, and
# the generated .s/.o/.map/.lnx land beside them; this Makefile iterates over the
# groups to build those.
#
# Any example that needs more than the plain "one .c -> one .lnx" rule - extra
# hand-authored objects, build-time sprite/audio assets, or a non-default link
# config - instead lives in its OWN subdirectory with its OWN Makefile (the same
# pattern as multicart/ and mikey/handymusic/). This top-level Makefile builds
# each of those by delegating with "make -C <dir>" (see SUBDIR_EXAMPLES below),
# so a single "make" here still builds the whole example set.
#

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
  SP65 = $(CC65_HOME)/bin/sp65
  SPRPCK = $(CC65_HOME)/bin/sprpck
else
  AS := $(if $(wildcard ../bin/ca65*),../bin/ca65,ca65)
  CC := $(if $(wildcard ../bin/cc65*),../bin/cc65,cc65)
  CL := $(if $(wildcard ../bin/cl65*),../bin/cl65,cl65)
  LD := $(if $(wildcard ../bin/ld65*),../bin/ld65,ld65)
  SP65 := $(if $(wildcard ../bin/sp65*),../bin/sp65,sp65)
  SPRPCK := $(if $(wildcard ../bin/sprpck*),../bin/sprpck,sprpck)
endif

# --------------------------------------------------------------------------
# The flat, single-file examples, grouped by subsystem. lynxdemo is the starter
# and lives at the examples root; every example listed here is a flat .c inside
# its group directory, built by the generic "one .c -> one .lnx" rule below.
# Multi-file examples are NOT listed here - they live in their own subdirectory
# (see SUBDIR_EXAMPLES).

ROOT_EXAMPLES = lynxdemo
GAMES         = breakout raycaster sybil tetris
SUZY          = muldivtest suzyasync suzyasyncbench suzybench packtest fonttest fontvar textdir collision palette
MIKEY         = setbpp sfxdemo poweroff
MEMORY        = heaptest zeropage
NETWORK       = comlynx
SDCARD_GD     = sdcardls

EXELIST = \
        $(addsuffix .lnx,$(ROOT_EXAMPLES)) \
        $(addprefix games/,$(addsuffix .lnx,$(GAMES))) \
        $(addprefix suzy/,$(addsuffix .lnx,$(SUZY))) \
        $(addprefix mikey/,$(addsuffix .lnx,$(MIKEY))) \
        $(addprefix memory/,$(addsuffix .lnx,$(MEMORY))) \
        $(addprefix network/,$(addsuffix .lnx,$(NETWORK))) \
        $(addprefix sdcard-gd/,$(addsuffix .lnx,$(SDCARD_GD)))

# The multi-file examples, each self-contained in its own subdirectory with its
# own Makefile. This top-level build delegates to each with "make -C <dir>".
# multicart and mikey/handymusic follow the same pattern and join the list.
SUBDIR_EXAMPLES = \
        games/invaders \
        games/lander \
        suzy/spritetest \
        suzy/spritesheet \
        suzy/spriteslice \
        suzy/spritefx \
        suzy/sprpcktest \
        mikey/sndtune \
        mikey/sfxtest \
        mikey/handymusic \
        multicart

SUBDIR_CLEAN = $(addsuffix .clean,$(SUBDIR_EXAMPLES))

# --------------------------------------------------------------------------
# Generic rules

.PHONY: all mostlyclean clean install zip examples $(SUBDIR_EXAMPLES) $(SUBDIR_CLEAN)

%: %.c
%: %.s

%.o: %.c
	$(CC) $(CFLAGS) -Ors --codesize 500 -T -g $<
	$(AS) $(<:.c=.s)

%.o: %.s
	$(AS) $(ASFLAGS) $<

.PRECIOUS: %.o

# The SDK libraries are split into a core archive plus opt-in subsystem
# archives (graphics, audio, compress, math) — see
# design/LYNX_SDK_LAYOUT_DESIGN.md sec. 6. cl65 resolves them automatically
# (sec. 6.6 and design/LYNX_CL65_AUTOLIBS_DESIGN.md): at the link step it
# appends the whole SDK set named by lib/lynx-sdklibs.list, in dependents-first
# / core-last order, so these examples link with no -l flags at all. ld65
# extracts only referenced modules, so subsystems an example does not touch
# contribute nothing and the .lnx is byte-identical to linking exactly the
# needed libraries. To link explicitly instead, use "cl65 --no-sdk-libs" with
# -l, or invoke ld65 directly with the dependency order from sec. 6.4.

%.lnx: %.o
	$(CL) $(LDFLAGS) -o $@ -m $(@:.lnx=.map) $^

# --------------------------------------------------------------------------
# The collision demo needs the collision buffer, which only the lynx-coll.cfg
# memory layout reserves (it carves 8 KB below the screen pages; the default
# lynx.cfg leaves that RAM to the program). So this one flat example overrides
# the link config; every other flat example links with the default lynx.cfg. See
# suzy/collision.c and doc/collisions.html.

suzy/collision.lnx: suzy/collision.o
	$(CL) $(LDFLAGS) -C lynx-coll.cfg -o $@ -m $(@:.lnx=.map) $^

# --------------------------------------------------------------------------
# Multi-file examples, each self-contained in its own subdirectory with its own
# Makefile. Delegate to each with "make -C <dir>", building from scratch so a
# changed toolchain, library or config is re-exercised. Every entry documents
# what makes it need more than the flat .c -> .lnx rule:
#
#   games/invaders     links a hand-authored snd-engine theme (invtheme.s)
#   games/lander       sp65 sprite sheets + three committed abccc tune streams
#   suzy/spritetest    heart.pcx in sp65 "packed" and "literal" encodings
#   suzy/spritesheet   sheet.pcx via the sp65 --sprite-sheet driver
#   suzy/spriteslice   sheet.pcx via the manual sp65 --slice/--pop chain
#   suzy/spritefx      five PCX shapes x five action points (21 sp65 headers)
#   suzy/sprpcktest    offline sprpck packing of BMP + SPS inputs (.incbin)
#   mikey/sndtune      links a compiled ABC tune stream (theme.s)
#   mikey/sfxtest      links the hand-authored rsound effect streams (sfx.s)
#   mikey/handymusic   hmcc-compiled music/SFX blobs + reserved-region cfg
#   multicart          bundles a menu + game ROMs via "lnx multicart" + lynxdir
#
# See each subdir's Makefile, design/LYNX_SPRITE_SHEET_DESIGN.md,
# design/LYNX_MULTICART_DESIGN.md, design/LYNX_HANDYMUSIC_DESIGN.md and
# doc/sp65.html, doc/sound.html, doc/sprites.html, doc/sprpck.html.

$(SUBDIR_EXAMPLES):
	@$(MAKE) --no-print-directory -C $@ clean all

# Per-subdir clean goal: "games/invaders.clean" cleans games/invaders.
$(SUBDIR_CLEAN): %.clean:
	@$(MAKE) --no-print-directory -C $* clean 2>$(NULLDEV) || true

# --------------------------------------------------------------------------

examples: $(EXELIST) $(SUBDIR_EXAMPLES)

# The top-level Makefile invokes "make -C examples all". Always rebuild the
# examples from scratch here so that a changed toolchain, library or config is
# guaranteed to be re-exercised (the .lnx files carry no dependency on the
# library, so an incremental build would otherwise leave them stale).
all:
	@$(MAKE) --no-print-directory mostlyclean
	@$(MAKE) --no-print-directory examples

# --------------------------------------------------------------------------
# Installation rules

INSTALL = install
examplesdir = $(PREFIX)/share/cc65/examples

install:
	$(if $(PREFIX),,$(error variable "PREFIX" must be set))
	$(INSTALL) -d $(DESTDIR)$(examplesdir)
	$(INSTALL) -m0644 Makefile $(addsuffix .c,$(ROOT_EXAMPLES)) $(DESTDIR)$(examplesdir)
	$(INSTALL) -d $(DESTDIR)$(examplesdir)/games
	$(INSTALL) -m0644 $(addprefix games/,$(addsuffix .c,$(GAMES))) $(DESTDIR)$(examplesdir)/games
	$(INSTALL) -d $(DESTDIR)$(examplesdir)/suzy
	$(INSTALL) -m0644 $(addprefix suzy/,$(addsuffix .c,$(SUZY))) $(DESTDIR)$(examplesdir)/suzy
	$(INSTALL) -d $(DESTDIR)$(examplesdir)/mikey
	$(INSTALL) -m0644 $(addprefix mikey/,$(addsuffix .c,$(MIKEY))) $(DESTDIR)$(examplesdir)/mikey
	$(INSTALL) -d $(DESTDIR)$(examplesdir)/memory
	$(INSTALL) -m0644 $(addprefix memory/,$(addsuffix .c,$(MEMORY))) $(DESTDIR)$(examplesdir)/memory
	$(INSTALL) -d $(DESTDIR)$(examplesdir)/network
	$(INSTALL) -m0644 $(addprefix network/,$(addsuffix .c,$(NETWORK))) $(DESTDIR)$(examplesdir)/network
	$(INSTALL) -d $(DESTDIR)$(examplesdir)/sdcard-gd
	$(INSTALL) -m0644 $(addprefix sdcard-gd/,$(addsuffix .c,$(SDCARD_GD))) $(DESTDIR)$(examplesdir)/sdcard-gd
	# Multi-file examples: install each subdir's Makefile and committed sources.
	$(INSTALL) -d $(DESTDIR)$(examplesdir)/games/invaders
	$(INSTALL) -m0644 games/invaders/Makefile games/invaders/invaders.c games/invaders/invtheme.s $(DESTDIR)$(examplesdir)/games/invaders
	$(INSTALL) -d $(DESTDIR)$(examplesdir)/games/lander
	$(INSTALL) -m0644 games/lander/Makefile games/lander/lander.c games/lander/lander.pcx games/lander/lander.pcx.py games/lander/flame.pcx games/lander/solidsq.pcx $(DESTDIR)$(examplesdir)/games/lander
	$(INSTALL) -m0644 games/lander/landermenu.abc games/lander/landergame.abc games/lander/landerbass.abc games/lander/landermenu.s games/lander/landergame.s games/lander/landerbass.s $(DESTDIR)$(examplesdir)/games/lander
	$(INSTALL) -d $(DESTDIR)$(examplesdir)/suzy/spritetest
	$(INSTALL) -m0644 suzy/spritetest/Makefile suzy/spritetest/spritetest.c suzy/spritetest/heart.pcx suzy/spritetest/heart.pcx.py $(DESTDIR)$(examplesdir)/suzy/spritetest
	$(INSTALL) -d $(DESTDIR)$(examplesdir)/suzy/spritesheet
	$(INSTALL) -m0644 suzy/spritesheet/Makefile suzy/spritesheet/spritesheet.c suzy/spritesheet/sheet.pcx suzy/spritesheet/sheet.pcx.py $(DESTDIR)$(examplesdir)/suzy/spritesheet
	$(INSTALL) -d $(DESTDIR)$(examplesdir)/suzy/spriteslice
	$(INSTALL) -m0644 suzy/spriteslice/Makefile suzy/spriteslice/spriteslice.c suzy/spriteslice/sheet.pcx suzy/spriteslice/sheet.pcx.py $(DESTDIR)$(examplesdir)/suzy/spriteslice
	$(INSTALL) -d $(DESTDIR)$(examplesdir)/suzy/spritefx
	$(INSTALL) -m0644 suzy/spritefx/Makefile suzy/spritefx/spritefx.c suzy/spritefx/spritefx_shapes.py suzy/spritefx/cube.pcx suzy/spritefx/crystal.pcx suzy/spritefx/star.pcx suzy/spritefx/ball.pcx suzy/spritefx/marker.pcx $(DESTDIR)$(examplesdir)/suzy/spritefx
	$(INSTALL) -d $(DESTDIR)$(examplesdir)/suzy/sprpcktest
	$(INSTALL) -m0644 suzy/sprpcktest/Makefile suzy/sprpcktest/sprpcktest.c suzy/sprpcktest/sprpckdata.s suzy/sprpcktest/logo.bmp suzy/sprpcktest/logo.bmp.py suzy/sprpcktest/icon.sps $(DESTDIR)$(examplesdir)/suzy/sprpcktest
	$(INSTALL) -d $(DESTDIR)$(examplesdir)/mikey/sndtune
	$(INSTALL) -m0644 mikey/sndtune/Makefile mikey/sndtune/sndtune.c mikey/sndtune/theme.abc mikey/sndtune/theme.s $(DESTDIR)$(examplesdir)/mikey/sndtune
	$(INSTALL) -d $(DESTDIR)$(examplesdir)/mikey/sfxtest
	$(INSTALL) -m0644 mikey/sfxtest/Makefile mikey/sfxtest/sfxtest.c mikey/sfxtest/sfx.s $(DESTDIR)$(examplesdir)/mikey/sfxtest
	$(INSTALL) -d $(DESTDIR)$(examplesdir)/mikey/handymusic
	$(INSTALL) -m0644 mikey/handymusic/Makefile mikey/handymusic/handymusic.c mikey/handymusic/hmdemo.s $(DESTDIR)$(examplesdir)/mikey/handymusic
	$(INSTALL) -m0644 mikey/handymusic/instr.txt mikey/handymusic/sfx.txt mikey/handymusic/track1.txt mikey/handymusic/track2.txt mikey/handymusic/track3.txt mikey/handymusic/track4.txt $(DESTDIR)$(examplesdir)/mikey/handymusic
	$(INSTALL) -d $(DESTDIR)$(examplesdir)/multicart
	$(INSTALL) -m0644 multicart/Makefile multicart/README.md multicart/menu.c multicart/game1.c multicart/game2.c multicart/game3.c $(DESTDIR)$(examplesdir)/multicart

# --------------------------------------------------------------------------
# Packaging rules

zip:
	@cd .. && zip -r cc65 examples/

# --------------------------------------------------------------------------
# Clean-up rules

# The flat examples' build products (the multi-file examples clean themselves
# through their own Makefiles - see $(SUBDIR_CLEAN)).
mostlyclean:
	@$(DEL) $(EXELIST:.lnx=.map) $(EXELIST:.lnx=.o) $(EXELIST:.lnx=.s) 2>$(NULLDEV)

clean: mostlyclean $(SUBDIR_CLEAN)
	@$(DEL) $(EXELIST) 2>$(NULLDEV)
