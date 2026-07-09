/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in this directory.
*/

/*
** Suzy hardware collision detection (Atari Lynx).
**
** This sample demonstrates the *collision buffer* and the per-sprite
** *collision depository* - the only collision facility the Lynx has in
** hardware. See doc/collisions.html for the full write-up; the short version:
**
**   * The collision buffer is a second, off-screen bitmap the exact shape of
**     the display. As Suzy paints a collideable sprite it stamps that sprite's
**     4-bit collision number (SPRCOLL, low nibble) into every cell it covers,
**     and *reads back* the numbers already there. The highest number it reads
**     is written to that sprite's collision depository byte when the sprite
**     finishes painting.
**
**   * gfx_init() points COLLBASE at the collision buffer and sets COLLOFF to
**     -1, so Suzy writes each sprite's depository to the byte *immediately
**     before* its SCB in memory. We give the player SCB a leading byte for
**     exactly that (see struct player_scb below).
**
**   * The buffer only exists when you link with lynx-coll.cfg (it carves 8 KB
**     out of RAM below the screen pages). gfx_setcollisiondetection(1) turns
**     the read-back on and makes gfx_clear() wipe the collision buffer too.
**
** The scene: four fixed target blocks sit in a row, each tagged with a
** distinct collision number 1..4. A player block sweeps left-to-right across
** them (pad up/down nudges it vertically, left/right steers). Targets are
** drawn first every frame, the player last; after the player is painted we
** read its depository - nonzero means it overlapped a target, and the value is
** *which* target (its collision number). We flash the struck target and show a
** HIT read-out, all from that one hardware byte - no per-pixel testing in C.
**
** Build:  cl65 -Ors -C lynx-coll.cfg -o collision.lnx collision.c
*/

#include <lynx/lynx.h>
#include <lynx/gfx.h>
#include <lynx/joystick.h>
#include <6502.h>
#include <stdio.h>

/* --- palette -----------------------------------------------------------
** Lynx format: 16 green nibbles, then 16 (blue<<4 | red) bytes.
**   pen 0 black (bg/transparent)   pen 1 white   (player)
**   pen 2 red  pen 3 green  pen 4 blue  pen 5 yellow  (the four targets)
**   pen 6 magenta (struck-target flash, distinct from the white player)
**   pen 15 white (text)
*/
static const unsigned char palette[32] = {
    /* green nibble per pen 0..15 */
    0x0, 0xF, 0x0, 0xF, 0x0, 0xF, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xF,
    /* blue<<4 | red per pen 0..15 */
    0x00, 0xFF, 0x0F, 0x00, 0xF0, 0x0F, 0xFF, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF
};

/* --- the block sprite --------------------------------------------------
** One 12x12 solid block, 4 bpp, every pixel = value 1. LITERAL encoding, so
** each scan line is [offset][6 data bytes][pad]. The trailing $00 pad byte
** feeds Suzy's last-pixel bug a transparent (pen-0) group to drop instead of
** the block's real right edge (offset counts itself + data + pad = 8 = $08).
** The per-SCB penpal maps value 1 to whatever pen that sprite should show, so
** one image serves the white player and all four coloured targets.
** A final $00 offset ends the sprite. See design/LYNX_SPRITE_PADBYTE_DESIGN.md.
*/
#define BLK_LINE 0x08, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x00
static const unsigned char block[] = {
    BLK_LINE, BLK_LINE, BLK_LINE, BLK_LINE, BLK_LINE, BLK_LINE,
    BLK_LINE, BLK_LINE, BLK_LINE, BLK_LINE, BLK_LINE, BLK_LINE,
    0x00
};

#define BLK   12                 /* on-screen block size, pixels        */
#define ROWY  46                 /* y of the target row and player      */

/* penpal that maps sprite value 1 -> pen p (value 0 stays pen 0/transparent) */
#define PENPAL1(p) { (unsigned char)(p), 0, 0, 0, 0, 0, 0, 0 }

/* One target: a coloured block with a fixed collision number 1..4. */
typedef struct target {
    unsigned char coll;          /* depository (unused for targets, kept -1  */
    SCB_REHV_PAL  scb;           /* offset so &scb-1 is a valid byte)        */
    unsigned char pen;           /* normal colour                            */
} target;

static target targets[4] = {
    { 0, { BPP_4 | TYPE_NORMAL, LITERAL | REHV, 1,
           0, (unsigned char *)block, 20, ROWY, 0x0100, 0x0100, PENPAL1(2) }, 2 },
    { 0, { BPP_4 | TYPE_NORMAL, LITERAL | REHV, 2,
           0, (unsigned char *)block, 55, ROWY, 0x0100, 0x0100, PENPAL1(3) }, 3 },
    { 0, { BPP_4 | TYPE_NORMAL, LITERAL | REHV, 3,
           0, (unsigned char *)block, 90, ROWY, 0x0100, 0x0100, PENPAL1(4) }, 4 },
    { 0, { BPP_4 | TYPE_NORMAL, LITERAL | REHV, 4,
           0, (unsigned char *)block, 125, ROWY, 0x0100, 0x0100, PENPAL1(5) }, 5 }
};

/* The player SCB with its collision depository byte immediately in front of
** it. COLLOFF = -1 means Suzy writes the result to (&scb - 1), which cc65's
** packed struct layout puts exactly at &player.coll.
*/
static struct player_scb {
    unsigned char coll;          /* <- Suzy writes the depository here       */
    SCB_REHV_PAL  scb;
} player = {
    0,
    { BPP_4 | TYPE_NORMAL, LITERAL | REHV, 5,
      0, (unsigned char *)block, 20, ROWY, 0x0100, 0x0100, PENPAL1(1) }
};

static void draw (unsigned char hit)
{
    unsigned char i;
    char msg[16];

    gfx_setcolor (0);
    gfx_clear ();                 /* clears screen AND collision buffer       */

    /* Targets first: they lay their collision numbers into the buffer. The
    ** one the player is currently on (hit) flashes to the bright pen. */
    for (i = 0; i < 4; ++i) {
        targets[i].scb.penpal[0] =
            (hit == targets[i].scb.sprcoll) ? 6 : targets[i].pen;
        gfx_sprite (&targets[i].scb);
    }

    /* Player last: as it paints, Suzy reads the numbers already in the buffer
    ** and drops the highest into player.coll. */
    gfx_sprite (&player.scb);

    gfx_setcolor (15);
    gfx_outtextxy (8, 6, "COLLISION BUFFER");
    if (hit)
        sprintf (msg, "HIT TARGET %u", hit);
    else
        sprintf (msg, "HIT TARGET -");
    gfx_outtextxy (8, 88, msg);
}

void main (void)
{
    unsigned char hit = 0;
    unsigned int  joy;
    signed int    dx = 2;         /* auto-sweep speed/direction               */

    gfx_init ();
    CLI ();
    gfx_setframerate (60);
    gfx_setpalette (palette);
    gfx_setcollisiondetection (1);

    for (;;) {
        while (gfx_busy ()) {}

        /* Auto-sweep the player across the row, bouncing at the edges. */
        player.scb.hpos += dx;
        if (player.scb.hpos < 4)          { player.scb.hpos = 4;   dx = 2;  }
        if (player.scb.hpos > 160 - BLK)  { player.scb.hpos = 160 - BLK; dx = -2; }

        /* Pad steers it too, so you can line it up with any target. */
        joy = joy_read ();
        if (joy & JOY_UP_MASK    && player.scb.vpos > 16)        player.scb.vpos -= 2;
        if (joy & JOY_DOWN_MASK  && player.scb.vpos < 102 - BLK) player.scb.vpos += 2;
        if (joy & JOY_LEFT_MASK)  { player.scb.hpos -= 2; dx = -2; }
        if (joy & JOY_RIGHT_MASK) { player.scb.hpos += 2; dx =  2; }

        draw (hit);
        gfx_updatedisplay ();

        /* Read the hardware depository written while the player painted. */
        hit = player.coll;
    }
}
