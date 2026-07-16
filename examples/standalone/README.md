<!--
SPDX-License-Identifier: MIT

Lynx Game Development SDK example, (c) 2026 the lynxcc authors.
Provided under the MIT License; copy it into your own projects freely.
See the LICENSE file in the examples directory.
-->

# Standalone hello-world project

The smallest self-contained project in the tree. Unlike the other samples, it
is **not** built by the shared `examples/` Makefile — it carries its own
one-line `cl65` build rule, so you can copy this directory out of the SDK tree,
rename it, and it still builds on its own.

## Layout

```
standalone/
├── Makefile        # self-contained cl65 build (not driven by examples/Makefile)
├── src/main.c      # init graphics, load the default palette, print HELLO, WORLD!
├── README.md       # this file
├── .gitignore      # ignores the build/ directory
└── build/          # all build outputs land here (created by make)
```

## Building

Make sure the SDK is reachable, then run `make`:

```bash
# Either put the SDK bin/ on PATH ...
export PATH="/path/to/lynx-sdk/bin:$PATH"
# ... or point CC65_HOME at the installed SDK root:
export CC65_HOME=/path/to/lynx-sdk

make            # produces build/hello.lnx (+ build/hello.map)
make clean      # removes the build/ directory
```

Every build product — the object files, the `.map` and the final `.lnx` — is
written under `build/`, so the source tree stays clean. The build is split into
a compile-to-object step and a link step so the intermediates have a home in
`build/`. Because `cl65` resolves the SDK's optional subsystem libraries
automatically (`design/LYNX_CL65_AUTOLIBS_DESIGN.md`), the Lynx graphics code is
linked for you — **no `-l` flags**. `cl65` already targets the Lynx by default.

## What it shows

`src/main.c` is the bare minimum needed to put text on screen:

- the `<lynx/...>` include layout (`<lynx/lynx.h>`, `<lynx/gfx.h>`);
- the static Lynx graphics library — `gfx_init()` with no driver to load;
- loading the built-in default palette with `gfx_setdefpalette()`;
- drawing one frame — `gfx_clear()`, `gfx_outtextxy()`, `gfx_updatedisplay()` —
  and holding it on screen.

`gfx_init()` already loads the default palette and selects black as the drawing
colour, so the explicit `gfx_setdefpalette()` call is redundant here; it is
included to show the call you would use to restore that palette after changing
it.

Run `hello.lnx` on real hardware or an emulator; it shows "HELLO, WORLD!"
centred on a black screen.
