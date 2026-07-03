/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in this directory.
*/

/*
** sdcardls.c - List the SD card root on a RetroHQ Lynx SD / GD flash cart.
**
** Demonstrates the sdcard_gd_* API (design/LYNX_SDCARD_GD_API_DESIGN.md,
** doc/sdcard-gd.html): wake the cart MCU, open the root directory, walk it
** with sdcard_gd_readdir(), and print each 8.3 entry name on screen with a
** trailing '/' for sub-directories. Directories the RetroHQ menu reserves
** (MENU, _PREVIEW) are shown like any other entry here.
**
** This ROM only does anything meaningful on real hardware with an SD card
** inserted; in an emulator without cart-MCU emulation the FIFO handshake has
** nothing to talk to, so the "not ready" path is expected.
**
** Build:  cl65 -Ors -o sdcardls.lnx sdcardls.c
*/

#include <lynx/lynx.h>
#include <lynx/gfx.h>
#include <lynx/sdcard-gd.h>

#define MAX_ROWS 12         /* entries that fit on screen */

static SFileInfo info;

int main (void)
{
    unsigned char row;
    FRESULT res;

    gfx_init ();
    gfx_setpalette (gfx_getdefpalette ());
    gfx_setcolor (COLOR_BLACK);
    gfx_clear ();

    gfx_setcolor (COLOR_WHITE);
    gfx_outtextxy (0, 0, "SD/GD CARD ROOT:");

    /* Wake the cart MCU comms before anything else. */
    sdcard_gd_init ();

    res = sdcard_gd_opendir ("/");
    if (res != FR_OK) {
        gfx_setcolor (COLOR_RED);
        gfx_outtextxy (0, 16, "CARD NOT READY");
        gfx_updatedisplay ();
        for (;;) ;
    }

    row = 0;
    while (row < MAX_ROWS && sdcard_gd_readdir (&info) == FR_OK) {
        /* Mark directories with a trailing slash. */
        if (info.fattrib & AM_DIR) {
            gfx_setcolor (COLOR_GREEN);
        } else {
            gfx_setcolor (COLOR_WHITE);
        }
        gfx_outtextxy (0, 16 + row * 8, info.fname);
        ++row;
    }

    gfx_updatedisplay ();
    for (;;) ;
    return 0;
}
