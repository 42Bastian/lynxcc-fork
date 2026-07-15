/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in this directory.
*/

/*
** palette.c - Load one palette two ways on the Atari Lynx.
**
** The palette generator (doc/palgen.html) emits TWO C arrays for the same
** sixteen colours, and the graphics library has a loader for each:
**
**   gfx_setpalette(pal32)    takes the 32-byte GCOLMAP wire format: 16 green
**                            bytes, then 16 blue/red bytes.
**   gfx_setpalette16(pal16)  takes the condensed 16-entry form: one unsigned
**                            short per pen, each a packed 12-bit colour of the
**                            form 0x0GBR (green nibble in bits 8-11, blue in
**                            4-7, red in 0-3). It splits each entry into the
**                            two GCOLMAP halves as it loads.
**
** pal16[] below is exactly what palgen prints as its "gfx_setpalette16()
** array"; pal32[] is the "gfx_setpalette() array" for the very same sixteen
** colours. Because both describe the same palette, loading either one leaves
** the hardware palette identical - the demo proves it: it installs pal32,
** snapshots GCOLMAP with gfx_getpalette(), installs pal16, snapshots again,
** and compares the two 32-byte snapshots. The result (MATCH / DIFFER) is
** shown on screen.
**
** The sixteen colours are drawn as horizontal bands, one per pen, so you can
** see them. Press A to toggle which loader is currently live: the bands do
** not change, because the two arrays install the same colours.
**
** Build:  cl65 -Ors -o palette.lnx palette.c
*/

#include <lynx/lynx.h>
#include <lynx/gfx.h>
#include <lynx/joystick.h>
#include <6502.h>
#include <string.h>

/* The same sixteen colours in the two layouts palgen emits. */

/* "gfx_setpalette16() array" - one packed 0x0GBR short per pen. */
static const unsigned short pal16[16] = {
    0x0000, 0x000F, 0x0F00, 0x00F0,   /* black, red, green, blue   */
    0x0F0F, 0x00FF, 0x0FF0, 0x0FFF,   /* yellow, magenta, cyan, wht */
    0x0008, 0x0800, 0x0080, 0x0888,   /* dark r/g/b, grey          */
    0x080F, 0x0F08, 0x0F80, 0x0444    /* orange, spring, teal, dk  */
};

/* "gfx_setpalette() array" - 16 green bytes, then 16 blue/red bytes. It is
** pal16 split into its two GCOLMAP halves (high byte = green, low = blue/red). */
static const unsigned char pal32[32] = {
    /* green */
    0x00, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x0F,
    0x00, 0x08, 0x00, 0x08, 0x08, 0x0F, 0x0F, 0x04,
    /* blue + red */
    0x00, 0x0F, 0x00, 0xF0, 0x0F, 0xFF, 0xF0, 0xFF,
    0x08, 0x00, 0x80, 0x88, 0x0F, 0x08, 0x80, 0x44
};

/* Draw the sixteen pens as stacked horizontal bands. */
static void draw_bands (void)
{
    unsigned char i;
    for (i = 0; i < 16; ++i) {
        gfx_setcolor (i);
        gfx_clearrows (12 + i * 5, 5);      /* rows 12..91, 5 px each */
    }
}

void main (void)
{
    unsigned char snap32[32], snap16[32];
    unsigned char match;
    unsigned char live = 0;                 /* 0 = pal32 live, 1 = pal16 */
    unsigned char joy, prev = 0, pressed;

    gfx_init ();
    CLI ();
    while (gfx_busy ()) {}
    gfx_setframerate (60);
    gfx_setcollisiondetection (0);

    /* Install each array in turn and snapshot the resulting hardware palette,
    ** then compare: the two loaders must leave GCOLMAP identical. */
    gfx_setpalette (pal32);
    memcpy (snap32, gfx_getpalette (), 32);
    gfx_setpalette16 (pal16);
    memcpy (snap16, gfx_getpalette (), 32);
    match = (memcmp (snap32, snap16, 32) == 0);

    for (;;) {
        joy     = joy_read ();
        pressed = joy & (unsigned char)~prev;
        prev    = joy;

        if (pressed & JOY_BTN_1_MASK) {
            live = !live;
        }

        /* Load the currently selected array every frame so the A button
        ** actually re-installs the palette by the chosen method. */
        if (live) {
            gfx_setpalette16 (pal16);
        } else {
            gfx_setpalette (pal32);
        }

        while (gfx_busy ()) {}

        gfx_setcolor (COLOR_BLACK);
        gfx_clear ();
        draw_bands ();

        gfx_setcolor (COLOR_WHITE);
        gfx_outtextxy (4, 2,
                       live ? "LOADER: SETPALETTE16" : "LOADER: SETPALETTE");
        gfx_outtextxy (4, 94, match ? "SNAPSHOTS: MATCH"
                                    : "SNAPSHOTS: DIFFER");

        gfx_updatedisplay ();
    }
}
