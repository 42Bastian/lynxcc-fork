/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in this directory.
*/

/*
** lander.c - Single-file "MOON LANDER" physics game for the Atari Lynx.
**
** A complete little lunar-lander built on the fork's static Lynx graphics and
** sound libraries: a title menu, an instructions screen and a play field with
** Newtonian flight over randomly generated terrain. Rotate the module, fire the
** descent engine against gravity, manage a finite fuel supply and touch down
** gently and upright on a flat landing pad.
**
** Rendering: the lander and its exhaust plume are pre-rendered rotation sprite
** sheets (24 headings each, lander.pcx/flame.pcx -> lander_anim[]/flame_anim[]
** via sp65 --sprite-sheet), so no rotation math runs on the 6502 - the game
** just indexes the table by heading. The terrain columns, landing pads and
** starfield are one solid 8x8 sprite (solidsq.pcx) stretched through each SCB's
** h/v size and recoloured through its pen palette, exactly the one-image-many-
** tints trick tetris.c uses for its blocks. Everything is emitted as a single
** SCB chain drawn with one gfx_sprite() call per frame.
**
** Physics: position and velocity are 7.7 fixed point (128 = one pixel). Gravity
** adds a constant to the vertical velocity each frame; thrust adds an impulse
** along the ship's facing, taken from a 24-entry sin/cos table. As required by
** the Suzy math contract, the !* !/ !% hardware operators used for the HUD read-
** outs and the landing score live in the main loop, never in IRQ context (see
** design/LYNX_CODEGEN_DESIGN.md section 2.6).
**
** Sound (snd engine + sfx pack, see doc/sound.html):
**   channel 0/1  background music - a calm menu loop, or the game melody+bass
**   channel 2    one-shot events - touchdown, crash, menu blips
**   channel 3    the looping thruster roar (sfx_engine) while the engine burns
** The music streams are compiled offline by abccc from landermenu.abc,
** landergame.abc and landerbass.abc (committed beside this file as .s).
**
** Controls:
**   Pad Left/Right  rotate the lander
**   Button A        fire the descent engine (burns fuel)
**   Pause           pause / resume
**   On the menu: Up/Down choose, A selects; on instructions: B returns.
**   After a landing or crash: A fly again, B return to the menu.
**
** Build:  cl65 -Ors -o lander.lnx lander.c landermenu.s landergame.s landerbass.s
*/

#include <lynx/lynx.h>
#include <lynx/gfx.h>
#include <lynx/joystick.h>
#include <lynx/sfx.h>
#include <6502.h>
#include <string.h>

#include "lander_anim.h"
#include "flame_anim.h"
#include "solidsq.h"

/* Compiled ABC music streams (abccc output, committed beside this file). */
extern const unsigned char landermenu_music[];
extern const unsigned char landergame_music[];
extern const unsigned char landerbass_music[];

/* ------------------------------------------------------------------ */
/* Palette. 16 colours; sprite pixel indices map straight through the   */
/* SCB pen palettes to these. Lynx format: 16 green nibbles, then 16     */
/* (blue<<4 | red) bytes.                                                */
/* ------------------------------------------------------------------ */

#define C_BG     0
#define C_HULLL  1      /* hull, lit          */
#define C_HULLD  2      /* hull, shadow/legs  */
#define C_WIN    3      /* window             */
#define C_TERR   4      /* terrain            */
#define C_PAD    5      /* landing pad        */
#define C_FOUT   6      /* flame, outer       */
#define C_FIN    7      /* flame, inner       */
#define C_WHITE  8
#define C_RED    9
#define C_GREEN  10
#define C_HUD    11
#define C_YEL    12
#define C_FDIM   13     /* flame, dim (flicker) */
#define C_PADG   14     /* pad glow           */
#define C_GREY   15

static const unsigned char palette[32] = {
    /* green nibble per pen 0..15 */
    0x0,0xD,0x6,0xE,0x7,0xE,0x8,0xE,0xF,0x2,0xE,0x9,0xE,0x5,0xF,0xA,
    /* (blue<<4 | red) per pen 0..15 */
    0x00,0xED,0x85,0xF4,0x68,0x43,0x2F,0x6F,0xFF,0x2E,0x54,0xF6,0x2F,0x1C,0x99,0xAA
};

/* ------------------------------------------------------------------ */
/* Geometry / tuning                                                   */
/* ------------------------------------------------------------------ */

#define SCRW        160
#define SCRH        102
#define GROUNDBOT   101         /* terrain fills down to here          */

#define NCOL        40          /* terrain columns                     */
#define COLW        4           /* column width, pixels                */

#define SHIP        24          /* sprite cell size                    */
#define SHIP_HALF   12          /* centre offset within the cell       */
#define FOOT        9           /* centre -> footpad, pixels           */
#define SIDE        7           /* centre -> hull edge, pixels         */

#define ANGLES      24          /* headings in the rotation sheet      */

#define FP          7           /* fixed-point shift (128 = 1 px)      */
#define GRAV        3           /* gravity, 7.7 units / frame^2        */
#define THR         7           /* engine thrust, 7.7 units / frame^2  */
#define VMAX        640         /* velocity clamp, 7.7 units           */
#define ROT_CD      3           /* frames between rotation steps       */

#define FUEL_MAX    750
#define LAND_VY     80          /* max safe descent rate (7.7)         */
#define LAND_VX     64          /* max safe drift rate (7.7)           */

/* Game states */
#define ST_MENU     0
#define ST_INSTR    1
#define ST_PLAY     2
#define ST_PAUSE    3
#define ST_LANDED   4
#define ST_CRASH    5

/* Sound channels */
#define CH_MEL      0
#define CH_BASS     1
#define CH_EVT      2
#define CH_THR      3

/* 24-entry sin/cos, scaled by 64. Heading 0 = pointing straight up;
** headings advance clockwise in 15-degree steps. */
static const signed char sintab[ANGLES] = {
      0, 17, 32, 45, 55, 62, 64, 62, 55, 45, 32, 17,
      0,-17,-32,-45,-55,-62,-64,-62,-55,-45,-32,-17
};
static const signed char costab[ANGLES] = {
     64, 62, 55, 45, 32, 17,  0,-17,-32,-45,-55,-62,
    -64,-62,-55,-45,-32,-17,  0, 17, 32, 45, 55, 62
};

/* ------------------------------------------------------------------ */
/* SCB pool: starfield -> terrain columns -> pad glow -> ship -> flame  */
/* ------------------------------------------------------------------ */

#define NSTARS 18
/* Worst case per frame: NSTARS stars + NCOL terrain columns + up to 8 pad-cap
** sprites (pads are 5 + 3 columns) + the ship + the flame, with a little slack. */
#define POOL   (NSTARS + NCOL + 8 + 4)
static SCB_REHV_PAL pool[POOL];

/* ------------------------------------------------------------------ */
/* State                                                               */
/* ------------------------------------------------------------------ */

static unsigned char ground[NCOL];      /* terrain surface y per column */
static unsigned char padid[NCOL];       /* 0 = rock, else pad number    */
static unsigned char pad_w[2];          /* width, in columns, per pad   */

static int  posx, posy;                 /* ship centre, 7.7             */
static int  velx, vely;                 /* velocity, 7.7                */
static unsigned char heading;           /* 0..ANGLES-1                  */
static unsigned char rot_cd;
static unsigned int  fuel;
static unsigned char thrusting, was_thrusting, flick;

static unsigned long total_score;
static unsigned int  land_score;

static unsigned char state, menu_sel;
static unsigned int  rng, tick;
static unsigned int  joy, prev_joy, pressed;

static unsigned char star_x[NSTARS], star_y[NSTARS];

static char buf[8];

/* ------------------------------------------------------------------ */

static unsigned char rnd (void)         /* 8-bit LCG draw */
{
    rng = rng * 33797u + 1u;
    return (unsigned char)(rng >> 8);
}

/* Format v as n decimal digits (leading zeros) using Suzy divide/modulo. */
static void fmtn (unsigned v, char* p, unsigned char n)
{
    p[n] = '\0';
    while (n--) {
        p[n] = '0' + (char)(v !% 10);
        v = v !/ 10;
    }
}

/* ------------------------------------------------------------------ */
/* Sound helpers                                                       */
/* ------------------------------------------------------------------ */

static void music_menu (void)
{
    snd_stop_channel (CH_BASS);
    snd_play (CH_MEL, landermenu_music);
}

static void music_game (void)
{
    snd_play (CH_MEL,  landergame_music);
    snd_play (CH_BASS, landerbass_music);
}

static void music_off (void)
{
    snd_stop_channel (CH_MEL);
    snd_stop_channel (CH_BASS);
    snd_stop_channel (CH_THR);
}

/* ------------------------------------------------------------------ */
/* Terrain                                                             */
/* ------------------------------------------------------------------ */

/* Lay down one flat pad of width w columns at column c, height y. */
static void make_pad (unsigned char id, unsigned char c, unsigned char w,
                      unsigned char y)
{
    unsigned char i;
    pad_w[id - 1] = w;
    for (i = 0; i < w; ++i) {
        ground[c + i] = y;
        padid[c + i]  = id;
    }
}

static void gen_terrain (void)
{
    unsigned char c, h;
    unsigned char p0, p1;

    memset (padid, 0, sizeof padid);

    /* Rolling rock surface: a bounded random walk. */
    h = 60 + (rnd () & 31);
    for (c = 0; c < NCOL; ++c) {
        signed char step = (signed char)((rnd () & 7) - 3);
        h = (unsigned char)(h + step);
        if (h < 52) h = 52;
        if (h > 92) h = 92;
        ground[c] = h;
    }

    /* Two flat pads: a wide one on the left half, a narrow bonus pad on the
    ** right half, each at its column's current height. */
    p0 = 4 + (rnd () % 10);                 /* wide pad, cols 4..13         */
    make_pad (1, p0, 5, ground[p0]);
    p1 = 24 + (rnd () % 10);                /* narrow pad, cols 24..33      */
    make_pad (2, p1, 3, ground[p1]);
}

static unsigned char ground_at (int px)
{
    int c = px / COLW;
    if (c < 0) c = 0;
    if (c >= NCOL) c = NCOL - 1;
    return ground[c];
}

static unsigned char pad_at (int px)
{
    int c = px / COLW;
    if (c < 0 || c >= NCOL) return 0;
    return padid[c];
}

/* ------------------------------------------------------------------ */
/* New game / round                                                    */
/* ------------------------------------------------------------------ */

static void new_round (void)
{
    gen_terrain ();
    posx      = 80 << FP;
    posy      = 12 << FP;
    velx      = (int)((rnd () & 127)) - 64;     /* small random drift */
    vely      = 0;
    heading   = 0;
    rot_cd    = 0;
    fuel      = FUEL_MAX;
    thrusting = 0;
    was_thrusting = 0;
    state     = ST_PLAY;
    music_game ();
}

/* ------------------------------------------------------------------ */
/* Physics                                                             */
/* ------------------------------------------------------------------ */

static void physics (void)
{
    int sxp, syp, gmin, gl, gc, gr;

    /* Rotation (auto-repeat while held). */
    if (rot_cd) --rot_cd;
    if (!rot_cd) {
        if (joy & JOY_LEFT_MASK) {
            heading = (unsigned char)((heading + ANGLES - 1) % ANGLES);
            rot_cd = ROT_CD;
        } else if (joy & JOY_RIGHT_MASK) {
            heading = (unsigned char)((heading + 1) % ANGLES);
            rot_cd = ROT_CD;
        }
    }

    /* Thrust along the ship's facing while A is held and fuel remains. */
    thrusting = 0;
    if ((joy & JOY_BTN_1_MASK) && fuel) {
        velx += (THR * sintab[heading]) >> 6;
        vely -= (THR * costab[heading]) >> 6;
        --fuel;
        thrusting = 1;
    }

    /* Gravity. */
    vely += GRAV;

    if (velx >  VMAX) velx =  VMAX;
    if (velx < -VMAX) velx = -VMAX;
    if (vely >  VMAX) vely =  VMAX;
    if (vely < -VMAX) vely = -VMAX;

    /* Thruster roar: start on the rising edge, stop when it ends. */
    if (thrusting && !was_thrusting) sfx_engine (CH_THR);
    if (!thrusting && was_thrusting) snd_stop_channel (CH_THR);
    was_thrusting = thrusting;

    /* Integrate. */
    posx += velx;
    posy += vely;

    /* Side walls: stop dead against them. */
    if (posx < (SIDE << FP))            { posx = SIDE << FP;            velx = 0; }
    if (posx > ((SCRW - SIDE) << FP))   { posx = (SCRW - SIDE) << FP;  velx = 0; }
    if (posy < 0)                       { posy = 0;                    if (vely < 0) vely = 0; }

    /* Ground contact under the footpads. */
    sxp = posx >> FP;
    syp = posy >> FP;
    gl = ground_at (sxp - SIDE);
    gc = ground_at (sxp);
    gr = ground_at (sxp + SIDE);
    gmin = gc;
    if (gl < gmin) gmin = gl;
    if (gr < gmin) gmin = gr;

    if (syp + FOOT >= gmin) {
        unsigned char padok = pad_at (sxp - 5) && pad_at (sxp + 5) &&
                              pad_at (sxp);
        unsigned char level = (heading <= 1 || heading >= ANGLES - 1);
        int avy = vely < 0 ? -vely : vely;
        int avx = velx < 0 ? -velx : velx;

        thrusting = 0;
        snd_stop_channel (CH_THR);
        was_thrusting = 0;
        music_off ();

        if (padok && level && avy < LAND_VY && avx < LAND_VX) {
            /* Score: pad-narrowness bonus (Suzy multiply), fuel bonus (Suzy
            ** divide) and a gentleness bonus. */
            unsigned char w = pad_w[pad_at (sxp) - 1];
            unsigned int bonus = ((unsigned)(6 - w)) !* 150;
            unsigned int fb    = fuel !/ 6;
            land_score  = 300 + bonus + fb
                        + (unsigned int)(LAND_VY - avy)
                        + (unsigned int)(LAND_VX - avx);
            total_score += land_score;
            state = ST_LANDED;
            sfx_land (CH_EVT);
        } else {
            state = ST_CRASH;
            sfx_explosion_large (CH_EVT);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Rendering                                                           */
/* ------------------------------------------------------------------ */

static unsigned pn;                     /* next free pool slot          */
static SCB_REHV_PAL* pprev;

static void chain (SCB_REHV_PAL* s)
{
    if (pprev) pprev->next = (char*)s;
    pprev = s;
}

/* Solid rectangle: the 8x8 sprite stretched to w x h and tinted to col. */
static void solid (int hp, int vp, unsigned w, unsigned h, unsigned char col)
{
    SCB_REHV_PAL* s = &pool[pn++];
    s->sprctl0 = BPP_1 | TYPE_NORMAL;
    s->sprctl1 = PACKED | REHV;
    s->sprcoll = NO_COLLIDE;
    s->data    = (unsigned char*)solidsq;
    s->hpos    = hp;
    s->vpos    = vp;
    s->hsize   = w * 32;                 /* (w * 256) / 8 */
    s->vsize   = h * 32;
    s->penpal[0] = col;                  /* index1 -> col; index0 unused */
    s->penpal[1] = 0; s->penpal[2] = 0; s->penpal[3] = 0;
    s->penpal[4] = 0; s->penpal[5] = 0; s->penpal[6] = 0; s->penpal[7] = 0;
    chain (s);
}

static void sheet (const unsigned char* data, int hp, int vp,
                   unsigned char p0, unsigned char p1)
{
    SCB_REHV_PAL* s = &pool[pn++];
    s->sprctl0 = BPP_2 | TYPE_NORMAL;
    s->sprctl1 = PACKED | REHV;
    s->sprcoll = NO_COLLIDE;
    s->data    = (unsigned char*)data;
    s->hpos    = hp;
    s->vpos    = vp;
    s->hsize   = 0x0100;
    s->vsize   = 0x0100;
    s->penpal[0] = p0; s->penpal[1] = p1;
    s->penpal[2] = 0; s->penpal[3] = 0;
    s->penpal[4] = 0; s->penpal[5] = 0; s->penpal[6] = 0; s->penpal[7] = 0;
    chain (s);
}

static void draw_world (void)
{
    unsigned char c, i;
    int sxp, syp;

    while (gfx_busy ()) {}
    gfx_setcolor (C_BG);
    gfx_clear ();

    pn = 0;
    pprev = (SCB_REHV_PAL*)0;

    /* Starfield. */
    for (i = 0; i < NSTARS; ++i)
        solid (star_x[i], star_y[i], 1, 1, C_WHITE);

    /* Terrain columns and pad caps. */
    for (c = 0; c < NCOL; ++c) {
        int gy = ground[c];
        solid (c * COLW, gy, COLW + 1, GROUNDBOT - gy + 1, C_TERR);
        if (padid[c])
            solid (c * COLW, gy - 1, COLW + 1, 2,
                   padid[c] == 2 ? C_PADG : C_PAD);
    }

    sxp = posx >> FP;
    syp = posy >> FP;

    /* Exhaust plume (behind the hull), recoloured for a flicker. */
    if (thrusting) {
        if (flick)
            sheet (flame_anim[heading], sxp - SHIP_HALF, syp - SHIP_HALF,
                   C_FIN, C_FOUT);
        else
            sheet (flame_anim[heading], sxp - SHIP_HALF, syp - SHIP_HALF,
                   C_FOUT, C_FDIM);
    }

    /* The lander (identity penpal: 0/1 here, 2/3 below). */
    {
        SCB_REHV_PAL* s = &pool[pn++];
        s->sprctl0 = BPP_2 | TYPE_NORMAL;
        s->sprctl1 = PACKED | REHV;
        s->sprcoll = NO_COLLIDE;
        s->data    = (unsigned char*)lander_anim[heading];
        s->hpos    = sxp - SHIP_HALF;
        s->vpos    = syp - SHIP_HALF;
        s->hsize   = 0x0100;
        s->vsize   = 0x0100;
        s->penpal[0] = (0 << 4) | C_HULLL;        /* 0->clear, 1->hull lit  */
        s->penpal[1] = (C_HULLD << 4) | C_WIN;    /* 2->shadow, 3->window   */
        s->penpal[2] = 0; s->penpal[3] = 0;
        s->penpal[4] = 0; s->penpal[5] = 0;
        s->penpal[6] = 0; s->penpal[7] = 0;
        chain (s);
    }

    if (pprev) pprev->next = (char*)0;
    gfx_sprite (&pool[0]);

    flick ^= 1;
}

/* Vertical/horizontal speed and safe-colour helper for the HUD. */
static void hud_speed (int x, int y, const char* lbl, int v, int limit)
{
    int a = v < 0 ? -v : v;
    unsigned int scaled = ((unsigned)a) !* 100;
    scaled >>= FP;
    gfx_setcolor (C_HUD);
    gfx_outtextxy (x, y, lbl);
    fmtn (scaled, buf, 3);
    gfx_setcolor (a < limit ? C_GREEN : C_RED);
    gfx_outtextxy (x + 24, y, buf);
}

static void draw_hud (void)
{
    int alt = (int)ground_at (posx >> FP) - (posy >> FP) - FOOT;
    if (alt < 0) alt = 0;

    /* Fuel gauge: a dark 60px trough with a coloured fill sprite. Drawn as
    ** its own tiny SCB chain, separate from the world chain. */
    gfx_setcolor (C_HUD);
    gfx_outtextxy (2, 2, "FUEL");
    {
        static SCB_REHV_PAL bar[2];
        unsigned int w = (fuel * 60u) / FUEL_MAX;
        bar[0].sprctl0 = BPP_1 | TYPE_NORMAL; bar[0].sprctl1 = PACKED | REHV;
        bar[0].sprcoll = NO_COLLIDE; bar[0].data = (unsigned char*)solidsq;
        bar[0].hpos = 27; bar[0].vpos = 3; bar[0].hsize = 60 * 32; bar[0].vsize = 4 * 32;
        bar[0].penpal[0] = C_HULLD; memset (bar[0].penpal + 1, 0, 7);
        bar[1] = bar[0];
        bar[1].hsize = (w ? w : 1) * 32;
        bar[1].penpal[0] = fuel < (FUEL_MAX / 5) ? C_RED : C_GREEN;
        bar[0].next = (char*)&bar[1];
        bar[1].next = (char*)0;
        gfx_sprite (&bar[0]);
    }

    hud_speed (96, 2, "VS", vely, LAND_VY);
    hud_speed (96, 11, "HS", velx, LAND_VX);

    gfx_setcolor (C_HUD);
    gfx_outtextxy (2, 11, "ALT");
    fmtn ((unsigned)alt, buf, 3);
    gfx_setcolor (C_WHITE);
    gfx_outtextxy (26, 11, buf);

    if (state == ST_PAUSE) {
        gfx_setcolor (C_YEL);
        gfx_outtextxy (62, 46, "PAUSED");
    } else if (state == ST_LANDED) {
        gfx_setcolor (C_GREEN);
        gfx_outtextxy (16, 40, "EAGLE HAS LANDED!");
        gfx_setcolor (C_WHITE);
        gfx_outtextxy (40, 52, "SCORE +");
        fmtn (land_score, buf, 5);
        gfx_outtextxy (96, 52, buf);
        gfx_setcolor (C_YEL);
        gfx_outtextxy (36, 66, "A-NEXT   B-MENU");
    } else if (state == ST_CRASH) {
        gfx_setcolor (C_RED);
        gfx_outtextxy (56, 42, "CRASHED!");
        gfx_setcolor (C_WHITE);
        gfx_outtextxy (4, 56, "TOO FAST OR OFF-PAD");
        gfx_setcolor (C_YEL);
        gfx_outtextxy (36, 68, "A-RETRY  B-MENU");
    }

    gfx_setcolor (C_HUD);
    gfx_outtextxy (2, SCRH - 8, "SCORE");
    fmtn ((unsigned)total_score, buf, 5);
    gfx_setcolor (C_WHITE);
    gfx_outtextxy (40, SCRH - 8, buf);

    gfx_updatedisplay ();
}

static void draw_play (void)
{
    draw_world ();
    draw_hud ();
}

static void draw_menu (void)
{
    unsigned char i;

    while (gfx_busy ()) {}
    gfx_setcolor (C_BG);
    gfx_clear ();

    pn = 0; pprev = (SCB_REHV_PAL*)0;
    for (i = 0; i < NSTARS; ++i)
        solid (star_x[i], star_y[i], 1, 1, C_WHITE);
    /* a resting lander on the title screen */
    sheet (lander_anim[0], 118, 60, C_HULLL, 0);
    {
        SCB_REHV_PAL* s = &pool[pn - 1];
        s->penpal[0] = (0 << 4) | C_HULLL;
        s->penpal[1] = (C_HULLD << 4) | C_WIN;
    }
    solid (108, 78, 44, 3, C_PAD);
    if (pprev) pprev->next = (char*)0;
    gfx_sprite (&pool[0]);

    gfx_settextscale (0x0180, 0x0180);
    gfx_setcolor (C_HUD);
    gfx_outtextxy (8, 12, "MOON LANDER");
    gfx_settextscale (0x0100, 0x0100);

    gfx_setcolor (menu_sel == 0 ? C_YEL : C_WHITE);
    gfx_outtextxy (24, 44, menu_sel == 0 ? "> START GAME" : "  START GAME");
    gfx_setcolor (menu_sel == 1 ? C_YEL : C_WHITE);
    gfx_outtextxy (24, 56, menu_sel == 1 ? "> INSTRUCTIONS" : "  INSTRUCTIONS");

    gfx_setcolor (C_GREY);
    gfx_outtextxy (18, 90, "UP/DOWN, A=SELECT");

    gfx_updatedisplay ();
}

static void draw_instructions (void)
{
    while (gfx_busy ()) {}
    gfx_setcolor (C_BG);
    gfx_clear ();

    gfx_setcolor (C_HUD);
    gfx_outtextxy (26, 1, "HOW TO PLAY");

    gfx_setcolor (C_WHITE);
    gfx_outtextxy (2, 14, "Fire the engine to");
    gfx_outtextxy (2, 23, "fight gravity. Land");
    gfx_outtextxy (2, 32, "on a flat PAD, slow");
    gfx_outtextxy (2, 41, "and upright.");

    gfx_setcolor (C_YEL);
    gfx_outtextxy (2, 55, "CONTROLS");
    gfx_setcolor (C_WHITE);
    gfx_outtextxy (2, 66, "LEFT/RIGHT  ROTATE");
    gfx_outtextxy (2, 75, "A           THRUST");
    gfx_outtextxy (2, 84, "PAUSE       PAUSE");

    gfx_setcolor (C_GREY);
    gfx_outtextxy (2, 94, "PRESS B TO GO BACK");

    gfx_updatedisplay ();
}

/* ------------------------------------------------------------------ */

void main (void)
{
    unsigned char i;

    gfx_init ();
    CLI ();
    while (gfx_busy ()) {}
    gfx_setpalette (palette);
    gfx_setframerate (60);
    gfx_setcollisiondetection (0);
    gfx_setfont (GFX_FONT_BITMAP);

    snd_init ();

    rng = 0xACE1;
    for (i = 0; i < NSTARS; ++i) {
        star_x[i] = 4 + (rnd () % 152);
        star_y[i] = 10 + (rnd () % 34);
    }

    total_score = 0;
    state       = ST_MENU;
    menu_sel    = 0;
    prev_joy    = 0;
    music_menu ();

    for (;;) {
        ++tick;
        joy      = joy_read ();
        pressed  = joy & ~prev_joy;
        prev_joy = joy;

        switch (state) {

        case ST_MENU:
            if (pressed & (JOY_UP_MASK | JOY_DOWN_MASK)) {
                menu_sel ^= 1;
                sfx_cursor_move (CH_EVT);
            }
            if (pressed & JOY_BTN_1_MASK) {
                sfx_confirm (CH_EVT);
                if (menu_sel == 0) { rng += tick; new_round (); }
                else               state = ST_INSTR;
            }
            draw_menu ();
            break;

        case ST_INSTR:
            if (pressed & (JOY_BTN_1_MASK | JOY_BTN_2_MASK))
                state = ST_MENU;
            draw_instructions ();
            break;

        case ST_PLAY:
            if (pressed & JOY_PAUSE_MASK) {
                state = ST_PAUSE;
                snd_stop_channel (CH_THR);
                was_thrusting = thrusting = 0;
                snd_pause ();
            } else {
                physics ();
            }
            draw_play ();
            break;

        case ST_PAUSE:
            if (pressed & JOY_PAUSE_MASK) {
                state = ST_PLAY;
                snd_continue ();
            }
            draw_play ();
            break;

        case ST_LANDED:
        case ST_CRASH:
            if (pressed & JOY_BTN_1_MASK) {
                rng += tick;
                new_round ();
            } else if (pressed & JOY_BTN_2_MASK) {
                state = ST_MENU;
                menu_sel = 0;
                music_menu ();
            }
            draw_play ();
            break;
        }
    }
}
