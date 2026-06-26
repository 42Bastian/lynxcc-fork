/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in this directory.
*/

/*
** zeropage.c - Demonstrates placing C variables in the zero page on the
** Atari Lynx with the __zeropage attribute (see <zeropage.h> and the
** "Variable storage" chapter of doc/cc65.html).
**
** The three counters below are declared __zeropage, so the compiler emits
** them into the built-in ZEROPAGE segment and the 6502 addresses them with
** the fast 2-byte zero-page addressing modes instead of 3-byte absolute
** ones. Functionally they behave exactly like ordinary globals; the only
** difference is where they live and how they are addressed. Compare the
** generated code with and without the attribute:
**
**     cl65 -S -Ors -o zeropage.s zeropage.c   (then read zeropage.s)
**
** Controls: A counts the "presses" counter up, B counts it down. The frame
** counter advances on its own.
**
** Build:  cl65 -Ors -o zeropage.lnx zeropage.c
*/

#include <lynx/lynx.h>
#include <lynx/gfx.h>
#include <lynx/joystick.h>
#include <6502.h>
#include <zeropage.h>

/* These three live in the zero page. */
unsigned char  frame   __zeropage;      /* free-running frame counter */
unsigned char  presses __zeropage;      /* bumped by the A / B buttons */
unsigned int   ticks   __zeropage;      /* 16-bit running total        */

/* Render an unsigned byte as a fixed 3-digit decimal string. */
static void byte2dec (unsigned char v, char* out)
{
    out[0] = '0' + (v / 100);
    out[1] = '0' + ((v / 10) % 10);
    out[2] = '0' + (v % 10);
    out[3] = '\0';
}

void main (void)
{
    unsigned char joy, prev = 0, pressed;
    char buf[4];

    gfx_init ();
    CLI ();
    while (gfx_busy ()) {}
    gfx_setpalette (gfx_getdefpalette ());
    gfx_setframerate (60);
    gfx_setcollisiondetection (0);
    gfx_setfont (GFX_FONT_COMPACT);

    for (;;) {
        joy     = joy_read ();
        pressed = joy & (unsigned char)~prev;
        prev    = joy;

        if (pressed & JOY_BTN_1_MASK) {
            ++presses;                  /* inc <zp>  */
        }
        if (pressed & JOY_BTN_2_MASK) {
            --presses;                  /* dec <zp>  */
        }

        ++frame;                        /* inc <zp>  */
        ++ticks;                        /* 16-bit inc through zp */

        while (gfx_busy ()) {}

        gfx_setcolor (COLOR_BLUE);
        gfx_clear ();

        gfx_setcolor (COLOR_WHITE);
        gfx_outtextxy (4, 4, "ZEROPAGE VARS");

        gfx_setcolor (COLOR_GREEN);
        byte2dec (frame, buf);
        gfx_outtextxy (4, 24, "FRAME");
        gfx_outtextxy (60, 24, buf);

        gfx_setcolor (COLOR_RED);
        byte2dec (presses, buf);
        gfx_outtextxy (4, 32, "PRESS");
        gfx_outtextxy (60, 32, buf);

        gfx_setcolor (COLOR_WHITE);
        byte2dec ((unsigned char)(ticks >> 8), buf);
        gfx_outtextxy (4, 40, "TICKH");
        gfx_outtextxy (60, 40, buf);

        gfx_setcolor (COLOR_WHITE);
        gfx_outtextxy (4, 76, "A = +   B = -");

        gfx_updatedisplay ();
    }
}
