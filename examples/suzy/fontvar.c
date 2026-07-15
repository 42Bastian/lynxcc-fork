/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in this directory.
*/

/*
** fontvar.c - Demonstrates the proportional (variable-width) Lynx graphics font on the
** Atari Lynx.
**
** GFX_FONT_VARIABLE (see design/LYNX_GFX_FONTVAR_DESIGN.md) is an all-caps
** pixel font whose glyphs are 1..5 px wide with a 1-px inter-glyph gap, so
** every character advances the cursor by its own width: an 'I' is tight, an
** 'M' is wide. Like the compact 5x5 font it has a transparent background and
** draws in the current pen (gfx_setcolor), so the text floats directly on the
** blue screen with no background box. The font was recovered from the
** "EGGSAVIER" intro screen, reproduced here as the BANNER line.
**
** Because the spacing is proportional, gfx_gettextwidth sums each glyph's
** advance instead of strlen*pitch. The demo draws a rule exactly under the
** measured width of a string to prove the query and the drawn glyphs agree,
** and centres the title using the same measurement. Suzy scales the text
** sprite natively in 8.8, so glyphs and spacing scale together (the SCALE line
** is drawn at 1x / 2x / 3x).
**
** Controls:
**   A  cycles the SCALE line (1x / 2x / 3x).
**   B  cycles the FONT used for the sample block (proportional / 5x5 / 8x8),
**      showing gfx_setfont toggling cleanly among all three fonts.
**
** Build:  cl65 -Ors -o fontvar.lnx fontvar.c
*/

#include <lynx/lynx.h>
#include <lynx/gfx.h>
#include <lynx/joystick.h>
#include <6502.h>

static const char* const banner  = "EGGSAVIER";
static const char* const alpha1  = "ABCDEFGHIJKLM";
static const char* const alpha2  = "NOPQRSTUVWXYZ";
static const char* const digits  = "0123456789 .,!?";
static const char* const proverb = "MIMI IN A WILLOW";

/* The three fonts the B button cycles through, with a label for each. */
static const unsigned char fonts[3] = {
    GFX_FONT_VARIABLE, GFX_FONT_COMPACT, GFX_FONT_BITMAP
};
static const char* const labels[3] = {
    "PROPORTIONAL", "5X5 COMPACT", "8X8 SYSTEM"
};

void main (void)
{
    unsigned char scale = 1;            /* 1, 2 or 3                       */
    unsigned char fsel  = 0;            /* index into fonts[] / labels[]   */
    unsigned char joy, prev = 0, pressed;
    unsigned char tick = 0;
    unsigned       w, cx;

    gfx_init ();
    CLI ();
    while (gfx_busy ()) {}
    gfx_setdefpalette ();
    gfx_setframerate (60);
    gfx_setcollisiondetection (0);

    for (;;) {
        joy     = joy_read ();
        pressed = joy & (unsigned char)~prev;
        prev    = joy;

        if (pressed & JOY_BTN_1_MASK) {
            if (++scale > 3) scale = 1;
        }
        if (pressed & JOY_BTN_2_MASK) {
            if (++fsel > 2) fsel = 0;
        }

        while (gfx_busy ()) {}

        /* Blue background so the transparent glyph background is visible. */
        gfx_setcolor (COLOR_BLUE);
        gfx_clear ();

        /* Title, drawn proportional and centred via gfx_gettextwidth. */
        gfx_setfont (GFX_FONT_VARIABLE);
        w  = gfx_gettextwidth ("PROPORTIONAL FONT");
        cx = (160 - w) >> 1;
        gfx_setcolor (COLOR_WHITE);
        gfx_outtextxy (cx, 3, "PROPORTIONAL FONT");

        /* The recovered EggSavier word in its source colour (yellow). */
        gfx_setcolor (COLOR_YELLOW);
        gfx_outtextxy (4, 14, banner);

        /* Full caps set + digits: proportional spacing is obvious here -
        ** the I and the M in the alphabet line are visibly un-equal. */
        gfx_setcolor (COLOR_WHITE);
        gfx_outtextxy (4, 24, alpha1);
        gfx_outtextxy (4, 32, alpha2);
        gfx_setcolor (COLOR_GREEN);
        gfx_outtextxy (4, 40, digits);

        /* Width-query proof: draw PROVERB, then mark the cursor end with a
        ** caret placed at 4 + gfx_gettextwidth(proverb). The caret must sit
        ** flush against the last glyph + its 1-px gap, confirming the
        ** proportional width query matches the drawn glyphs. */
        gfx_setcolor (COLOR_WHITE);
        gfx_outtextxy (4, 52, proverb);
        w = gfx_gettextwidth (proverb);
        gfx_setcolor (COLOR_RED);
        gfx_outtextxy (4 + w, 52, "<");

        /* Sample block in the currently selected font (B cycles it). */
        gfx_setfont (fonts[fsel]);
        gfx_setcolor (COLOR_WHITE);
        gfx_outtextxy (4, 68, labels[fsel]);
        gfx_outtextxy (4, 78, "THE QUICK BROWN FOX");

        /* Scaled proportional text: 1x / 2x / 3x via Suzy 8.8 scaling. */
        gfx_setfont (GFX_FONT_VARIABLE);
        gfx_settextscale ((unsigned)scale << 8, (unsigned)scale << 8);
        gfx_setcolor (COLOR_GREEN);
        gfx_outtextxy (4, 88, "SCALE");
        gfx_settextscale (0x100, 0x100);

        /* Footer hint, blinking. */
        if (tick & 0x20) {
            gfx_setfont (GFX_FONT_VARIABLE);
            gfx_setcolor (COLOR_WHITE);
            gfx_outtextxy (90, 96, "A SCALE  B FONT");
        }
        ++tick;

        gfx_updatedisplay ();
    }
}
