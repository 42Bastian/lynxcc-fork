# Makefile for a Lynx Game Development SDK project (the "basic" template).
#
# Requires GNU make and the lynxcc SDK: either put its bin/ on your PATH, or set
# CC65_HOME to an installed SDK root (the dir holding bin/ include/ lib/ ...).
#
# Linking uses cl65's automatic SDK-library resolution
# (design/LYNX_CL65_AUTOLIBS_DESIGN.md): whichever of the graphics, audio, math
# and compression subsystems the program references are pulled in for you, so no
# -l flags are needed here. cl65 already targets the Lynx by default. If you ever
# link the explicit way (cl65 --no-sdk-libs), name extra archives in LIBS.

PROGRAM = game
SOURCES = src/main.c

ifdef CC65_HOME
  CL = $(CC65_HOME)/bin/cl65
else
  CL = cl65
endif

CFLAGS  = -Ors --codesize 500
LDFLAGS =
LIBS    =

# One cl65 invocation compiles, assembles and links the whole program and emits
# a ready-to-run .lnx plus a .map.
$(PROGRAM).lnx: $(SOURCES)
	$(CL) $(CFLAGS) $(LDFLAGS) -o $@ -m $(PROGRAM).map $(SOURCES) $(LIBS)

.PHONY: all clean
all: $(PROGRAM).lnx

clean:
	$(RM) $(PROGRAM).lnx $(PROGRAM).map src/*.o src/*.s
