# SPDX-License-Identifier: MIT
#
# Host tools for the Lynx Game Development SDK sound engine.  These are built
# with the host C compiler and are intentionally decoupled from the cc65 build
# (design/LYNX_SND_ENGINE_DESIGN.md sec. 7/9): they run before, and independent
# of, the on-target toolchain.
#
#   abccc   ABC tune text -> compiled event stream
#   abcrom  patch a compiled tune into the template .lnx for emulator auditioning
#
# The abcrom template ROM is built separately with the cc65 toolchain; see
# abcrom/template/Makefile.

all clean install:
	$(MAKE) -C abccc  $@
	$(MAKE) -C abcrom $@

# Convenience: (re)build the template ROM with the lynxcc toolchain.
template:
	$(MAKE) -C abcrom/template all

.PHONY: all clean install template
