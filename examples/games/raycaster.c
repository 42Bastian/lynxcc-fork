/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in this directory.
*/

/*
** raycaster.c - Single-file Wolfenstein-3D-style raycaster for the
** Atari Lynx. A companion to breakout.c / invaders.c.
**
** Where invaders.c shows off coloured sprites + direct Mikey audio,
** this one leans on the hardware the Lynx makes cheap:
**
**   1. Hardware sprite SCALING as a fill primitive. Every vertical
**      wall slice is ONE 2x1 solid sprite stretched by the SCB's
**      vsize field to the slice height the raycaster computed. 80
**      such scaled sprites paint the whole 3D view - no per-pixel
**      plotting, Suzy does the column fills. The sky/ceiling band,
**      the floor band, the billboard enemies and the whole status-bar
**      panel are scaled sprites too - all members of ONE chained
**      engine run, so there is no per-frame screen clear at all.
**
**   2. Suzy hardware math. The wall-hit distance inside a cell is a
**      hardware multiply (`!*`); the billboard and hitscan projections
**      are the FUSED multiply-divide the fork recognises as one op
**      (`lat !* PROJH !/ depth`). The per-column delta-distances and
**      slice height WERE Suzy divides but are now reciprocal-table
**      lookups (recip[]/slicelut[], built once at startup - see the
**      cast_walls() header), so cast_walls() issues no divide at all.
**      All of it runs in the main loop (never in IRQ, per
**      design/LYNX_CODEGEN_DESIGN.md 2.6) so the math unit is never
**      contended - and all of it runs BEFORE any sprite is launched,
**      since the sprite engine shares Suzy's math registers.
**
**   3. Cheap per-frame ray setup. The camera-plane sweep across the 80
**      columns is a linear ramp, so instead of a divide + multiply per
**      column the ray direction is stepped INCREMENTALLY with a 32-bit
**      add (accX/accY) - and because those per-column values depend
**      only on the view ANGLE, the whole ramp runs only on frames the
**      player turns (rebuild_raycache); walking reuses the cached
**      tables, and a frame where nothing moved skips the cast
**      entirely. The DDA inner loop itself is add/index/load/test on
**      a flat 256-byte map with the hot scalars in the zero page.
**
**   4. A Wolfenstein-style status bar across the bottom 22 rows: an
**      animated soldier face, WAVE / SCORE / HEALTH / enemies-left.
**      Because the bar is opaque, the 3D view only has to be rendered
**      into the top 80 rows - a third fewer scaled-sprite fill pixels
**      than a full-screen view, which is where a chunk of the speed-up
**      comes from.
**
** Gameplay: you are in a 16x16 maze with patrolling guards. Pad
** up/down walks, left/right turns (hold B to strafe), A fires your
** pistol at whatever is under the crosshair. Clear every guard to
** advance a wave. Guards that reach you chip your health; at zero
** it's game over (A restarts). Opt 1 toggles the HUD/text overlay
** for a clean, glyph-free view; Opt 2 toggles a half-resolution
** fast mode (40 double-width columns - half the raycast per frame).
**
** All fixed-point math is integer. Positions are 8.8 in map cells;
** angles are 0..255 = full circle via a 256-entry signed sine LUT.
**
** Build:  cl65 -Ors -o raycaster.lnx raycaster.c
*/

#include <lynx/lynx.h>
#include <lynx/gfx.h>
#include <lynx/joystick.h>
#include <6502.h>
#include <string.h>
#include <zeropage.h>

/* ------------------------------------------------------------------ */
/* Geometry / tuning                                                   */
/* ------------------------------------------------------------------ */

#define SCREEN_W    160
#define SCREEN_H    102

#define VIEW_H      80              /* 3D viewport height (rows 0..79) */
#define VIEW_CY     40              /* viewport vertical centre        */
#define HUD_Y       80              /* status bar top row              */
#define HUD_H       22              /* status bar height (80..101)     */

#define NCOL        80              /* ray columns; each COLW wide    */
#define COLW        2               /* SCREEN_W / NCOL                */
#define CAMSTEP     1638            /* 8.8 camera-plane step / column  */
                                    /* = 512*256/80 (sweep -256..+256) */

/* Opt 2 low-res mode (design 9.3 item 9): half the columns, each twice
** as wide. Halves the whole cast (and the ray-cache rebuild on turns)
** at a visible coarsening - an explicit quality/speed tradeoff. */
#define NCOL_LO     40
#define CAMSTEP_LO  3276            /* = 512*256/40                    */

#define MAPW        16
#define MAPH        16

#define FOV         169             /* 0.66 * 256 (camera-plane scale)*/
#define PROJK       1280            /* slice height = PROJK / perp     */
#define PROJH       121             /* horizontal billboard projection */
#define ABSCALE     4096            /* DDA delta-dist numerator        */
#define DDMAX       255             /* clamp so fracX*ddX fits 16 bits */
#define MAXSLICE    VIEW_H          /* wall never overdraws the HUD    */
#define RECIP_MAX   430             /* recip[] domain: max clamped |ray|  */
#define SLICE_N     641             /* slicelut[] entries (indexed perp>>1)*/

#define MOVESPEED   26              /* 8.8 cell step per frame         */
#define TURNSPEED   4               /* angle units per frame           */

#define NENEMY      6
#define ENEMY_SPEED 10              /* 8.8 cell step per frame         */
#define ATTACK_DIST (256 + 64)      /* 8.8: ~1.25 cells               */
#define WAKE_DIST   (256 * 8)       /* enemies wake within 8 cells     */
#define AIM_TOL     7               /* px half-width of the crosshair  */
#define FIRE_COOL   8               /* frames between shots            */
#define ENEMY_HP    2

/* Hardware pens. 16 total (4bpp); two wall materials leave room for   */
/* the dedicated status-bar steel colours (7/8).                       */
#define PEN_NONE    0
#define PEN_SKY     1               /* ceiling band                    */
#define PEN_FLOOR   2
#define PEN_WALLA   3               /* stone; +1 = dark (N/S face)     */
#define PEN_WALLB   5               /* brick; +1 = dark                */
#define PEN_HUD     7               /* status-bar panel                */
#define PEN_HUDLT   8               /* status-bar highlight edge       */
#define PEN_EUNI    9               /* enemy uniform                   */
#define PEN_ESKIN   10              /* enemy / face skin               */
#define PEN_GUN     11
#define PEN_GRIP    12              /* gun grip / face hair+detail     */
#define PEN_FLASH   13
#define PEN_WARN    14
#define PEN_TEXT    15

/* Game states */
#define ST_PLAY     0
#define ST_OVER     1
#define ST_WIN      2

/* ------------------------------------------------------------------ */
/* Map. 0 = open, 1..3 = wall types (folded to two materials). Stored  */
/* as a FLAT 256-byte array indexed (mapY << 4) | mapX so the DDA in   */
/* cast_walls() walks it with +/-1 (X step) and +/-16 (Y step) index   */
/* adds - no row multiply, and the whole map coordinate fits one byte  */
/* (design/LYNX_RAYCASTER_DRAW_DESIGN.md 9.1 item 3).                  */
/*                                                                     */
/* INVARIANT: every border cell is a wall (nonzero). The DDA inner     */
/* loop carries NO bounds tests (9.1 item 2) - each step moves exactly */
/* one axis, so a ray can never leave the map without first hitting    */
/* the border ring. Keep the ring closed when editing the layout.      */
/* ------------------------------------------------------------------ */

static const unsigned char worldmap[MAPH * MAPW] = {
    1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,
    1,0,0,0,3,3,0,0,0,0,2,2,0,0,0,2,
    1,0,0,0,3,0,0,0,0,0,0,2,0,0,0,2,
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,
    1,0,0,0,0,0,1,1,1,0,0,0,0,0,0,2,
    1,0,0,0,0,0,1,0,1,0,0,0,3,3,0,2,
    1,0,0,0,0,0,1,0,0,0,0,0,0,3,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,2,2,0,0,0,0,0,0,0,0,0,0,1,
    3,0,0,2,0,0,0,3,3,3,0,0,0,0,0,1,
    3,0,0,0,0,0,0,0,0,0,0,0,1,1,0,1,
    3,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,
    3,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
    3,3,3,3,3,1,1,1,1,1,2,2,2,2,1,1,
};

/* ------------------------------------------------------------------ */
/* Signed 8.8 sine LUT, 256 steps = full circle.                       */
/* ------------------------------------------------------------------ */

static const int sintab[256] = {
       0,    6,   13,   19,   25,   31,   38,   44,   50,   56,   62,   68,   74,   80,   86,   92,
      98,  104,  109,  115,  121,  126,  132,  137,  142,  147,  152,  157,  162,  167,  172,  177,
     181,  185,  190,  194,  198,  202,  206,  209,  213,  216,  220,  223,  226,  229,  231,  234,
     237,  239,  241,  243,  245,  247,  248,  250,  251,  252,  253,  254,  255,  255,  256,  256,
     256,  256,  256,  255,  255,  254,  253,  252,  251,  250,  248,  247,  245,  243,  241,  239,
     237,  234,  231,  229,  226,  223,  220,  216,  213,  209,  206,  202,  198,  194,  190,  185,
     181,  177,  172,  167,  162,  157,  152,  147,  142,  137,  132,  126,  121,  115,  109,  104,
      98,   92,   86,   80,   74,   68,   62,   56,   50,   44,   38,   31,   25,   19,   13,    6,
       0,   -6,  -13,  -19,  -25,  -31,  -38,  -44,  -50,  -56,  -62,  -68,  -74,  -80,  -86,  -92,
     -98, -104, -109, -115, -121, -126, -132, -137, -142, -147, -152, -157, -162, -167, -172, -177,
    -181, -185, -190, -194, -198, -202, -206, -209, -213, -216, -220, -223, -226, -229, -231, -234,
    -237, -239, -241, -243, -245, -247, -248, -250, -251, -252, -253, -254, -255, -255, -256, -256,
    -256, -256, -256, -255, -255, -254, -253, -252, -251, -250, -248, -247, -245, -243, -241, -239,
    -237, -234, -231, -229, -226, -223, -220, -216, -213, -209, -206, -202, -198, -194, -190, -185,
    -181, -177, -172, -167, -162, -157, -152, -147, -142, -137, -132, -126, -121, -115, -109, -104,
     -98,  -92,  -86,  -80,  -74,  -68,  -62,  -56,  -50,  -44,  -38,  -31,  -25,  -19,  -13,   -6,
};

#define SINE(a)   sintab[(unsigned char)(a)]
#define COSINE(a) sintab[(unsigned char)((a) + 64)]

/* ------------------------------------------------------------------ */
/* Sprite image data (4bpp literal: count byte incl. itself, then     */
/* pixel-pair bytes, then a trailing 0x00 pad byte; 0 count ends the  */
/* sprite). The pad works around Suzy's last-pixel bug (it drops the  */
/* final pixel of every literal line); value 0 maps to PEN_NONE (pen  */
/* 0, transparent), so the 0x00 pad is invisible - and it keeps the   */
/* stretched wall/sky/HUD fill full-width. See                        */
/* design/LYNX_SPRITE_PADBYTE_DESIGN.md.                              */
/* ------------------------------------------------------------------ */

/* 2x1 solid block: every wall slice, the sky/ceiling band and the     */
/* status-bar panels are this image stretched by the SCB hsize/vsize.  */
static unsigned char solid_img[] = {
    0x03, 0x11, 0x00,
    0x00
};

/* Guard, 16x16. value 1 = uniform, 2 = face. */
static unsigned char guard_img[] = {
    0x0A, 0x00, 0x00, 0x01, 0x11, 0x11, 0x10, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x12, 0x22, 0x22, 0x10, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x12, 0x22, 0x22, 0x10, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x01, 0x11, 0x11, 0x10, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x01, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00, 0x00,
    0x0A, 0x00, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00,
    0x0A, 0x01, 0x11, 0x12, 0x11, 0x11, 0x12, 0x11, 0x10, 0x00,
    0x0A, 0x01, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00,
    0x0A, 0x01, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00,
    0x0A, 0x00, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00,
    0x0A, 0x00, 0x01, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00, 0x00,
    0x0A, 0x00, 0x01, 0x11, 0x00, 0x00, 0x11, 0x10, 0x00, 0x00,
    0x0A, 0x00, 0x01, 0x11, 0x00, 0x00, 0x11, 0x10, 0x00, 0x00,
    0x0A, 0x00, 0x01, 0x11, 0x00, 0x00, 0x11, 0x10, 0x00, 0x00,
    0x0A, 0x00, 0x11, 0x10, 0x00, 0x00, 0x01, 0x11, 0x00, 0x00,
    0x0A, 0x00, 0x11, 0x10, 0x00, 0x00, 0x01, 0x11, 0x00, 0x00,
    0x00
};

/* Guard, attack frame (arms out). */
static unsigned char guard_img2[] = {
    0x0A, 0x00, 0x00, 0x01, 0x11, 0x11, 0x10, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x12, 0x22, 0x22, 0x10, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x12, 0x22, 0x22, 0x10, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x01, 0x11, 0x11, 0x10, 0x00, 0x00, 0x00,
    0x0A, 0x10, 0x01, 0x11, 0x11, 0x11, 0x11, 0x10, 0x01, 0x00,
    0x0A, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x00,
    0x0A, 0x01, 0x11, 0x12, 0x11, 0x11, 0x12, 0x11, 0x10, 0x00,
    0x0A, 0x01, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00,
    0x0A, 0x00, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00,
    0x0A, 0x00, 0x01, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00, 0x00,
    0x0A, 0x00, 0x01, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00, 0x00,
    0x0A, 0x00, 0x01, 0x11, 0x00, 0x00, 0x11, 0x10, 0x00, 0x00,
    0x0A, 0x00, 0x01, 0x11, 0x00, 0x00, 0x11, 0x10, 0x00, 0x00,
    0x0A, 0x00, 0x01, 0x11, 0x00, 0x00, 0x11, 0x10, 0x00, 0x00,
    0x0A, 0x00, 0x11, 0x10, 0x00, 0x00, 0x01, 0x11, 0x00, 0x00,
    0x0A, 0x00, 0x11, 0x10, 0x00, 0x00, 0x01, 0x11, 0x00, 0x00,
    0x00
};

/* Pistol viewmodel, 24x20. value 1 = metal, 2 = grip. */
static unsigned char gun_img[] = {
    0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0E, 0x00, 0x00, 0x00, 0x00, 0x01, 0x11, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0E, 0x00, 0x00, 0x00, 0x00, 0x01, 0x11, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0E, 0x00, 0x00, 0x00, 0x00, 0x01, 0x11, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0E, 0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0E, 0x00, 0x00, 0x00, 0x01, 0x11, 0x11, 0x11, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0E, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0E, 0x00, 0x00, 0x01, 0x11, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00, 0x00, 0x00, 0x00,
    0x0E, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00,
    0x0E, 0x00, 0x01, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00, 0x00, 0x00,
    0x0E, 0x00, 0x01, 0x11, 0x12, 0x22, 0x22, 0x11, 0x11, 0x11, 0x10, 0x00, 0x00, 0x00,
    0x0E, 0x00, 0x00, 0x11, 0x22, 0x22, 0x22, 0x21, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00,
    0x0E, 0x00, 0x00, 0x02, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0E, 0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0E, 0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0E, 0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0E, 0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0E, 0x00, 0x02, 0x22, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0E, 0x00, 0x02, 0x22, 0x22, 0x22, 0x22, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0E, 0x00, 0x22, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00
};

/* Muzzle flash, 12x12. */
static unsigned char flash_img[] = {
    0x08, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x00, 0x11, 0x11, 0x00, 0x00, 0x00,
    0x08, 0x01, 0x00, 0x11, 0x00, 0x10, 0x00, 0x00,
    0x08, 0x00, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00,
    0x08, 0x01, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00,
    0x08, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x00,
    0x08, 0x01, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00,
    0x08, 0x00, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00,
    0x08, 0x01, 0x00, 0x11, 0x00, 0x10, 0x00, 0x00,
    0x08, 0x00, 0x00, 0x11, 0x11, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x00, 0x01, 0x10, 0x00, 0x00, 0x00,
    0x00
};

/* Status-bar soldier face, 16x16. value 1 = skin, 2 = hair/eyes/mouth. */
static unsigned char face_img[] = {
    0x0A, 0x00, 0x02, 0x22, 0x22, 0x22, 0x20, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x22, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00,
    0x0A, 0x02, 0x21, 0x11, 0x11, 0x11, 0x12, 0x20, 0x00, 0x00,
    0x0A, 0x02, 0x11, 0x11, 0x11, 0x11, 0x11, 0x20, 0x00, 0x00,
    0x0A, 0x01, 0x11, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00, 0x00,
    0x0A, 0x01, 0x12, 0x21, 0x11, 0x22, 0x11, 0x10, 0x00, 0x00,
    0x0A, 0x01, 0x12, 0x21, 0x11, 0x22, 0x11, 0x10, 0x00, 0x00,
    0x0A, 0x01, 0x11, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00, 0x00,
    0x0A, 0x01, 0x11, 0x12, 0x22, 0x11, 0x11, 0x10, 0x00, 0x00,
    0x0A, 0x01, 0x11, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00, 0x00,
    0x0A, 0x01, 0x11, 0x22, 0x22, 0x22, 0x11, 0x10, 0x00, 0x00,
    0x0A, 0x01, 0x11, 0x12, 0x22, 0x21, 0x11, 0x10, 0x00, 0x00,
    0x0A, 0x00, 0x11, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x21, 0x11, 0x11, 0x11, 0x12, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x02, 0x21, 0x11, 0x12, 0x20, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00, 0x00,
    0x00
};

/* Status-bar face, hurt frame (gritted / open mouth). */
static unsigned char face_img2[] = {
    0x0A, 0x00, 0x02, 0x22, 0x22, 0x22, 0x20, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x22, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00,
    0x0A, 0x02, 0x21, 0x11, 0x11, 0x11, 0x12, 0x20, 0x00, 0x00,
    0x0A, 0x02, 0x11, 0x11, 0x11, 0x11, 0x11, 0x20, 0x00, 0x00,
    0x0A, 0x01, 0x11, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00, 0x00,
    0x0A, 0x01, 0x22, 0x11, 0x11, 0x11, 0x22, 0x10, 0x00, 0x00,
    0x0A, 0x01, 0x12, 0x21, 0x11, 0x22, 0x11, 0x10, 0x00, 0x00,
    0x0A, 0x01, 0x11, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00, 0x00,
    0x0A, 0x01, 0x11, 0x12, 0x22, 0x11, 0x11, 0x10, 0x00, 0x00,
    0x0A, 0x01, 0x11, 0x22, 0x22, 0x22, 0x11, 0x10, 0x00, 0x00,
    0x0A, 0x01, 0x12, 0x22, 0x22, 0x22, 0x21, 0x10, 0x00, 0x00,
    0x0A, 0x01, 0x12, 0x11, 0x11, 0x11, 0x21, 0x10, 0x00, 0x00,
    0x0A, 0x00, 0x12, 0x22, 0x22, 0x22, 0x21, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x21, 0x11, 0x11, 0x11, 0x12, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x02, 0x21, 0x11, 0x12, 0x20, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00, 0x00,
    0x00
};

/* ------------------------------------------------------------------ */
/* Palette. 32 bytes: 16 green nibbles then 16 red/blue (GCOLMAP).     */
/* 12-bit colours written $GRB and split into the two halves.         */
/* ------------------------------------------------------------------ */

#define GRB_G(c)    ((unsigned char)(((c) >> 8) & 0x0F))
#define GRB_RB(c)   ((unsigned char)((c) & 0xFF))

static unsigned char pal[32];

static const unsigned int pen_base[16] = {
    0x000,      /* 0  transparent / black      */
    0x335,      /* 1  ceiling (blue-grey)      */
    0x342,      /* 2  floor (olive brown)      */
    0x9AA,      /* 3  wall A lit (grey stone)  */
    0x566,      /* 4  wall A dark              */
    0x2D3,      /* 5  wall B lit (brick red)   */
    0x081,      /* 6  wall B dark              */
    0x225,      /* 7  HUD panel (steel blue)   */
    0x78A,      /* 8  HUD highlight edge       */
    0x214,      /* 9  enemy uniform (SS blue)  */
    0x9D7,      /* 10 enemy / face skin        */
    0x666,      /* 11 gun metal                */
    0x320,      /* 12 gun grip / face detail   */
    0xFC0,      /* 13 muzzle flash / gold      */
    0x0F0,      /* 14 warning red              */
    0xFFF       /* 15 text / crosshair         */
};

static void pal_init (void)
{
    unsigned char i;
    for (i = 0; i < 16; ++i) {
        pal[i]      = GRB_G (pen_base[i]);
        pal[16 + i] = GRB_RB (pen_base[i]);
    }
}

/* ------------------------------------------------------------------ */
/* Sound: direct Mikey audio (same scheme as invaders.c).             */
/* ------------------------------------------------------------------ */

#define FB_TONE     0x01
#define FB_NOISE    0x3F

static void snd_voice (struct _mikey_audio* c, unsigned char vol,
                       unsigned char fb, unsigned char clk,
                       unsigned char reload)
{
    c->control  = 0;
    c->feedback = fb;
    c->shiftlo  = 0;
    c->other    = 0;
    c->reload   = reload;
    c->dac      = 0;
    c->volume   = vol;
    c->control  = 0x18 | clk;
}

static void snd_silence (struct _mikey_audio* c)
{
    c->control = 0;
    c->volume  = 0;
    c->dac     = 0;
}

static unsigned char shot_t;        /* channel A: pistol crack   */
static unsigned char kill_t, kill_v;/* channel B: enemy death    */
static unsigned char hurt_t, hurt_p;/* channel D: player hurt    */

static void sfx_shot (void)
{
    shot_t = 4;
    snd_voice (&MIKEY.channel_a, 0x50, FB_NOISE, 1, 0x20);
}

static void sfx_kill (void)
{
    kill_v = 0x48;
    kill_t = 14;
    snd_voice (&MIKEY.channel_b, kill_v, FB_NOISE, 4, 0x60);
}

static void sfx_hurt (void)
{
    hurt_p = 90;
    hurt_t = 10;
    snd_voice (&MIKEY.channel_d, 0x40, FB_TONE, 4, hurt_p);
}

static void sfx_update (void)
{
    if (shot_t) {
        if (--shot_t == 0) snd_silence (&MIKEY.channel_a);
    }
    if (kill_t) {
        if (kill_v > 5) kill_v -= 5;
        MIKEY.channel_b.volume = kill_v;
        if (--kill_t == 0) snd_silence (&MIKEY.channel_b);
    }
    if (hurt_t) {
        hurt_p += 6;
        MIKEY.channel_d.reload = hurt_p;
        if (--hurt_t == 0) snd_silence (&MIKEY.channel_d);
    }
}

/* ------------------------------------------------------------------ */
/* Sprite control blocks                                               */
/* ------------------------------------------------------------------ */

/* One SCB per ray column, chained into a single engine run. */
static SCB_REHV_PAL wallscb[NCOL];

/* Ceiling band: top VIEW_CY rows of the viewport. */
static SCB_REHV_PAL sky_scb = {
    BPP_4 | TYPE_NORMAL, LITERAL | REHV, NO_COLLIDE,
    0, solid_img, 0, 0, 0x5000, (unsigned)VIEW_CY << 8,
    { (PEN_NONE << 4) | PEN_SKY, 0, 0, 0, 0, 0, 0, 0 }
};

/* Floor band: rows VIEW_CY..VIEW_H-1, a chain member right after the sky.
** Replaces the old full-screen gfx_clear() - sky + floor + opaque HUD panel
** cover every row, so nothing needs a clear (design 9.1 item 1: ~62 % of the
** cleared pixels were repainted the same frame anyway). */
static SCB_REHV_PAL floor_scb = {
    BPP_4 | TYPE_NORMAL, LITERAL | REHV, NO_COLLIDE,
    0, solid_img, 0, VIEW_CY, 0x5000, (unsigned)(VIEW_H - VIEW_CY) << 8,
    { (PEN_NONE << 4) | PEN_FLOOR, 0, 0, 0, 0, 0, 0, 0 }
};

/* One SCB per visible billboard, so they can coexist as distinct members of
** the single master chain (each with its own data/hpos/vpos/size). The shared
** control and penpal bytes are initialised once in main(); project_enemies()
** fills and links the first draw_n of these each frame. */
static SCB_REHV_PAL guardscb[NENEMY];

static SCB_REHV_PAL gun_scb = {
    BPP_4 | TYPE_NORMAL, LITERAL | REHV, NO_COLLIDE,
    0, gun_img, 66, 58, 0x0200, 0x0200,
    { (PEN_NONE << 4) | PEN_GUN, (PEN_GRIP << 4) | PEN_NONE, 0, 0, 0, 0, 0, 0 }
};

static SCB_REHV_PAL flash_scb = {
    BPP_4 | TYPE_NORMAL, LITERAL | REHV, NO_COLLIDE,
    0, flash_img, 74, 44, 0x0200, 0x0200,
    { (PEN_NONE << 4) | PEN_FLASH, 0, 0, 0, 0, 0, 0, 0 }
};

/* Status-bar panel (full width) + a 1px highlight along its top edge. */
static SCB_REHV_PAL hud_scb = {
    BPP_4 | TYPE_NORMAL, LITERAL | REHV, NO_COLLIDE,
    0, solid_img, 0, HUD_Y, 0x5000, (unsigned)HUD_H << 8,
    { (PEN_NONE << 4) | PEN_HUD, 0, 0, 0, 0, 0, 0, 0 }
};

static SCB_REHV_PAL hudlt_scb = {
    BPP_4 | TYPE_NORMAL, LITERAL | REHV, NO_COLLIDE,
    0, solid_img, 0, HUD_Y, 0x5000, 0x0100,
    { (PEN_NONE << 4) | PEN_HUDLT, 0, 0, 0, 0, 0, 0, 0 }
};

/* Status-bar face (centred in the panel). */
static SCB_REHV_PAL face_scb = {
    BPP_4 | TYPE_NORMAL, LITERAL | REHV, NO_COLLIDE,
    0, face_img, 72, HUD_Y + 3, 0x0100, 0x0100,
    { (PEN_NONE << 4) | PEN_ESKIN, (PEN_GRIP << 4) | PEN_NONE, 0, 0, 0, 0, 0, 0 }
};

/* ------------------------------------------------------------------ */
/* Game state                                                          */
/* ------------------------------------------------------------------ */

static int  posX, posY;             /* 8.8 cell coords            */
static unsigned char ang;           /* 0..255 = full circle       */
static int  dirX, dirY;             /* 8.8 unit vector            */
static int  planeX, planeY;         /* camera plane (FOV scaled)  */

static unsigned int zbuf[NCOL];     /* per-column wall perp dist  */

typedef struct {
    int  x, y;                      /* 8.8 cell coords            */
    unsigned char alive;
    unsigned char hp;
    unsigned char hurtflash;        /* attack-frame timer         */
    unsigned char cool;             /* attack cooldown            */
} Enemy;

static Enemy enemy[NENEMY];

static const unsigned char espawn[NENEMY][2] = {
    { 8,  7}, {12, 11}, { 3, 13}, {13,  4}, { 5,  9}, {10,  2}
};

/* Pre-projected billboard draw list (built by project_enemies, far->near,
** already depth-tested against the wall z-buffer). Kept separate from the
** draw pass so all Suzy math finishes before any sprite is launched. */
static unsigned char draw_n;   /* visible billboards -> guardscb[0..draw_n-1] */

static int  health;
static unsigned int score;
static unsigned char alive_cnt;
static unsigned char wave;
static unsigned char state;
static unsigned char fire_cool;
static unsigned char flash_t;       /* muzzle flash frames        */
static unsigned char gunkick;       /* recoil offset frames       */
static unsigned char faceflash;     /* hurt-face timer            */

static unsigned char joy, prev_joy, pressed;
static unsigned char show_text = 1;     /* Opt 1 toggles HUD/text     */

/* Opt 2 quality toggle (design 9.3 item 9): 80 crisp or 40 fat columns. */
static unsigned char lowres;            /* nonzero: NCOL_LO wide columns  */
static unsigned char ncol = NCOL;       /* active column count            */
static unsigned char colshift = 1;      /* screen x -> column: sx>>colshift */
static int  camstep = CAMSTEP;          /* 8.8 camera-plane step / column */

/* HUD strings. */
static char hud_wave[]  = "WV 01";
static char hud_score[] = "SC 000";
static char hud_hp[]    = "HP 100";
static char hud_left[]  = "EN 0";

/* ------------------------------------------------------------------ */

static unsigned char walkable (int x, int y)
{
    int cx = x >> 8;
    int cy = y >> 8;
    if (cx < 0 || cx >= MAPW || cy < 0 || cy >= MAPH) return 0;
    return worldmap[(cy << 4) | cx] == 0;
}

static void fmt2 (unsigned int v, char* p)
{
    p[0] = '0' + (char)((v / 10) % 10);
    p[1] = '0' + (char)(v % 10);
}

static void fmt3 (unsigned int v, char* p)
{
    p[0] = '0' + (char)((v / 100) % 10);
    p[1] = '0' + (char)((v / 10) % 10);
    p[2] = '0' + (char)(v % 10);
}

static unsigned char raydirty;      /* view angle changed: ray cache stale */

static void update_camera (void)
{
    dirX = COSINE (ang);
    dirY = SINE (ang);
    planeX = (int)(((long)(-dirY) * FOV) >> 8);
    planeY = (int)(((long)dirX * FOV) >> 8);
    raydirty = 1;                   /* cast_walls() rebuilds the ray cache */
}

/* Configure the wall run for the current quality setting: column count,
** width, spacing and the intra-run .next links. The static sprctl/data
** fields cover all NCOL SCBs (set once in main()), so flipping back and
** forth just relinks the first ncol of them. */
static void set_quality (void)
{
    unsigned char i;
    if (lowres) { ncol = NCOL_LO; colshift = 2; camstep = CAMSTEP_LO; }
    else        { ncol = NCOL;    colshift = 1; camstep = CAMSTEP;    }
    for (i = 0; i < ncol; ++i) {
        wallscb[i].hpos  = (int)i << colshift;
        wallscb[i].hsize = lowres ? 0x0200 : 0x0100;  /* 2px src -> COLW */
        wallscb[i].next  = (i + 1 < ncol) ? (char*)&wallscb[i + 1] : 0;
    }
    raydirty = 1;                   /* column rays changed: full recast */
}

/* ------------------------------------------------------------------ */

static void spawn_wave (void)
{
    unsigned char i;
    for (i = 0; i < NENEMY; ++i) {
        enemy[i].x = ((int)espawn[i][0] << 8) + 128;
        enemy[i].y = ((int)espawn[i][1] << 8) + 128;
        enemy[i].alive = 1;
        enemy[i].hp = ENEMY_HP;
        enemy[i].hurtflash = 0;
        enemy[i].cool = 0;
    }
    alive_cnt = NENEMY;
}

static void new_game (void)
{
    posX  = (2 << 8) + 128;
    posY  = (2 << 8) + 128;
    ang   = 32;                     /* look into the maze         */
    health = 100;
    score = 0;
    wave  = 1;
    fire_cool = 0;
    flash_t = 0;
    gunkick = 0;
    faceflash = 0;
    state = ST_PLAY;
    update_camera ();
    spawn_wave ();
}

/* ------------------------------------------------------------------ */
/* Player movement + actions                                           */
/* ------------------------------------------------------------------ */

static void try_move (int nx, int ny)
{
    /* axis-separated so we slide along walls */
    if (walkable (nx, posY)) posX = nx;
    if (walkable (posX, ny)) posY = ny;
}

static void update_player (void)
{
    if (joy & JOY_LEFT_MASK) {
        if (joy & JOY_BTN_2_MASK) {                 /* strafe left */
            try_move (posX + (int)(((long)dirY * MOVESPEED) >> 8),
                      posY - (int)(((long)dirX * MOVESPEED) >> 8));
        } else {
            ang -= TURNSPEED;
            update_camera ();
        }
    }
    if (joy & JOY_RIGHT_MASK) {
        if (joy & JOY_BTN_2_MASK) {                 /* strafe right */
            try_move (posX - (int)(((long)dirY * MOVESPEED) >> 8),
                      posY + (int)(((long)dirX * MOVESPEED) >> 8));
        } else {
            ang += TURNSPEED;
            update_camera ();
        }
    }
    if (joy & JOY_UP_MASK) {
        try_move (posX + (int)(((long)dirX * MOVESPEED) >> 8),
                  posY + (int)(((long)dirY * MOVESPEED) >> 8));
    }
    if (joy & JOY_DOWN_MASK) {
        try_move (posX - (int)(((long)dirX * MOVESPEED) >> 8),
                  posY - (int)(((long)dirY * MOVESPEED) >> 8));
    }

    if (fire_cool) --fire_cool;
    if ((joy & JOY_BTN_1_MASK) && fire_cool == 0) {
        fire_cool = FIRE_COOL;
        flash_t = 3;
        gunkick = 4;
        sfx_shot ();

        /* hitscan: nearest alive enemy under the crosshair, not behind
        ** a wall (depth-tested against last frame's zbuf). The screen-x
        ** projection is Suzy's fused multiply-divide. */
        {
            unsigned char i, best = 0xFF;
            int bestdepth = 0x7FFF;
            int projh = PROJH;                      /* var so !*!/ fuses */
            for (i = 0; i < NENEMY; ++i) {
                int dx, dy, depthC, lat, sx, col, proj;
                if (!enemy[i].alive) continue;
                dx = enemy[i].x - posX;
                dy = enemy[i].y - posY;
                depthC = (int)(((long)dx * dirX + (long)dy * dirY) >> 8);
                if (depthC <= 16) continue;         /* behind / too near */
                lat = (int)((-(long)dx * dirY + (long)dy * dirX) >> 8);
                proj = lat !* projh !/ depthC;      /* Suzy fused muldiv */
                sx = SCREEN_W / 2 + proj;
                if (sx < SCREEN_W / 2 - AIM_TOL ||
                    sx > SCREEN_W / 2 + AIM_TOL) continue;
                col = sx >> colshift;                /* /COLW */
                if (col < 0) col = 0;
                if (col >= ncol) col = ncol - 1;
                if ((unsigned)(depthC >> 4) >= zbuf[col]) continue; /* wall */
                if (depthC < bestdepth) { bestdepth = depthC; best = i; }
            }
            if (best != 0xFF) {
                if (--enemy[best].hp == 0) {
                    enemy[best].alive = 0;
                    score += 1;
                    sfx_kill ();
                    if (--alive_cnt == 0) state = ST_WIN;
                } else {
                    enemy[best].hurtflash = 4;
                    sfx_kill ();
                }
            }
        }
    }

    if (gunkick) --gunkick;
    if (faceflash) --faceflash;
}

/* ------------------------------------------------------------------ */
/* Enemy AI: wake near the player, walk toward them, bite on contact.  */
/* ------------------------------------------------------------------ */

static void update_enemies (void)
{
    unsigned char i;
    for (i = 0; i < NENEMY; ++i) {
        int dx, dy, adx, ady, nx, ny;
        Enemy* e = &enemy[i];
        if (!e->alive) continue;
        if (e->hurtflash) --e->hurtflash;
        if (e->cool) --e->cool;

        dx = posX - e->x;
        dy = posY - e->y;
        adx = dx < 0 ? -dx : dx;
        ady = dy < 0 ? -dy : dy;

        if (adx + ady > WAKE_DIST) continue;        /* still asleep */

        if (adx + ady <= ATTACK_DIST) {
            e->hurtflash = 6;                       /* show attack frame */
            if (e->cool == 0) {
                e->cool = 24;
                health -= 6;
                faceflash = 8;
                sfx_hurt ();
                if (health <= 0) { health = 0; state = ST_OVER; }
            }
            continue;
        }

        /* step toward the player, axis-separated wall avoidance */
        nx = e->x + (dx > 0 ? ENEMY_SPEED : -ENEMY_SPEED);
        ny = e->y + (dy > 0 ? ENEMY_SPEED : -ENEMY_SPEED);
        if (walkable (nx, e->y)) e->x = nx;
        if (walkable (e->x, ny)) e->y = ny;
    }
}

/* ------------------------------------------------------------------ */
/* Reciprocal lookup tables (design/LYNX_RAYCASTER_DRAW_DESIGN.md 8.2/8.3).*/
/*                                                                     */
/* cast_walls() used three Suzy divides per column (240/frame): the two */
/* delta-distance reciprocals ABSCALE/ar and the slice height PROJK/perp.*/
/* A reciprocal is nonlinear so it cannot be stepped incrementally the  */
/* way the ray direction is, but it can be precomputed once and looked  */
/* up - the same "divide once" idea DDA is built on. Both delta-distance */
/* divides are the SAME map (ABSCALE/ar, DDMAX-clamped) so they share    */
/* recip[]; slicelut[] holds PROJK/perp clamped to [1,MAXSLICE]. Built   */
/* once at startup (build_tables), so the frame loop issues zero divides */
/* here - only the in-cell first-step Suzy multiplies remain.           */
/* ------------------------------------------------------------------ */

static unsigned char recip[RECIP_MAX + 1];  /* ABSCALE/ar, DDMAX-clamped  */
static unsigned char slicelut[SLICE_N];      /* PROJK/perp, indexed perp>>1*/
static unsigned char vposlut[SLICE_N];       /* (VIEW_H - slicelut[i]) / 2 */

static void build_tables (void)
{
    unsigned int a, q;

    recip[0] = DDMAX;                        /* ar<16 never used; keep safe */
    for (a = 1; a <= RECIP_MAX; ++a) {
        q = ABSCALE / a;
        recip[a] = (q > DDMAX) ? DDMAX : (unsigned char)q;
    }

    /* slicelut[i] answers PROJK/perp for perp in {2i, 2i+1} (the low bit is
    ** dropped by the perp>>1 index); i==0 stands in for the clamped perp>=1. */
    for (a = 0; a < SLICE_N; ++a) {
        unsigned int perp = a ? (a << 1) : 1;
        q = PROJK / perp;
        if (q > MAXSLICE) q = MAXSLICE;
        if (q < 1) q = 1;
        slicelut[a] = (unsigned char)q;
        /* parallel table: the wall slice's screen row for that height, so
        ** cast_walls() skips the signed (VIEW_H - lh) / 2 per column
        ** (design 9.1 item 4). */
        vposlut[a] = (unsigned char)((VIEW_H - q) / 2);
    }
}

/* ------------------------------------------------------------------ */
/* Raycast the 80 wall columns into the chained wallscb[] + zbuf[].    */
/*                                                                     */
/* Second optimization pass (design 9.1/9.2):                          */
/*                                                                     */
/*  - The per-column ray quantities that depend only on the view ANGLE */
/*    (delta-distances and step directions) live in a cache rebuilt    */
/*    only when the player turns (rebuild_raycache). Walk-only frames  */
/*    skip the whole 32-bit camera-plane ramp, the small-ray clamps,   */
/*    the abs and both recip[] lookups for every column.               */
/*  - If neither the position nor the angle changed, the cast is       */
/*    skipped outright: zbuf[] and every wall SCB still hold exactly   */
/*    last frame's (correct) values.                                   */
/*  - The DDA inner loop is add/index/load/test only: flat map index   */
/*    stepped +/-1 or +/-16, no bounds tests (see the worldmap border  */
/*    invariant), and the hot scalars are __zeropage statics instead   */
/*    of cc65 stack-frame locals.                                      */
/*                                                                     */
/* The in-cell first-step is still a Suzy multiply, and no sprite is   */
/* launched here, so the math unit is exclusive.                       */
/* ------------------------------------------------------------------ */

/* Per-column ray cache, a pure function of ang (design 9.2 item 5). */
static unsigned char cddX[NCOL];    /* delta-distance per X cell        */
static unsigned char cddY[NCOL];    /* delta-distance per Y cell        */
static signed char   cstepX[NCOL];  /* flat-map index step on X: +/-1   */
static signed char   cstepY[NCOL];  /* flat-map index step on Y: +/-16  */

static int lastX, lastY;            /* position of the last full cast   */

/* Hot DDA scalars, hoisted to the zero page (design 9.1 item 4): file-
** scope __zeropage statics use the fast zp addressing modes instead of
** stack-frame locals. Plain scratch, only touched from the main loop.  */
static __zeropage__ unsigned int  sideX;
static __zeropage__ unsigned int  sideY;
static __zeropage__ unsigned int  perp;
static __zeropage__ unsigned int  pi;
static __zeropage__ unsigned char ddX;
static __zeropage__ unsigned char ddY;
static __zeropage__ unsigned char mi;
static __zeropage__ unsigned char mi0;
static __zeropage__ unsigned char fracX;
static __zeropage__ unsigned char fracY;
static __zeropage__ unsigned char tex;
static __zeropage__ unsigned char side;
static __zeropage__ unsigned char guard;
static __zeropage__ signed char   stepX;
static __zeropage__ signed char   stepYi;

static void rebuild_raycache (void)
{
    unsigned char col;
    int rayX, rayY;
    unsigned int arX, arY;
    long accX = -((long)planeX << 16);
    long accY = -((long)planeY << 16);
    long dAX  =  (long)planeX * camstep;
    long dAY  =  (long)planeY * camstep;

    for (col = 0; col < ncol; ++col) {
        rayX = dirX + (int)(accX >> 16);
        rayY = dirY + (int)(accY >> 16);
        accX += dAX;
        accY += dAY;
        if (rayX < 16 && rayX > -16) rayX = (rayX < 0) ? -16 : 16;
        if (rayY < 16 && rayY > -16) rayY = (rayY < 0) ? -16 : 16;

        arX = (rayX < 0) ? -rayX : rayX;
        arY = (rayY < 0) ? -rayY : rayY;
        if (arX > RECIP_MAX) arX = RECIP_MAX;
        if (arY > RECIP_MAX) arY = RECIP_MAX;
        cddX[col] = recip[arX];         /* was ABSCALE !/ arX, DDMAX-clamped */
        cddY[col] = recip[arY];         /* frac*dd stays < 2^16             */
        cstepX[col] = (rayX < 0) ? -1 : 1;
        cstepY[col] = (rayY < 0) ? -MAPW : MAPW;
    }
}

static void cast_walls (void)
{
    unsigned char col, pen;
    SCB_REHV_PAL* s;

    if (raydirty) {
        rebuild_raycache ();
        raydirty = 0;
    } else if (posX == lastX && posY == lastY) {
        /* Idle skip (design 9.2 item 6): nothing the cast depends on     */
        /* changed, so zbuf[] and the wall SCBs are still exactly right.  */
        return;
    }
    lastX = posX;
    lastY = posY;

    /* Position-derived values shared by all 80 columns. */
    mi0   = (unsigned char)(((posY >> 8) << 4) | (posX >> 8));
    fracX = (unsigned char)posX;
    fracY = (unsigned char)posY;

    /* Running SCB pointer: &wallscb[col] would be a (software) multiply by
    ** sizeof(SCB_REHV_PAL) per column; ++s is a constant add. */
    s = wallscb;
    for (col = 0; col < ncol; ++col, ++s) {
        ddX    = cddX[col];
        ddY    = cddY[col];
        stepX  = cstepX[col];
        stepYi = cstepY[col];

        /* first-step distances: Suzy multiply, >>8 back to step units */
        if (stepX < 0) sideX = (fracX !* ddX) >> 8;
        else           sideX = ((256 - fracX) !* ddX) >> 8;
        if (stepYi < 0) sideY = (fracY !* ddY) >> 8;
        else            sideY = ((256 - fracY) !* ddY) >> 8;

        /* DDA walk. No bounds tests: the worldmap border ring is solid   */
        /* wall (see the invariant at its definition), so every ray hits. */
        mi = mi0;
        side = 0;
        tex = 0;
        guard = 40;
        do {
            if (sideX < sideY) { sideX += ddX; mi += stepX;  side = 0; }
            else               { sideY += ddY; mi += stepYi; side = 1; }
            tex = worldmap[mi];
            if (tex) break;
        } while (--guard);

        perp = (side == 0) ? (sideX - ddX) : (sideY - ddY);
        if (perp < 1) perp = 1;
        zbuf[col] = perp;

        pi = perp >> 1;                             /* was PROJK !/ perp   */
        if (pi >= SLICE_N) pi = SLICE_N - 1;        /* same clamped lh = 1 */

        /* two wall materials, lit for E/W faces, dark (+1) for N/S */
        pen = (tex >= 2) ? PEN_WALLB : PEN_WALLA;
        if (side == 1) ++pen;

        /* Invariant fields (sprctl0/1, sprcoll, data, hpos, hsize) are set
        ** once in main(); only these vary per frame. The vsize low byte is
        ** always zero (whole-pixel heights) and wallscb[] is BSS, so only
        ** the high byte is ever written (design 9.1 item 4). This store
        ** once miscompiled (member offset dropped - fixed in the compiler,
        ** see design/LYNX_MEMBER_ADDR_CAST_FIX_DESIGN.md); it doubles as a
        ** live regression canary for that fix. */
        s->vpos      = vposlut[pi];
        ((unsigned char*)&s->vsize)[1] = slicelut[pi];
        s->penpal[0] = (PEN_NONE << 4) | pen;
    }
}

/* ------------------------------------------------------------------ */
/* Project the billboard enemies into the draw list (far->near, depth- */
/* tested). All Suzy fused-multiply-divide projection happens here, so */
/* the later draw pass launches sprites with the math unit idle.       */
/* ------------------------------------------------------------------ */

static void project_enemies (void)
{
    int order[NENEMY], depth[NENEMY];
    unsigned char n = 0, i, j;

    for (i = 0; i < NENEMY; ++i) {
        int dx, dy, depthC;
        if (!enemy[i].alive) continue;
        dx = enemy[i].x - posX;
        dy = enemy[i].y - posY;
        depthC = (int)(((long)dx * dirX + (long)dy * dirY) >> 8);
        if (depthC <= 16) continue;                 /* behind camera */
        order[n] = i;
        depth[n] = depthC;
        ++n;
    }
    /* sort descending depth (draw far first) */
    for (i = 0; i + 1 < n; ++i)
        for (j = i + 1; j < n; ++j)
            if (depth[j] > depth[i]) {
                int t = depth[i]; depth[i] = depth[j]; depth[j] = t;
                t = order[i]; order[i] = order[j]; order[j] = t;
            }

    draw_n = 0;
    for (i = 0; i < n; ++i) {
        Enemy* e = &enemy[order[i]];
        int dx = e->x - posX;
        int dy = e->y - posY;
        int depthC = depth[i];
        int lat = (int)((-(long)dx * dirY + (long)dy * dirX) >> 8);
        int projh = PROJH;                                   /* var so !*!/ fuses */
        int proj = lat !* projh !/ depthC;                   /* Suzy fused MDV */
        int sx  = SCREEN_W / 2 + proj;
        /* Billboard height: same reciprocal map as the wall slices, so the
        ** slicelut[] lookup replaces the last mid-frame Suzy divide (design
        ** 9.2 item 7; the VIEW_H clamp is baked into the table). The >>5
        ** index never overruns: depthC < 8192, so depthC>>5 < 256.        */
        int sh  = slicelut[(unsigned)depthC >> 5];  /* was PROJK !/ (depthC>>4) */
        int col;

        if (sh < 6) continue;                       /* too far to see */

        /* occlusion: test the centre column against the wall z-buffer */
        col = sx >> colshift;
        if (col < 0) col = 0;
        if (col >= ncol) col = ncol - 1;
        if ((unsigned)(depthC >> 4) >= zbuf[col]) continue;

        {
            SCB_REHV_PAL* g = &guardscb[draw_n];
            g->data  = enemy[order[i]].hurtflash ? guard_img2 : guard_img;
            g->hpos  = sx - (sh >> 1);
            g->vpos  = (VIEW_H - sh) / 2;
            g->hsize = ((unsigned)sh << 8) / 16;    /* 16px src -> sh */
            g->vsize = g->hsize;
        }
        ++draw_n;
    }

    /* Relink the variable middle of the master sprite chain. Pointer stores
    ** only (no Suzy access), so the all-math-before-the-single-launch contract
    ** still holds. The static joints (sky->walls, hud tail, flash->hud) are set
    ** once in main(); only these three vary with the frame's draw list. */
    {
        unsigned char k;
        for (k = 0; k < draw_n; ++k)
            guardscb[k].next = (k + 1 < draw_n) ? (char*)&guardscb[k + 1]
                                                : (char*)&gun_scb;
        wallscb[ncol - 1].next = draw_n ? (char*)&guardscb[0]
                                        : (char*)&gun_scb;
        gun_scb.next = flash_t ? (char*)&flash_scb : (char*)&hud_scb;
    }
}

/* Status-bar glyph readouts (WV / SC / HP / EN). Gated on show_text. */
static void draw_hud_text (void)
{
    fmt2 (wave, hud_wave + 3);
    fmt3 (score, hud_score + 3);
    fmt3 ((unsigned)health, hud_hp + 3);
    fmt2 (alive_cnt, hud_left + 3);

    gfx_setcolor (PEN_TEXT);
    gfx_outtextxy (4, HUD_Y + 2, hud_wave);
    gfx_outtextxy (4, HUD_Y + 12, hud_score);
    gfx_setcolor (health <= 25 ? PEN_WARN : PEN_TEXT);
    gfx_outtextxy (110, HUD_Y + 2, hud_hp);
    gfx_setcolor (PEN_TEXT);
    gfx_outtextxy (110, HUD_Y + 12, hud_left);
}

static void draw (void)
{
    while (gfx_busy ()) {}

    /* No screen clear: the sky, floor and HUD-panel chain members are opaque
    ** and cover every row between them (design 9.1 item 1). */

    /* Per-frame field updates for chain members that are not touched by
    ** project_enemies() (no Suzy math here — just field/pointer stores). The
    ** gun kicks down on recoil; the face shows its hurt frame on faceflash. */
    gun_scb.vpos  = 58 + (gunkick ? 4 : 0);
    face_scb.data = faceflash ? face_img2 : face_img;

    /* One launch walks the whole master chain, back-to-front:
    ** sky -> floor -> walls -> [enemies] -> gun -> [flash] -> hud
    ** panel/edge/face. The variable joints were relinked in
    ** project_enemies(). */
    gfx_sprite (&sky_scb);
    if (flash_t) --flash_t;         /* flash was linked in for this frame */

    /* All glyph text (crosshair, HUD readouts, banners) is drawn last and
    ** gated on show_text so Opt 1 flips to a clean, glyph-free view. */
    if (show_text) {
        gfx_setcolor (PEN_TEXT);
        gfx_outtextxy (SCREEN_W / 2 - 4, VIEW_CY - 4, "+");   /* crosshair */

        draw_hud_text ();

        if (state == ST_OVER) {
            gfx_setcolor (PEN_WARN);
            gfx_outtextxy (44, 30, "GAME OVER");
            gfx_setcolor (PEN_TEXT);
            gfx_outtextxy (28, 44, "A = NEW GAME");
        } else if (state == ST_WIN) {
            gfx_setcolor (PEN_TEXT);
            gfx_outtextxy (32, 30, "WAVE CLEAR!");
            gfx_outtextxy (28, 44, "A = NEXT WAVE");
        }
    }

    gfx_updatedisplay ();
}

/* ------------------------------------------------------------------ */

void main (void)
{
    unsigned char i;

    gfx_init ();
    CLI ();
    while (gfx_busy ()) {}

    build_tables ();                /* reciprocal LUTs for cast_walls()   */

    /* Initialise every wall-SCB field that never changes; cast_walls()
    ** rewrites only vpos, the vsize high byte and penpal[0]. The chain
    ** links, hpos spacing and hsize depend on the Opt 2 quality setting
    ** and are (re)built by set_quality(); the wall-run tail
    ** (wallscb[ncol-1].next) is a variable joint of the master chain and
    ** is relinked each frame by project_enemies(). */
    for (i = 0; i < NCOL; ++i) {
        SCB_REHV_PAL* s = &wallscb[i];
        s->sprctl0 = BPP_4 | TYPE_NORMAL;
        s->sprctl1 = LITERAL | REHV;
        s->sprcoll = NO_COLLIDE;
        s->data    = solid_img;
    }
    set_quality ();                 /* hpos/hsize/.next for 80-col mode */

    /* Shared control/penpal bytes for the billboard SCBs (data/pos/size and
    ** the .next links are filled per frame by project_enemies()). */
    for (i = 0; i < NENEMY; ++i) {
        guardscb[i].sprctl0   = BPP_4 | TYPE_NORMAL;
        guardscb[i].sprctl1   = LITERAL | REHV;
        guardscb[i].sprcoll   = NO_COLLIDE;
        guardscb[i].penpal[0] = (PEN_NONE << 4) | PEN_EUNI;
        guardscb[i].penpal[1] = (PEN_ESKIN << 4) | PEN_NONE;
    }

    /* Static joints of the single master sprite chain (the whole frame is one
    ** gfx_sprite(&sky_scb) launch). Back-to-front z-order:
    **   sky -> floor -> walls -> [enemies] -> gun -> [flash]
    **       -> hud panel -> edge -> face
    ** Only the wall tail, the enemy links and gun->{flash|hud} vary per frame
    ** (set in project_enemies()); everything else is fixed here. */
    sky_scb.next   = (char*)&floor_scb;
    floor_scb.next = (char*)&wallscb[0];
    flash_scb.next = (char*)&hud_scb;   /* only reached when flash is linked in */
    hud_scb.next   = (char*)&hudlt_scb;
    hudlt_scb.next = (char*)&face_scb;
    face_scb.next  = 0;                 /* end of chain */

    MIKEY.mstereo = 0x00;
    snd_silence (&MIKEY.channel_a);
    snd_silence (&MIKEY.channel_b);
    snd_silence (&MIKEY.channel_c);
    snd_silence (&MIKEY.channel_d);

    pal_init ();
    gfx_setpalette (pal);
    gfx_setframerate (60);
    gfx_setcollisiondetection (0);

    new_game ();
    prev_joy = 0;

    for (;;) {
        joy = (unsigned char)joy_read ();
        pressed = joy & (unsigned char)~prev_joy;
        prev_joy = joy;

        if (pressed & JOY_OPT1_MASK) show_text ^= 1;   /* clean-view toggle */
        if (pressed & JOY_OPT2_MASK) {                 /* quality toggle    */
            lowres ^= 1;
            set_quality ();
        }

        if (state == ST_PLAY) {
            update_player ();
            update_enemies ();
        } else if (pressed & JOY_BTN_1_MASK) {
            if (state == ST_WIN) {
                ++wave;
                spawn_wave ();
                state = ST_PLAY;
            } else {
                new_game ();
            }
        }

        cast_walls ();
        project_enemies ();
        sfx_update ();
        draw ();
    }
}
