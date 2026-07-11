# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
#
# Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.

ifneq ($(shell echo),)
  CMD_EXE = 1
endif

# Standalone SDK utilities (not part of the cc65-derived compiler suite).
# Each program is a subdirectory of .c sources; none depend on compiler/common.
# All binaries install into the single root bin/ (LYNX_SDK_LAYOUT_DESIGN.md §2).
#
#   lnx     .lnx header/cart tool
#   abccc   ABC tune text -> compiled event stream (sound engine, MIT)
#   abcrom  patch a compiled tune into the template .lnx for emulator auditioning
#   hmcc    HandyMusic script compiler (instrument/SFX/music .txt -> .mus/.sfx)
#
# abccc/abcrom are host-compiled like the rest of tools/; unlike them, abcrom
# also ships a pre-assembled template ROM (abcrom/template.lnx) that IS built
# with the lynxcc toolchain — rebuilt on demand via the abcrom-template target
# below, not as part of `all` (it needs the compiler + libraries, which build
# after tools/). See design/LYNX_SND_ENGINE_DESIGN.md §7/§9.
#
# hmcc is a first-party portable port of Osman Celimli's HandyMusic "HMCC"
# script compiler (design/LYNX_HANDYMUSIC_DESIGN.md §3).  Like abccc/abcrom it
# is a lynxcc-authored derived work — NOT a tools/extern/ subtree — so it builds
# straight from PROGS with the strict host CFLAGS; its provenance and grant are
# recorded in doc/licenses.html §4.5.
PROGS = lnx abccc abcrom hmcc

# hmcc's note-frequency maths (notes.c) uses pow() from libm; the other PROGS
# link nothing extra.  Per-program LDLIBS keeps that dependency local to hmcc.
hmcc_LDLIBS = -lm

# Vendored third-party host tools.  Each lives in extern/<tool>/ as a pristine
# git subtree that is NEVER modified (see design/LYNX_EXTERN_TOOLS_DESIGN.md and
# extern/README.md).  They build with the rest of tools/ into the shared root
# bin/, but every project-owned bit — this build glue included — stays OUTSIDE
# the vendored tree so a `git subtree pull` never has to touch our files.  Since
# upstream code cannot be edited, each tool declares its own source glob, any
# upstream-required defines, and builds with warnings suppressed (we do not lint
# code we do not own).  Their upstream licenses are recorded in doc/licenses.html.
#
#   sprpck  Lynx sprite packer (Apache-2.0, https://codeberg.org/42Bastian/sprpck)
EXTERN_PROGS = sprpck

sprpck_SRC      = extern/sprpck/src
sprpck_CPPFLAGS = -DUNIX

# Vendored third-party tools written in C++.  Same policy as EXTERN_PROGS, but
# their sources are .cpp and must be compiled and linked with the C++ driver
# ($(CXX)); they use EXTERN_CXX_template below instead of EXTERN_template.  A
# tool's sources may sit at its own repo root, so $($1_SRC) points there and the
# glob is $($1_SRC)/*.cpp.
#
#   lynxdir  Lynx ROM builder (no upstream licence declared — all rights
#            reserved, (c) Björn Spruck 2010-2017; https://github.com/bspruck/lynxdir)
EXTERN_CXX_PROGS = lynxdir

lynxdir_SRC = extern/lynxdir

# All tool binaries (project-authored + vendored, C and C++) share the
# build/install/clean machinery below and the single root bin/.
ALL_PROGS = $(PROGS) $(EXTERN_PROGS) $(EXTERN_CXX_PROGS)

.PHONY: all mostlyclean clean install zip avail unavail bin abcrom-template $(ALL_PROGS)

.SUFFIXES:

bindir := $(PREFIX)/bin

ifdef CMD_EXE
  NULLDEV = nul:
  DIRLIST = $(strip $(foreach dir,$1,$(wildcard $(dir))))
  MKDIR = mkdir $(subst /,\,$1)
  RMDIR = $(if $(DIRLIST),rmdir /s /q $(subst /,\,$(DIRLIST)))
else
  NULLDEV = /dev/null
  MKDIR = mkdir -p $1
  RMDIR = $(RM) -r $1
endif

CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
AR = $(CROSS_COMPILE)ar

ifdef CROSS_COMPILE
  $(info CC: $(CC))
  $(info CXX: $(CXX))
  $(info AR: $(AR))
endif

ifdef USER_CFLAGS
  $(info USER_CFLAGS: $(USER_CFLAGS))
endif

CFLAGS += -MMD -MP -O3 \
          -Wall -Wextra -Wno-char-subscripts $(USER_CFLAGS)

# Vendored third-party sources are compiled but not linted: we cannot fix
# warnings in code we are forbidden to modify, so drop the strict -W flags for
# EXTERN_PROGS while keeping optimisation and dependency generation.
EXTERN_CFLAGS = -MMD -MP -O3 -w $(USER_CFLAGS)

# C++ counterpart of EXTERN_CFLAGS for EXTERN_CXX_PROGS.  Same relaxed policy
# (optimise + generate deps, no linting of code we do not own).
EXTERN_CXXFLAGS = -MMD -MP -O3 -w $(USER_CFLAGS)

ifdef CMD_EXE
  EXE_SUFFIX=.exe
endif

ifdef CROSS_COMPILE
  EXE_SUFFIX=.exe
endif

all bin: $(ALL_PROGS)

# (Re)build the abcrom template ROM with the lynxcc toolchain.  On demand only:
# the committed abcrom/template.lnx is the default and clean does NOT remove it.
abcrom-template:
	$(MAKE) -C abcrom/template all

mostlyclean:
	$(call RMDIR,$(foreach prog,$(ALL_PROGS),../wrk/$(prog)))

clean:
	$(call RMDIR,$(foreach prog,$(ALL_PROGS),../wrk/$(prog)))

ifdef CMD_EXE

install avail unavail:

else # CMD_EXE

INSTALL = install

define AVAIL_recipe

ln -s $(abspath ../bin/$(prog)) /usr/local/bin/$(prog)

endef # AVAIL_recipe

define UNAVAIL_recipe

$(RM) /usr/local/bin/$(prog)

endef # UNAVAIL_recipe

install:
	$(if $(PREFIX),,$(error variable "PREFIX" must be set))
	$(INSTALL) -d $(DESTDIR)$(bindir)
	$(foreach prog,$(ALL_PROGS),$(INSTALL) ../bin/$(prog) $(DESTDIR)$(bindir);)

avail:
	$(foreach prog,$(ALL_PROGS),$(AVAIL_recipe))

unavail:
	$(foreach prog,$(ALL_PROGS),$(UNAVAIL_recipe))

endif # CMD_EXE

zip:
	@cd .. && zip cc65 $(foreach prog,$(ALL_PROGS),bin/$(prog)$(EXE_SUFFIX))

define OBJS_template

$1_OBJS := $$(patsubst %.c,../wrk/$1/%.o,$$(notdir $$(wildcard $1/*.c)))

$$($1_OBJS): | ../wrk/$1

../wrk/$1:
	@$$(call MKDIR,$$@)

DEPS += $$($1_OBJS:.o=.d)

endef # OBJS_template

define PROG_template

$$(eval $$(call OBJS_template,$1))

../bin/$1$(EXE_SUFFIX): $$($1_OBJS) | ../bin
	$$(CC) $$(LDFLAGS) -o $$@ $$^ $$(LDLIBS) $$($1_LDLIBS)

$1: ../bin/$1$(EXE_SUFFIX)

../wrk/$1/%.o: $1/%.c
	@echo $$<
	@$$(CC) -c $$(CFLAGS) -o $$@ $$<

endef # PROG_template

../bin:
	@$(call MKDIR,$@)

# Vendored third-party tools: same shape as PROG_template, but sources come from
# the tool's own (unmodifiable) layout via $($1_SRC), compiled with the relaxed
# EXTERN_CFLAGS plus the tool's own $($1_CPPFLAGS).  Object files still land in
# ../wrk/$1 and the binary in the shared ../bin, so clean/install/zip treat
# vendored tools identically to project-authored ones.
define EXTERN_OBJS_template

$1_OBJS := $$(patsubst %.c,../wrk/$1/%.o,$$(notdir $$(wildcard $$($1_SRC)/*.c)))

$$($1_OBJS): | ../wrk/$1

../wrk/$1:
	@$$(call MKDIR,$$@)

DEPS += $$($1_OBJS:.o=.d)

endef # EXTERN_OBJS_template

define EXTERN_template

$$(eval $$(call EXTERN_OBJS_template,$1))

../bin/$1$(EXE_SUFFIX): $$($1_OBJS) | ../bin
	$$(CC) $$(LDFLAGS) -o $$@ $$^ $$(LDLIBS)

$1: ../bin/$1$(EXE_SUFFIX)

../wrk/$1/%.o: $$($1_SRC)/%.c
	@echo $$<
	@$$(CC) -c $$(EXTERN_CFLAGS) $$($1_CPPFLAGS) -o $$@ $$<

endef # EXTERN_template

# Vendored third-party tools written in C++: same shape as EXTERN_template but
# sources are globbed as $($1_SRC)/*.cpp and both compiled and linked with the
# C++ driver ($(CXX)) using EXTERN_CXXFLAGS.  Objects still land in ../wrk/$1 and
# the binary in the shared ../bin, so clean/install/zip treat them identically.
define EXTERN_CXX_OBJS_template

$1_OBJS := $$(patsubst %.cpp,../wrk/$1/%.o,$$(notdir $$(wildcard $$($1_SRC)/*.cpp)))

$$($1_OBJS): | ../wrk/$1

../wrk/$1:
	@$$(call MKDIR,$$@)

DEPS += $$($1_OBJS:.o=.d)

endef # EXTERN_CXX_OBJS_template

define EXTERN_CXX_template

$$(eval $$(call EXTERN_CXX_OBJS_template,$1))

../bin/$1$(EXE_SUFFIX): $$($1_OBJS) | ../bin
	$$(CXX) $$(LDFLAGS) -o $$@ $$^ $$(LDLIBS)

$1: ../bin/$1$(EXE_SUFFIX)

../wrk/$1/%.o: $$($1_SRC)/%.cpp
	@echo $$<
	@$$(CXX) -c $$(EXTERN_CXXFLAGS) $$($1_CPPFLAGS) -o $$@ $$<

endef # EXTERN_CXX_template

$(foreach prog,$(PROGS),$(eval $(call PROG_template,$(prog))))
$(foreach prog,$(EXTERN_PROGS),$(eval $(call EXTERN_template,$(prog))))
$(foreach prog,$(EXTERN_CXX_PROGS),$(eval $(call EXTERN_CXX_template,$(prog))))

-include $(DEPS)
