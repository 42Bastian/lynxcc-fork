/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in the examples directory.
*/

/*
** Standalone "hello world" for the Lynx Game Development SDK.
**
** The smallest self-contained project in the tree: it lives outside the shared
** examples/ Makefile and builds itself with its own one-line cl65 rule. All it
** does is
**
**   1. bring up the static Lynx graphics library  (gfx_init),
**   2. load the built-in default palette           (gfx_setdefpalette),
**   3. draw "HELLO, WORLD!" once and hold it on screen.
**
** gfx_init already loads the default palette and picks black as the drawing
** colour; the explicit gfx_setdefpalette() call here is redundant but shows the
** call you would use to restore that palette after changing it. Copy this
** directory out of the SDK tree to start a project from the bare minimum.
**
** Build:  make            (or: cl65 -Ors -o hello.lnx src/main.c)
** Run:    boot hello.lnx on a Lynx or an emulator.
*/

#include <lynx/lynx.h>
#include <lynx/gfx.h>
#include <6502.h>

void main (void)
{
    gfx_init ();                        /* bring up the display (4bpp, page 0) */
    gfx_setdefpalette ();               /* load the built-in default palette   */
    CLI ();                             /* enable IRQs: the frame timer needs them */

    /* Render one frame: black background, white text, then request the swap. */
    while (gfx_busy ()) {}
    gfx_setcolor (COLOR_BLACK);
    gfx_clear ();                       /* gfx_clear fills in the draw colour */
    gfx_setcolor (COLOR_WHITE);
    gfx_outtextxy (GFX_XRES / 2 - 39, GFX_YRES / 2 - 3, "HELLO, WORLD!");
    gfx_updatedisplay ();

    /* Nothing left to do; hold the picture on screen. */
    for (;;) {}
}
