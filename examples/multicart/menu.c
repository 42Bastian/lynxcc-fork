/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in the examples directory.
*/

/*
** Multicart menu.
**
** The front end of a multicart: a single .lnx that bundles this menu plus
** several game ROMs (game1/game2/game3 here). At power-on the SDK bootloader
** runs this menu; the player moves the cursor and presses A to launch a game.
** multicart_run(n) copies the relocatable runtime loader to $0040 and jumps to
** it, which reads game n off the cartridge over the top of this menu and runs
** it -- so multicart_run() never returns. See design/LYNX_MULTICART_DESIGN.md.
**
** The menu, and each game, are built as BLL objects (cfg/lynx-bll.cfg) and
** stitched into the cart by "lnx multicart" + lynxdir; see the Makefile.
*/

#include <lynx/lynx.h>
#include <lynx/gfx.h>
#include <lynx/joystick.h>
#include <lynx/multicart.h>
#include <6502.h>

#define NGAMES 3

static const char* const names[NGAMES] = { "GAME 1", "GAME 2", "GAME 3" };

void main (void)
{
    unsigned char sel = 0, i;
    unsigned      prev = 0, now, pressed;

    gfx_init ();
    gfx_setdefpalette ();
    CLI ();                             /* the frame timer needs IRQs */
    gfx_setframerate (60);

    for (;;) {
        /* Edge-detect the joypad so one press moves one step. */
        now     = joy_read ();
        pressed = now & ~prev;
        prev    = now;

        if ((pressed & JOY_DOWN_MASK) && sel < NGAMES - 1) ++sel;
        if ((pressed & JOY_UP_MASK)   && sel > 0)          --sel;
        if (pressed & (JOY_BTN_A_MASK | JOY_BTN_B_MASK)) {
            multicart_run (sel);        /* load + run game "sel"; never returns */
        }

        /* --- draw --- */
        while (gfx_busy ()) {}
        gfx_setcolor (COLOR_BLACK);
        gfx_clear ();

        gfx_setcolor (COLOR_YELLOW);
        gfx_outtextxy (24, 8, "MULTICART DEMO");

        for (i = 0; i < NGAMES; ++i) {
            gfx_setcolor (i == sel ? COLOR_WHITE : COLOR_GREY);
            gfx_outtextxy (48, 36 + i * 12, names[i]);
            if (i == sel) {
                gfx_outtextxy (34, 36 + i * 12, ">");
            }
        }

        gfx_setcolor (COLOR_LIGHTBLUE);
        gfx_outtextxy (8, 88, "UP/DOWN  A=PLAY");

        gfx_updatedisplay ();
    }
}
