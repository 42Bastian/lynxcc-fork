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
** A minimal but complete program. It brings up the static TGI graphics library
** (design/LYNX_TGI_DESIGN.md): there is no driver to load, just tgi_init().
** Then it runs a double-buffered main loop that clears the screen, moves a
** label with the joystick, and redraws. Replace the body of the loop with your
** game.
**
** Build:  make            (or: cl65 -Ors -o game.lnx src/main.c)
** Run:    boot game.lnx on a Lynx or an emulator.
*/

#include <lynx/lynx.h>
#include <lynx/tgi.h>
#include <lynx/joystick.h>
#include <6502.h>

void main (void)
{
    int x = TGI_XRES / 2 - 36;          /* label position, kept on-screen */
    int y = TGI_YRES / 2 - 3;
    unsigned char joy;

    tgi_init ();                        /* bring up the display */
    CLI ();                             /* enable IRQs: the frame timer needs them */
    tgi_setframerate (60);

    for (;;) {
        /* --- input: move the label one pixel per frame --- */
        joy = (unsigned char)joy_read ();
        if ((joy & JOY_LEFT_MASK)  && x > 0)            --x;
        if ((joy & JOY_RIGHT_MASK) && x < TGI_XRES - 1) ++x;
        if ((joy & JOY_UP_MASK)    && y > 0)            --y;
        if ((joy & JOY_DOWN_MASK)  && y < TGI_YRES - 9) ++y;

        /* --- render into the back buffer, then request the swap --- */
        while (tgi_busy ()) {}
        tgi_setcolor (COLOR_BLACK);
        tgi_clear ();                   /* tgi_clear fills in the draw color */
        tgi_setcolor (COLOR_WHITE);
        tgi_outtextxy (x, y, "HELLO, LYNX!");
        tgi_updatedisplay ();
    }
}
