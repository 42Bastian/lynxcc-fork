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
PROGS = lnx

.PHONY: all mostlyclean clean install zip avail unavail bin $(PROGS)

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
AR = $(CROSS_COMPILE)ar

ifdef CROSS_COMPILE
  $(info CC: $(CC))
  $(info AR: $(AR))
endif

ifdef USER_CFLAGS
  $(info USER_CFLAGS: $(USER_CFLAGS))
endif

CFLAGS += -MMD -MP -O3 \
          -Wall -Wextra -Wno-char-subscripts $(USER_CFLAGS)

ifdef CMD_EXE
  EXE_SUFFIX=.exe
endif

ifdef CROSS_COMPILE
  EXE_SUFFIX=.exe
endif

all bin: $(PROGS)

mostlyclean:
	$(call RMDIR,$(foreach prog,$(PROGS),../wrk/$(prog)))

clean:
	$(call RMDIR,$(foreach prog,$(PROGS),../wrk/$(prog)))

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
	$(foreach prog,$(PROGS),$(INSTALL) ../bin/$(prog) $(DESTDIR)$(bindir);)

avail:
	$(foreach prog,$(PROGS),$(AVAIL_recipe))

unavail:
	$(foreach prog,$(PROGS),$(UNAVAIL_recipe))

endif # CMD_EXE

zip:
	@cd .. && zip cc65 $(foreach prog,$(PROGS),bin/$(prog)$(EXE_SUFFIX))

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
	$$(CC) $$(LDFLAGS) -o $$@ $$^ $$(LDLIBS)

$1: ../bin/$1$(EXE_SUFFIX)

../wrk/$1/%.o: $1/%.c
	@echo $$<
	@$$(CC) -c $$(CFLAGS) -o $$@ $$<

endef # PROG_template

../bin:
	@$(call MKDIR,$@)

$(foreach prog,$(PROGS),$(eval $(call PROG_template,$(prog))))

-include $(DEPS)
