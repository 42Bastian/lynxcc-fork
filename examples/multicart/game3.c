/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in the examples directory.
*/

/*
** Multicart demo game 3.
**
** A stand-in for a real game: it is a complete, independent Lynx program built
** as a BLL object (cfg/lynx-bll.cfg) and bundled into the multicart by the
** Makefile. Launched from the menu, it just fills the screen and shows a big
** "3" so you can see which ROM the runtime loader pulled off the cartridge.
** Games 1 and 2 are identical bar the number and colour. There is no path back
** to the menu without a reboot: launching a game overwrites the menu.
*/

#include <lynx/lynx.h>
#include <lynx/gfx.h>
#include <6502.h>

void main (void)
{
    gfx_init ();
    CLI ();
    gfx_setframerate (60);

    for (;;) {
        while (gfx_busy ()) {}
        gfx_setcolor (COLOR_VIOLET);
        gfx_clear ();

        gfx_setcolor (COLOR_WHITE);
        gfx_settextscale (0x0500, 0x0500);      /* 5x the 8x8 font */
        gfx_outtextxy (60, 26, "3");
        gfx_settextscale (0x0100, 0x0100);
        gfx_outtextxy (40, 84, "GAME THREE");

        gfx_updatedisplay ();
    }
}
