/*
** fonttest.c - Demonstrates the compact 5x5 TGI font on the Atari Lynx.
**
** The compact font (TGI_FONT_COMPACT, see design/LYNX_TGI_FONT5X5_DESIGN.md) packs
** glyphs at a 6-px pitch with a transparent background; the foreground is
** drawn in the current pen (tgi_setcolor). The whole screen is cleared to
** blue so the transparency is obvious - the text floats directly on blue
** with no background box. Suzy scales the text sprite natively in 8.8, so
** tgi_settextscale works for this font too; the SCALE line is drawn at
** 1x / 2x / 3x.
**
** Controls: A cycles the scale (1x / 2x / 3x).
**
** Build:  cl65 -t lynx -Ors -o fonttest.lnx fonttest.c
*/

#include <lynx.h>
#include <tgi.h>
#include <joystick.h>
#include <6502.h>

static const char* const banner = "ABCDEFGHIJKLMNOP";
static const char* const digits = "0123456789 .,!?";

void main (void)
{
    unsigned char scale = 1;            /* 1, 2 or 3                       */
    unsigned char joy, prev = 0, pressed;
    unsigned char tick = 0;

    tgi_init ();
    CLI ();
    while (tgi_busy ()) {}
    tgi_setpalette (tgi_getdefpalette ());
    tgi_setframerate (60);
    tgi_setcollisiondetection (0);

    /* The compact font is the only font this program uses. */
    tgi_setfont (TGI_FONT_COMPACT);

    for (;;) {
        joy     = joy_read (JOY_1);
        pressed = joy & (unsigned char)~prev;
        prev    = joy;

        if (pressed & JOY_BTN_1_MASK) {
            if (++scale > 3) scale = 1;
        }

        while (tgi_busy ()) {}

        /* Blue background so the transparent glyph background is visible. */
        tgi_setcolor (COLOR_BLUE);
        tgi_clear ();

        tgi_setcolor (COLOR_WHITE);
        tgi_outtextxy (4, 4, "5X5 COMPACT FONT");

        tgi_setcolor (COLOR_WHITE);
        tgi_outtextxy (4, 20, banner);
        tgi_setcolor (COLOR_RED);
        tgi_outtextxy (4, 28, digits);
        tgi_setcolor (COLOR_GREEN);
        tgi_outtextxy (4, 36, "TRANSPARENT BG");

        /* Scaled text: 1x, 2x or 3x via Suzy 8.8 scaling. */
        tgi_settextscale ((unsigned)scale << 8, (unsigned)scale << 8);
        tgi_setcolor (COLOR_WHITE);
        tgi_outtextxy (4, 60, "SCALE");
        tgi_settextscale (0x100, 0x100);

        /* Footer hint, blinking. */
        if (tick & 0x20) {
            tgi_setcolor (COLOR_WHITE);
            tgi_outtextxy (4, 92, "A = SCALE");
        }
        ++tick;

        tgi_updatedisplay ();
    }
}
