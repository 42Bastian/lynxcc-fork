/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in this directory.
*/

/*
** fonttest.c - Demonstrates the compact 5x5 Lynx graphics font on the Atari Lynx.
**
** The compact font (GFX_FONT_COMPACT, see design/LYNX_GFX_FONT5X5_DESIGN.md) packs
** glyphs at a 6-px pitch with a transparent background; the foreground is
** drawn in the current pen (gfx_setcolor). The whole screen is cleared to
** blue so the transparency is obvious - the text floats directly on blue
** with no background box. Suzy scales the text sprite natively in 8.8, so
** gfx_settextscale works for this font too; the SCALE line is drawn at
** 1x / 2x / 3x.
**
** Controls: A cycles the scale (1x / 2x / 3x).
**
** Build:  cl65 -Ors -o fonttest.lnx fonttest.c
*/

#include <lynx/lynx.h>
#include <lynx/gfx.h>
#include <lynx/joystick.h>
#include <6502.h>

static const char* const banner = "ABCDEFGHIJKLMNOP";
static const char* const digits = "0123456789 .,!?";

void main (void)
{
    unsigned char scale = 1;            /* 1, 2 or 3                       */
    unsigned char joy, prev = 0, pressed;
    unsigned char tick = 0;

    gfx_init ();
    CLI ();
    while (gfx_busy ()) {}
    gfx_setpalette (gfx_getdefpalette ());
    gfx_setframerate (60);
    gfx_setcollisiondetection (0);

    /* The compact font is the only font this program uses. */
    gfx_setfont (GFX_FONT_COMPACT);

    for (;;) {
        joy     = joy_read ();
        pressed = joy & (unsigned char)~prev;
        prev    = joy;

        if (pressed & JOY_BTN_1_MASK) {
            if (++scale > 3) scale = 1;
        }

        while (gfx_busy ()) {}

        /* Blue background so the transparent glyph background is visible. */
        gfx_setcolor (COLOR_BLUE);
        gfx_clear ();

        gfx_setcolor (COLOR_WHITE);
        gfx_outtextxy (4, 4, "5X5 COMPACT FONT");

        gfx_setcolor (COLOR_WHITE);
        gfx_outtextxy (4, 20, banner);
        gfx_setcolor (COLOR_RED);
        gfx_outtextxy (4, 28, digits);
        gfx_setcolor (COLOR_GREEN);
        gfx_outtextxy (4, 36, "TRANSPARENT BG");

        /* Scaled text: 1x, 2x or 3x via Suzy 8.8 scaling. */
        gfx_settextscale ((unsigned)scale << 8, (unsigned)scale << 8);
        gfx_setcolor (COLOR_WHITE);
        gfx_outtextxy (4, 60, "SCALE");
        gfx_settextscale (0x100, 0x100);

        /* Footer hint, blinking. */
        if (tick & 0x20) {
            gfx_setcolor (COLOR_WHITE);
            gfx_outtextxy (4, 92, "A = SCALE");
        }
        ++tick;

        gfx_updatedisplay ();
    }
}
