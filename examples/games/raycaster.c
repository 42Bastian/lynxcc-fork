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
**      the billboard enemies and the whole status-bar panel are
**      scaled sprites too.
**
**   2. Suzy hardware math. The DDA inner loop needs a divide per
**      column for the delta-distances (`ABSCALE !/ arX`) and another
**      for the slice height (`PROJK !/ perp`); the wall-hit distance
**      inside a cell is a hardware multiply (`!*`); the billboard and
**      hitscan projections are the FUSED multiply-divide the fork
**      recognises as one op (`lat !* PROJH !/ depth`). All of it runs
**      in the main loop (never in IRQ, per design/LYNX_CODEGEN_DESIGN.md
**      2.6) so the math unit is never contended - and all of it runs
**      BEFORE any sprite is launched, since the sprite engine shares
**      Suzy's math registers.
**
**   3. Cheap per-frame ray setup. The camera-plane sweep across the 80
**      columns is a linear ramp, so instead of a divide + multiply per
**      column the ray direction is stepped INCREMENTALLY with a 32-bit
**      add (accX/accY), removing 80 software divides and 160 long
**      multiplies from every frame.
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
** it's game over (A restarts).
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

#define MAPW        16
#define MAPH        16

#define FOV         169             /* 0.66 * 256 (camera-plane scale)*/
#define PROJK       1280            /* slice height = PROJK / perp     */
#define PROJH       121             /* horizontal billboard projection */
#define ABSCALE     4096            /* DDA delta-dist numerator        */
#define DDMAX       255             /* clamp so fracX*ddX fits 16 bits */
#define MAXSLICE    VIEW_H          /* wall never overdraws the HUD    */

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
/* Map. 0 = open, 1..3 = wall types (folded to two materials).         */
/* ------------------------------------------------------------------ */

static const unsigned char worldmap[MAPH][MAPW] = {
    {1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2},
    {1,0,0,0,3,3,0,0,0,0,2,2,0,0,0,2},
    {1,0,0,0,3,0,0,0,0,0,0,2,0,0,0,2},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2},
    {1,0,0,0,0,0,1,1,1,0,0,0,0,0,0,2},
    {1,0,0,0,0,0,1,0,1,0,0,0,3,3,0,2},
    {1,0,0,0,0,0,1,0,0,0,0,0,0,3,0,3},
    {3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3},
    {3,0,0,2,2,0,0,0,0,0,0,0,0,0,0,1},
    {3,0,0,2,0,0,0,3,3,3,0,0,0,0,0,1},
    {3,0,0,0,0,0,0,0,0,0,0,0,1,1,0,1},
    {3,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1},
    {3,0,0,0,1,1,0,0,0,0,0,0,0,0,0,1},
    {3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {3,3,3,3,3,1,1,1,1,1,2,2,2,2,1,1},
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

static SCB_REHV_PAL guard_scb = {
    BPP_4 | TYPE_NORMAL, LITERAL | REHV, NO_COLLIDE,
    0, guard_img, 0, 0, 0x0100, 0x0100,
    { (PEN_NONE << 4) | PEN_EUNI, (PEN_ESKIN << 4) | PEN_NONE, 0, 0, 0, 0, 0, 0 }
};

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
static unsigned char draw_n;
static unsigned char draw_data[NENEMY];   /* frame: normal / attack     */
static int           draw_x[NENEMY];       /* SCB hpos                   */
static int           draw_y[NENEMY];       /* SCB vpos                   */
static unsigned int  draw_s[NENEMY];       /* SCB hsize == vsize scale   */

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
    return worldmap[cy][cx] == 0;
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

static void update_camera (void)
{
    dirX = COSINE (ang);
    dirY = SINE (ang);
    planeX = (int)(((long)(-dirY) * FOV) >> 8);
    planeY = (int)(((long)dirX * FOV) >> 8);
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
                col = sx >> 1;                       /* /COLW */
                if (col < 0) col = 0;
                if (col >= NCOL) col = NCOL - 1;
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
/* Raycast the 80 wall columns into the chained wallscb[] + zbuf[].    */
/*                                                                     */
/* The ray direction sweeps the camera plane linearly, so rather than  */
/* a per-column divide+multiply it is stepped incrementally: accX/accY */
/* hold planeX/planeY * camX in 16.16 and advance by a constant add    */
/* each column. The two DDA delta-distance divides, the in-cell first- */
/* step multiplies and the slice-height divide are all Suzy hardware   */
/* ops, and no sprite is launched here, so the math unit is exclusive. */
/* ------------------------------------------------------------------ */

static void cast_walls (void)
{
    int col;
    long accX = -((long)planeX << 16);
    long accY = -((long)planeY << 16);
    long dAX  =  (long)planeX * CAMSTEP;
    long dAY  =  (long)planeY * CAMSTEP;

    for (col = 0; col < NCOL; ++col) {
        int rayX, rayY;
        unsigned int arX, arY, ddX, ddY;
        int mapX, mapY;
        signed char stepX, stepY;
        unsigned int sideX, sideY;
        unsigned int fracX, fracY;
        unsigned int perp;
        unsigned int lh;
        unsigned char side, tex, hit, guard, pen;
        SCB_REHV_PAL* s = &wallscb[col];

        rayX = dirX + (int)(accX >> 16);
        rayY = dirY + (int)(accY >> 16);
        accX += dAX;
        accY += dAY;
        if (rayX < 16 && rayX > -16) rayX = (rayX < 0) ? -16 : 16;
        if (rayY < 16 && rayY > -16) rayY = (rayY < 0) ? -16 : 16;

        arX = (rayX < 0) ? -rayX : rayX;
        arY = (rayY < 0) ? -rayY : rayY;
        ddX = ABSCALE !/ arX;                       /* Suzy divide */
        ddY = ABSCALE !/ arY;
        if (ddX > DDMAX) ddX = DDMAX;               /* keep frac*dd < 2^16 */
        if (ddY > DDMAX) ddY = DDMAX;

        mapX = posX >> 8;
        mapY = posY >> 8;
        fracX = (unsigned int)(posX & 255);
        fracY = (unsigned int)(posY & 255);
        /* first-step distances: Suzy multiply, >>8 back to step units */
        if (rayX < 0) { stepX = -1; sideX = (fracX !* ddX) >> 8; }
        else          { stepX =  1; sideX = ((256 - fracX) !* ddX) >> 8; }
        if (rayY < 0) { stepY = -1; sideY = (fracY !* ddY) >> 8; }
        else          { stepY =  1; sideY = ((256 - fracY) !* ddY) >> 8; }

        side = 0; tex = 1; hit = 0; guard = 0;
        while (!hit && guard < 40) {
            ++guard;
            if (sideX < sideY) { sideX += ddX; mapX += stepX; side = 0; }
            else               { sideY += ddY; mapY += stepY; side = 1; }
            if (mapX < 0 || mapX >= MAPW || mapY < 0 || mapY >= MAPH) {
                tex = 1; hit = 1; break;
            }
            tex = worldmap[mapY][mapX];
            if (tex) hit = 1;
        }

        perp = (side == 0) ? (sideX - ddX) : (sideY - ddY);
        if (perp < 1) perp = 1;
        zbuf[col] = perp;

        lh = PROJK !/ perp;                         /* Suzy divide */
        if (lh > MAXSLICE) lh = MAXSLICE;
        if (lh < 1) lh = 1;

        /* two wall materials, lit for E/W faces, dark (+1) for N/S */
        pen = (tex >= 2) ? PEN_WALLB : PEN_WALLA;
        if (side == 1) ++pen;

        s->sprctl0 = BPP_4 | TYPE_NORMAL;
        s->sprctl1 = LITERAL | REHV;
        s->sprcoll = NO_COLLIDE;
        s->data    = solid_img;
        s->hpos    = col * COLW;
        s->vpos    = (VIEW_H - (int)lh) / 2;
        s->hsize   = 0x0100;                         /* source already 2px */
        s->vsize   = lh << 8;
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
        int sh  = PROJK !/ (unsigned)(depthC >> 4);          /* Suzy div */
        int col;

        if (sh > VIEW_H) sh = VIEW_H;
        if (sh < 6) continue;                       /* too far to see */

        /* occlusion: test the centre column against the wall z-buffer */
        col = sx >> 1;
        if (col < 0) col = 0;
        if (col >= NCOL) col = NCOL - 1;
        if ((unsigned)(depthC >> 4) >= zbuf[col]) continue;

        draw_data[draw_n] = enemy[order[i]].hurtflash;
        draw_x[draw_n]    = sx - (sh >> 1);
        draw_y[draw_n]    = (VIEW_H - sh) / 2;
        draw_s[draw_n]    = ((unsigned)sh << 8) / 16;   /* 16px src -> sh */
        ++draw_n;
    }
}

/* ------------------------------------------------------------------ */
/* Draw pass: sprites + text only. No Suzy math is issued here, so the */
/* sprite engine (which shares the math registers) runs unobstructed.  */
/* ------------------------------------------------------------------ */

static void draw_enemies (void)
{
    unsigned char i;
    for (i = 0; i < draw_n; ++i) {
        guard_scb.data  = draw_data[i] ? guard_img2 : guard_img;
        guard_scb.hpos  = draw_x[i];
        guard_scb.vpos  = draw_y[i];
        guard_scb.hsize = draw_s[i];
        guard_scb.vsize = draw_s[i];
        gfx_sprite (&guard_scb);
    }
}

static void draw_hud (void)
{
    gfx_sprite (&hud_scb);          /* panel */
    gfx_sprite (&hudlt_scb);        /* top highlight edge */

    face_scb.data = faceflash ? face_img2 : face_img;
    gfx_sprite (&face_scb);

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

    /* floor fills the viewport, ceiling band over the top half */
    gfx_setcolor (PEN_FLOOR);
    gfx_clear ();
    gfx_sprite (&sky_scb);

    /* one chained engine run paints all 80 wall columns */
    gfx_sprite (&wallscb[0]);

    draw_enemies ();

    /* gun viewmodel (kicks down on recoil) */
    gun_scb.vpos = 58 + (gunkick ? 4 : 0);
    gfx_sprite (&gun_scb);
    if (flash_t) {
        gfx_sprite (&flash_scb);
        --flash_t;
    }

    /* crosshair */
    gfx_setcolor (PEN_TEXT);
    gfx_outtextxy (SCREEN_W / 2 - 4, VIEW_CY - 4, "+");

    draw_hud ();

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

    gfx_updatedisplay ();
}

/* ------------------------------------------------------------------ */

void main (void)
{
    unsigned char i;

    gfx_init ();
    CLI ();
    while (gfx_busy ()) {}

    /* chain the wall SCBs once; fields are rewritten each frame */
    for (i = 0; i < NCOL; ++i)
        wallscb[i].next = (i + 1 < NCOL) ? (char*)&wallscb[i + 1] : 0;

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
