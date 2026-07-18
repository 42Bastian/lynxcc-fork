/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in this directory.
*/

/*
** Sprite sheets the manual way: sp65's --slice / --pop chain.
**
** This sample draws the SAME four-frame bouncing ball as spritesheet.c, but
** built without the --sprite-sheet convenience. The Makefile reads the shared
** sheet.pcx once and dices it into four standalone headers in a single sp65
** run, popping back to the whole image between cells:
**
**   sp65 -r sheet.pcx \
**     --slice  0,0,16,16 -c lynx-sprite,mode=packed -w frame0.h,format=c,ident=frame0 --pop \
**     --slice 16,0,16,16 -c lynx-sprite,mode=packed -w frame1.h,format=c,ident=frame1 --pop \
**     --slice 32,0,16,16 -c lynx-sprite,mode=packed -w frame2.h,format=c,ident=frame2 --pop \
**     --slice 48,0,16,16 -c lynx-sprite,mode=packed -w frame3.h,format=c,ident=frame3
**
** Each header is an independent "const unsigned char frameN[]" sprite, so the
** program has to gather them into its own table by hand (frames[] below) and
** track the count itself. That hand-work is exactly what spritesheet.c gets for
** free from the driver - the frame *data* is identical either way (verified
** byte-for-byte, see design/LYNX_SPRITE_SHEET_DESIGN.md sec. 7).
**
** This --slice/--pop chain needs no new tooling and is the right choice when
** you only want a couple of frames or want per-frame control.
**
** Build:  cl65 -Ors -o spriteslice.lnx spriteslice.c  (after generating headers)
*/

#include <lynx/lynx.h>
#include <lynx/gfx.h>
#include <6502.h>

#include "frame0.h"
#include "frame1.h"
#include "frame2.h"
#include "frame3.h"

/* The four separate sprites, gathered into a table by hand. With the driver
** (spritesheet.c) this table is generated for you.
*/
static const unsigned char *const frames[] = {
    frame0, frame1, frame2, frame3
};
#define FRAME_COUNT (sizeof (frames) / sizeof (frames[0]))

/* Same palette as spritesheet.c / sheet.pcx. */
static const unsigned char palette[32] = {
    0x0, 0xC, 0xF, 0x3, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x00, 0xF3, 0xFC, 0x70, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static SCB_REHV_PAL scb = {
    BPP_2 | TYPE_NORMAL, PACKED | REHV, NO_COLLIDE,
    0, 0, 0, 0, 0x0400, 0x0400,                 /* 4x scale */
    { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF }
};

void main (void)
{
    unsigned char frame = 0;
    unsigned char tick  = 0;

    gfx_init ();
    CLI ();
    gfx_setframerate (60);
    gfx_setpalette (palette);

    for (;;) {
        while (gfx_busy ()) {}

        gfx_setcolor (0);
        gfx_clear ();

        gfx_setcolor (2);
        gfx_outtextxy (24, 8, "SLICE + POP");
        gfx_outtextxy (16, 20, "sp65 --slice");

        scb.data = (unsigned char *)frames[frame];
        scb.hpos = 48;
        scb.vpos = 40;
        gfx_sprite (&scb);

        gfx_updatedisplay ();

        if (++tick >= 8) {
            tick = 0;
            frame = (unsigned char)((frame + 1) % FRAME_COUNT);
        }
    }
}
