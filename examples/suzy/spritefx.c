/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in this directory.
*/

/*
** Interactive tour of Suzy's four sprite transforms - hsize, vsize, stretch
** and tilt - and of where the sprite's ACTION POINT sits (doc/sprites.html
** sec. 6, sec. 6.1).
**
** One shape at a time is drawn around a fixed on-screen anchor and the
** selected transform is animated back and forth so you can watch the picture
** grow or shear OUTWARD FROM THE ANCHOR while the anchor pixel itself never
** moves. A bright magenta cross is drawn at that anchor every frame so the
** pivot is unmistakable.
**
**   D-pad     choose which transform is live and animating:
**               LEFT  = hsize   (whole-sprite horizontal 8.8 scale)
**               RIGHT = vsize   (whole-sprite vertical 8.8 scale)
**               UP    = stretch (adds to hsize on every scan line -> trapezoid)
**               DOWN  = tilt    (adds to hpos on every scan line -> shear)
**   A / B     cycle the shape: cube / crystal / star / ball
**   OPTION 1  cycle the action point through five spots in the artwork:
**               top-left, top-right, centre, bottom-left, bottom-right
**
** Action points and why five variants exist
** -----------------------------------------
** The reference (action) point is baked into the sprite DATA when sp65
** compacts it: its ax,ay attribute splits the image into up to four quadrants
** that all pivot about that one pixel. The SCB only carries hpos/vpos, so the
** only way to move the pivot inside the artwork is to compact the image again
** at a different ax,ay. The Makefile therefore runs sp65 five times per shape
** (see the header block below), and this program keeps a table of the five
** resulting sprites. Cycling the action point swaps the DATA pointer only -
** every SCB scale field stays byte-for-byte identical - yet a scaled or
** sheared shape lands somewhere else, because it now grows around a different
** pixel. A single-quadrant corner sprite (ax=ay=0) can only anchor at a
** corner; the centre variant is a true four-quadrant sprite, which is exactly
** the case the hsizeoff/vsizeoff $007F correction below is there to serve.
**
** The magenta cross is a separate 16x16 sprite (marker.pcx) whose own action
** point is its centre, drawn at the same anchor: it marks the fixed pivot on
** screen. Watch how each shape sits relative to it as you change the action
** point - corner anchors push the art off to one side, the centre anchor
** keeps it wrapped around the cross.
**
** Build:  make suzy/spritefx.lnx   (regenerates the sprite headers via sp65)
*/

#include <lynx/lynx.h>
#include <lynx/gfx.h>
#include <lynx/joystick.h>
#include <peekpoke.h>
#include <6502.h>

/* Five action-point variants per shape, emitted by sp65 (ax,ay per name):
**   _tl top-left(0,0)  _tr top-right(15,0)  _c centre(8,8)
**   _bl bottom-left(0,15)  _br bottom-right(15,15)
*/
#include "cube_tl.h"
#include "cube_tr.h"
#include "cube_c.h"
#include "cube_bl.h"
#include "cube_br.h"
#include "crystal_tl.h"
#include "crystal_tr.h"
#include "crystal_c.h"
#include "crystal_bl.h"
#include "crystal_br.h"
#include "star_tl.h"
#include "star_tr.h"
#include "star_c.h"
#include "star_bl.h"
#include "star_br.h"
#include "ball_tl.h"
#include "ball_tr.h"
#include "ball_c.h"
#include "ball_bl.h"
#include "ball_br.h"
#include "marker.h"          /* magenta cross (16x16), action point at centre */

#define NSHAPE   4
#define NAP      5

/* One shared 16-entry palette colours every shape: the four shapes use the
** disjoint pen ranges baked in by spritefx_shapes.py (cube 1-3, crystal 4-6,
** star 7-9, ball 10-12), pen 13 is HUD white and pen 15 is the marker's
** magenta. Lynx format: 16 green nibbles, then 16 (blue<<4 | red) bytes.
*/
static const unsigned char palette[32] = {
    /* green nibble, pen 0..15 */
    0x0, 0x9, 0x6, 0x3, 0xF, 0xA, 0x5, 0xE,
    0xA, 0x6, 0xD, 0x4, 0x1, 0xF, 0x8, 0x0,
    /* (blue<<4 | red), pen 0..15 */
    0x00, 0xC7, 0x94, 0x62, 0xF7, 0xC2, 0x81, 0x3F,
    0x1E, 0x09, 0xBF, 0x3E, 0x19, 0xFF, 0x88, 0xFF
};

static const unsigned char *const shape_data[NSHAPE][NAP] = {
    { cube_tl,    cube_tr,    cube_c,    cube_bl,    cube_br    },
    { crystal_tl, crystal_tr, crystal_c, crystal_bl, crystal_br },
    { star_tl,    star_tr,    star_c,    star_bl,    star_br    },
    { ball_tl,    ball_tr,    ball_c,    ball_bl,    ball_br    }
};
static const char *const shape_name[NSHAPE] = { "CUBE", "CRYSTAL", "STAR", "BALL" };

/* sp65 chooses each shape's bit depth from its highest pen index, so the four
** shapes differ (cube 2bpp .. ball 4bpp). The _COLORS define lets us set the
** matching SPRCTL0 depth bits without hard-coding them.
*/
static const unsigned char shape_colors[NSHAPE] = {
    cube_tl_COLORS, crystal_tl_COLORS, star_tl_COLORS, ball_tl_COLORS
};

static const char *const ap_name[NAP] = {
    "TOP-LEFT", "TOP-RIGHT", "CENTRE", "BOT-LEFT", "BOT-RIGHT"
};
static const char *const fx_name[4] = { "HSIZE", "VSIZE", "STRETCH", "TILT" };

/* Full SCB: REHVST reloads every scale field each frame; PACKED data; own
** identity penpal (value v -> pen v). sprctl0 depth is patched per shape. */
static SCB_REHVST_PAL scb = {
    BPP_4 | TYPE_NORMAL, PACKED | REHVST, NO_COLLIDE,
    0, 0, 0, 0, 0x0100, 0x0100, 0x0000, 0x0000,
    { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF }
};

/* The pivot marker: 1x, no stretch/tilt, identity penpal (its lit value
** 15 -> pen 15 = magenta). 16x16 field, cross centred on the action point,
** transparent elsewhere. Drawn on top of the shape at the anchor. */
static SCB_REHV_PAL mk = {
    BPP_4 | TYPE_NORMAL, PACKED | REHV, NO_COLLIDE,
    (char *)marker, (unsigned char *)marker, 0, 0, 0x0100, 0x0100,
    { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF }
};

/* Fixed on-screen anchor. Chosen so the biggest animated shape stays on the
** 160x102 screen from every action point. */
#define ANCHOR_X   100
#define ANCHOR_Y    52
#define SBASE      0x0180       /* 1.5x, the resting scale of the idle axis   */

static unsigned char bpp_bits (unsigned char colors)
{
    if (colors < 3) return BPP_1;
    if (colors < 5) return BPP_2;
    if (colors < 9) return BPP_3;
    return BPP_4;
}

static unsigned char shape = 2;     /* start on the star   */
static unsigned char ap    = 2;     /* start at the centre */
static unsigned char fx    = 0;     /* start on hsize      */
static unsigned int  frame = 0;

static void input (void)
{
    static unsigned prev = 0;
    unsigned now = joy_read ();
    unsigned hit = now & ~prev;
    prev = now;

    if (hit & JOY_LEFT_MASK)  fx = 0;
    if (hit & JOY_RIGHT_MASK) fx = 1;
    if (hit & JOY_UP_MASK)    fx = 2;
    if (hit & JOY_DOWN_MASK)  fx = 3;

    if (hit & JOY_BTN_A_MASK) shape = (shape + 1) % NSHAPE;
    if (hit & JOY_BTN_B_MASK) shape = (unsigned char)((shape + NSHAPE - 1) % NSHAPE);

    if (hit & JOY_OPT1_MASK)  ap = (ap + 1) % NAP;
}

static void draw (void)
{
    /* Triangle wave 0..60..0 drives the live transform. */
    unsigned f   = frame % 120;
    unsigned tri = (f < 60) ? f : (120 - f);

    /* Reset to the resting pose, then animate only the selected transform. */
    scb.hsize = scb.vsize = SBASE;
    scb.stretch = scb.tilt = 0;
    switch (fx) {
        case 0: scb.hsize = 0x0100 + tri * 6;          break;   /* hsize   */
        case 1: scb.vsize = 0x0100 + tri * 6;          break;   /* vsize   */
        case 2: scb.stretch = tri * 4;                 break;   /* stretch */
        case 3: scb.tilt = (unsigned)((int)(tri - 30) * 8); break; /* tilt */
    }

    scb.sprctl0 = bpp_bits (shape_colors[shape]) | TYPE_NORMAL;
    scb.data    = (unsigned char *)shape_data[shape][ap];
    scb.hpos    = ANCHOR_X;
    scb.vpos    = ANCHOR_Y;

    gfx_setcolor (0);
    gfx_clear ();

    gfx_sprite (&scb);                  /* the transformed shape ...        */
    mk.hpos = ANCHOR_X;
    mk.vpos = ANCHOR_Y;
    gfx_sprite (&mk);                   /* ... then the pivot cross on top  */

    /* HUD, drawn last so it stays legible over the sprite. */
    gfx_setcolor (13);
    gfx_outtextxy (2, 2,  "SPRITE TRANSFORMS");
    gfx_outtextxy (2, 16, "FX:");
    gfx_outtextxy (26, 16, fx_name[fx]);
    gfx_outtextxy (2, 28, "SPR:");
    gfx_outtextxy (34, 28, shape_name[shape]);
    gfx_outtextxy (2, 40, "AP:");
    gfx_outtextxy (26, 40, ap_name[ap]);
    gfx_outtextxy (2, 88, "PAD:FX  A/B:SPR");
    gfx_outtextxy (2, 96, "OPT1:ACTION POINT");
}

void main (void)
{
    gfx_init ();
    CLI ();
    gfx_setframerate (60);
    gfx_setpalette (palette);

    /* Cancel the one-pixel bump at the reference point of a scaled multi-
    ** quadrant sprite: load the "magic" $007F on the right/down directions
    ** (doc/sprites.html sec. 6). Shared by all sprites, set once here. */
    SUZY.hsizeoff = 0x007F;
    SUZY.vsizeoff = 0x007F;

    for (;;) {
        while (gfx_busy ()) {}
        input ();
        draw ();
        gfx_updatedisplay ();
        ++frame;
    }
}
