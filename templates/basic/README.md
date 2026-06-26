<!--
SPDX-License-Identifier: MIT

Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
Provided under the MIT License; copy it into your own projects freely.
See the LICENSE file in this directory.
-->

# Basic Lynx SDK project template

A minimal, complete starting point for a new Atari Lynx game built with the
**Lynx Game Development SDK**. Copy this directory out of the SDK tree, rename
it, and start editing `src/main.c`.

## Layout

```
basic/
├── Makefile        # one-step cl65 build; relies on automatic SDK libraries
├── src/main.c      # init graphics, double-buffered main loop, read the joystick
├── README.md       # this file
└── .gitignore      # ignores the build outputs (game.lnx, .map, .o, .s)
```

## Building

Make sure the SDK is reachable, then run `make`:

```bash
# Either put the SDK bin/ on PATH ...
export PATH="/path/to/lynx-sdk/bin:$PATH"
# ... or point CC65_HOME at the installed SDK root:
export CC65_HOME=/path/to/lynx-sdk

make            # produces game.lnx (+ game.map)
make clean
```

The single `cl65` invocation compiles, assembles and links in one step. Because
`cl65` resolves the SDK's optional subsystem libraries automatically
(`design/LYNX_CL65_AUTOLIBS_DESIGN.md`), the graphics, audio, math and
compression code you call is linked for you — **no `-l` flags**. `cl65` already
targets the Lynx by default.

## What it shows

`src/main.c` is the canonical example of the SDK conventions:

- the `<lynx/...>` include layout (`<lynx/lynx.h>`, `<lynx/gfx.h>`,
  `<lynx/joystick.h>`);
- the static Lynx graphics library — `gfx_init()` with no driver to load;
- a double-buffered loop (`gfx_busy()` / `gfx_updatedisplay()`) reading the
  joystick with `joy_read()`.

Run `game.lnx` on real hardware or an emulator; the label "HELLO, LYNX!" moves
with the D-pad.

## Growing the project

Add more `.c`/`.s` files to `SOURCES` in the `Makefile`. To link explicitly
instead of using the automatic libraries, build with `cl65 --no-sdk-libs` and
list the archives you need in `LIBS` (see `design/LYNX_SDK_LAYOUT_DESIGN.md`
sec. 6.4 for the dependency order). Further templates (graphics, audio,
full-game) build on this one.
