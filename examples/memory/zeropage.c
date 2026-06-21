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
#include <lynx/tgi.h>
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

    tgi_init ();
    CLI ();
    while (tgi_busy ()) {}
    tgi_setpalette (tgi_getdefpalette ());
    tgi_setframerate (60);
    tgi_setcollisiondetection (0);
    tgi_setfont (TGI_FONT_COMPACT);

    for (;;) {
        joy     = joy_read (JOY_1);
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

        while (tgi_busy ()) {}

        tgi_setcolor (COLOR_BLUE);
        tgi_clear ();

        tgi_setcolor (COLOR_WHITE);
        tgi_outtextxy (4, 4, "ZEROPAGE VARS");

        tgi_setcolor (COLOR_GREEN);
        byte2dec (frame, buf);
        tgi_outtextxy (4, 24, "FRAME");
        tgi_outtextxy (60, 24, buf);

        tgi_setcolor (COLOR_RED);
        byte2dec (presses, buf);
        tgi_outtextxy (4, 32, "PRESS");
        tgi_outtextxy (60, 32, buf);

        tgi_setcolor (COLOR_WHITE);
        byte2dec ((unsigned char)(ticks >> 8), buf);
        tgi_outtextxy (4, 40, "TICKH");
        tgi_outtextxy (60, 40, buf);

        tgi_setcolor (COLOR_WHITE);
        tgi_outtextxy (4, 76, "A = +   B = -");

        tgi_updatedisplay ();
    }
}
