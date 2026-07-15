/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in the examples directory.
*/

/*
** Multicart demo game 2.
**
** A stand-in for a real game: it is a complete, independent Lynx program built
** as a BLL object (cfg/lynx-bll.cfg) and bundled into the multicart by the
** Makefile. Launched from the menu, it just fills the screen and shows a big
** "2" so you can see which ROM the runtime loader pulled off the cartridge.
** Games 1 and 3 are identical bar the number and colour. There is no path back
** to the menu without a reboot: launching a game overwrites the menu.
*/

#include <lynx/lynx.h>
#include <lynx/gfx.h>
#include <6502.h>

void main (void)
{
    gfx_init ();
    gfx_setdefpalette ();
    CLI ();
    gfx_setframerate (60);

    for (;;) {
        while (gfx_busy ()) {}
        gfx_setcolor (COLOR_GREEN);
        gfx_clear ();

        gfx_setcolor (COLOR_WHITE);
        gfx_settextscale (0x0500, 0x0500);      /* 5x the 8x8 font */
        gfx_outtextxy (60, 26, "2");
        gfx_settextscale (0x0100, 0x0100);
        gfx_outtextxy (44, 84, "GAME TWO");

        gfx_updatedisplay ();
    }
}
