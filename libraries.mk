# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
#
# Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.

# libraries.mk - Lynx Game Development SDK runtime + libraries build
#
# Builds the C runtime, startup glue and the SDK libraries into the partitioned
# archives in lib/ (design/LYNX_SDK_LAYOUT_DESIGN.md sec. 6):
#
#   lib/lynx.lib            core, always linked: runtime/rt + runtime/lynx +
#                           libraries/core + libraries/libc
#   lib/lynx-graphics.lib   opt-in: libraries/graphics (Lynx graphics + fonts)
#   lib/lynx-audio.lib      opt-in: libraries/audio (Mikey sound)
#   lib/lynx-math.lib       opt-in: libraries/math (Suzy hw mul/div + async)
#   lib/lynx-compress.lib   opt-in: libraries/compress (zlib + lz4)
#   lib/lynx-sdcard-gd.lib  opt-in: libraries/sdcard-gd (RetroHQ SD/GD cart)
#
# Run from the repo root (the CC65_HOME directory); the top-level Makefile
# invokes it as "make -f libraries.mk".  Replaces the old single-target
# libsrc/Makefile, which produced one monolithic lynx.lib.

ifneq ($(shell echo),)
  CMD_EXE = 1
endif

.PHONY: all mostlyclean clean install zip lib dirs
.SUFFIXES:

ifdef CMD_EXE
  DIRLIST = $(strip $(foreach dir,$1,$(wildcard $(dir))))
  MKDIR = $(if $(wildcard $1),,mkdir $(subst /,\,$1))
  RMDIR = $(if $(DIRLIST),rmdir /s /q $(subst /,\,$(DIRLIST)))
else
  MKDIR = mkdir -p $1
  RMDIR = $(RM) -r $1
endif

TARGET = lynx
WRK    = libwrk/$(TARGET)

CA65FLAGS =
CC65FLAGS = -Or -W error

AR65 := $(if $(wildcard bin/ar65*),bin/ar65,ar65)
CA65 := $(if $(wildcard bin/ca65*),bin/ca65,ca65)
CC65 := $(if $(wildcard bin/cc65*),bin/cc65,cc65)
LD65 := $(if $(wildcard bin/ld65*),bin/ld65,ld65)

export CC65_HOME := $(abspath .)

# --------------------------------------------------------------------------
# Source-tree groups -> output archive (design sec. 6.1).  "core" folds the
# runtime helpers, Lynx startup glue, base platform and C standard library into
# the always-linked lynx.lib; each optional subsystem is its own archive.

CORE_DIRS     = runtime/rt runtime/lynx libraries/core libraries/libc
GRAPHICS_DIRS = libraries/graphics
AUDIO_DIRS    = libraries/audio libraries/audio/sfx
MATH_DIRS     = libraries/math
COMPRESS_DIRS = libraries/compress
SDCARD_GD_DIRS = libraries/sdcard-gd

ALL_DIRS = $(CORE_DIRS) $(GRAPHICS_DIRS) $(AUDIO_DIRS) $(MATH_DIRS) $(COMPRESS_DIRS) \
           $(SDCARD_GD_DIRS)

vpath %.s $(ALL_DIRS)
vpath %.c $(ALL_DIRS)

# Object basenames are globally unique across the source tree, so every group's
# objects live flat in $(WRK) and are sorted into archives by membership.
objs = $(addprefix $(WRK)/,$(sort $(notdir \
         $(patsubst %.s,%.o,$(wildcard $(foreach d,$1,$d/*.s))) \
         $(patsubst %.c,%.o,$(wildcard $(foreach d,$1,$d/*.c))))))

# multicartldr.o is excluded: runtime/lynx/multicartldr.s is the standalone
# SOURCE for the relocatable multicart loader, linked on its own at $0040 only
# to regenerate the committed blob libraries/core/multicartldr_gen.s (via
# tools/lnx/gen-multicartldr.sh). It must not be archived into the core library.
CORE_OBJS     = $(filter-out $(WRK)/multicartldr.o,$(call objs,$(CORE_DIRS)))
GRAPHICS_OBJS = $(call objs,$(GRAPHICS_DIRS))
AUDIO_OBJS    = $(call objs,$(AUDIO_DIRS))
MATH_OBJS     = $(call objs,$(MATH_DIRS))
COMPRESS_OBJS = $(call objs,$(COMPRESS_DIRS))
SDCARD_GD_OBJS = $(call objs,$(SDCARD_GD_DIRS))

OBJS = $(CORE_OBJS) $(GRAPHICS_OBJS) $(AUDIO_OBJS) $(MATH_OBJS) $(COMPRESS_OBJS) \
       $(SDCARD_GD_OBJS)
DEPS = $(OBJS:.o=.d)

LIBS = lib/lynx.lib          \
       lib/lynx-graphics.lib \
       lib/lynx-audio.lib    \
       lib/lynx-math.lib     \
       lib/lynx-compress.lib \
       lib/lynx-sdcard-gd.lib

# The cl65 auto-library manifest (design sec. 6.6): one archive per line, in the
# order cl65 hands them to ld65 — dependents first, core (lynx.lib) last — so a
# single in-order library pass resolves every cross-reference. cl65 reads this
# from lib/ (found via the same CC65_HOME/lib + WinBin search ld65 uses) and
# appends the listed archives instead of only lynx.lib; ld65 extracts only the
# referenced modules, so unused subsystems cost nothing. Keep this order in step
# with the cl65 link order and the table in
# design/LYNX_CL65_AUTOLIBS_DESIGN.md.
MANIFEST = lib/lynx-sdklibs.list

# --------------------------------------------------------------------------

all lib: $(LIBS) $(MANIFEST)

mostlyclean:
	$(call RMDIR,libwrk)

clean:
	$(call RMDIR,libwrk lib)

# --------------------------------------------------------------------------
# Compilation

define ASSEMBLE_recipe

$(if $(QUIET),,@echo $(TARGET) - $<)
@$(CA65) $(CA65FLAGS) --create-dep $(@:.o=.d) -o $@ $<

endef # ASSEMBLE_recipe

define COMPILE_recipe

$(if $(QUIET),,@echo $(TARGET) - $<)
@$(CC65) $(CC65FLAGS) --create-dep $(@:.o=.d) --dep-target $@ -o $(@:.o=.s) $<
@$(CA65) -o $@ $(@:.o=.s)

endef # COMPILE_recipe

$(WRK)/%.o: %.s | dirs
	$(ASSEMBLE_recipe)

$(WRK)/%.o: %.c | dirs
	$(COMPILE_recipe)

# --------------------------------------------------------------------------
# Archiving.  Each archive is rebuilt in full from its complete object set, so
# a removed source can never leave a stale member behind (no manual "ar65 d"),
# matching the project's always-full-rebuild discipline.

define ARCHIVE_recipe

@$(RM) $@
$(AR65) a $@ $(filter %.o,$^)

endef # ARCHIVE_recipe

lib/lynx.lib: $(CORE_OBJS) | dirs
	$(ARCHIVE_recipe)

lib/lynx-graphics.lib: $(GRAPHICS_OBJS) | dirs
	$(ARCHIVE_recipe)

lib/lynx-audio.lib: $(AUDIO_OBJS) | dirs
	$(ARCHIVE_recipe)

lib/lynx-math.lib: $(MATH_OBJS) | dirs
	$(ARCHIVE_recipe)

lib/lynx-compress.lib: $(COMPRESS_OBJS) | dirs
	$(ARCHIVE_recipe)

lib/lynx-sdcard-gd.lib: $(SDCARD_GD_OBJS) | dirs
	$(ARCHIVE_recipe)

# Regenerate the manifest whenever this Makefile (its source of truth) changes.
# Written with one echo per line rather than make's $(file ...) function, which
# needs GNU make 4.0+ — the macOS system make is still 3.81, where $(file ...)
# silently writes nothing and cl65 would then fall back to linking only
# lynx.lib (leaving e.g. the Lynx graphics symbols unresolved). No space before
# ">>" so Windows cmd does not append a trailing blank to each archive name.
$(MANIFEST): libraries.mk | dirs
	$(if $(QUIET),,@echo $(TARGET) - $@)
	@$(RM) $@
	@echo lynx-graphics.lib>>$@
	@echo lynx-audio.lib>>$@
	@echo lynx-compress.lib>>$@
	@echo lynx-math.lib>>$@
	@echo lynx-sdcard-gd.lib>>$@
	@echo lynx.lib>>$@

dirs:
	@$(call MKDIR,lib)
	@$(call MKDIR,$(WRK))

# --------------------------------------------------------------------------
# Installation / packaging.  The four data directories the binaries hard-code
# (include, asminc, cfg, lib) plus their lynx/ subdirs are copied verbatim;
# full release/install packaging is handled by the make install/zip targets
# (design sec. 11).

datadir = $(PREFIX)/share/cc65

OUTPUTDIRS := lib    \
              cfg    \
              asminc \
              include \
              $(filter-out $(wildcard asminc/*.*),$(wildcard asminc/*)) \
              $(filter-out $(wildcard include/*.*),$(wildcard include/*))

ifdef CMD_EXE

install:

else # CMD_EXE

INSTALL = install

define INSTALL_recipe

$(if $(PREFIX),,$(error variable "PREFIX" must be set))
$(INSTALL) -d $(DESTDIR)$(datadir)/$(dir)
$(INSTALL) -m0644 $(dir)/*.* $(DESTDIR)$(datadir)/$(dir)

endef # INSTALL_recipe

install: $(LIBS)
	$(foreach dir,$(OUTPUTDIRS),$(INSTALL_recipe))

endif # CMD_EXE

define ZIP_recipe

@zip cc65 $(dir)/*.*

endef # ZIP_recipe

zip:
	$(foreach dir,$(OUTPUTDIRS),$(ZIP_recipe))

-include $(DEPS)
