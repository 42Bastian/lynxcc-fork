/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in this directory.
*/

/*
** Basic Lynx Game Development SDK project template.
**
** A minimal but complete program. It brings up the static Lynx graphics library
** (design/LYNX_GFX_DESIGN.md): there is no driver to load, just gfx_init().
** Then it runs a double-buffered main loop that clears the screen, moves a
** label with the joystick, and redraws. Replace the body of the loop with your
** game.
**
** Build:  make            (or: cl65 -Ors -o game.lnx src/main.c)
** Run:    boot game.lnx on a Lynx or an emulator.
*/

#include <lynx/lynx.h>
#include <lynx/gfx.h>
#include <lynx/joystick.h>
#include <6502.h>

void main (void)
{
    int x = GFX_XRES / 2 - 36;          /* label position, kept on-screen */
    int y = GFX_YRES / 2 - 3;
    unsigned char joy;

    gfx_init ();                        /* bring up the display */
    CLI ();                             /* enable IRQs: the frame timer needs them */
    gfx_setframerate (60);

    for (;;) {
        /* --- input: move the label one pixel per frame --- */
        joy = (unsigned char)joy_read ();
        if ((joy & JOY_LEFT_MASK)  && x > 0)            --x;
        if ((joy & JOY_RIGHT_MASK) && x < GFX_XRES - 1) ++x;
        if ((joy & JOY_UP_MASK)    && y > 0)            --y;
        if ((joy & JOY_DOWN_MASK)  && y < GFX_YRES - 9) ++y;

        /* --- render into the back buffer, then request the swap --- */
        while (gfx_busy ()) {}
        gfx_setcolor (COLOR_BLACK);
        gfx_clear ();                   /* gfx_clear fills in the draw color */
        gfx_setcolor (COLOR_WHITE);
        gfx_outtextxy (x, y, "HELLO, LYNX!");
        gfx_updatedisplay ();
    }
}
