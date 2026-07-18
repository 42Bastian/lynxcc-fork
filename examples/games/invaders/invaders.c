/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in this directory.
*/

/*
** invaders.c - Single-file Space Invaders for the Atari Lynx.
**
** A companion to breakout.c. Where that sample shows off the fork's
** Suzy hardware math operators (!* !/ !%), this one adds two things
** the Lynx does in hardware and almost nothing else of its era could:
**
**   1. Coloured enemies + colour-cycle effects. Every invader sprite
**      is the same 1bpp-ish image; its row colour comes purely from
**      the SCB penpal, and the live RGB behind each pen is rewritten
**      every few frames from a 12-entry rainbow LUT. The result is a
**      rainbow that flows down the formation - zero extra sprite data,
**      just Mikey's 16-entry 12-bit palette (0xFDA0) being repainted.
**
**   2. Sound. Channels C and D play the two-voice Space Invaders theme
**      as compiled snd-engine streams (invtheme.s, snd_play on channels
**      2 and 3); the snd IRQ walks the event streams - patterns, loops
**      and a shared attack envelope - and the streams self-loop. Channels
**      A and B stay hand-driven for the interactive effects (laser and
**      explosion noise; player-death / UFO share channel A). Square tone
**      = feedback tap 0 (0x01); noise = a long tap set (0x3F). Those SFX
**      envelopes run once per frame in the main loop. See invtheme.s and
**      doc/sound.html.
**
** Suzy math contract (design/LYNX_CODEGEN_DESIGN.md section 2.6): all !* !/
** !% sites are in the main loop, never in IRQ context, so the math
** unit is never contended. They lay out the invader grid, do the hit-
** test cell lookup, pace the march and format the score.
**
** Controls: pad left/right moves the cannon, A fires, A restarts.
**
** Build:  cl65 -Ors -o invaders.lnx invaders.c invtheme.s
*/

#include <lynx/lynx.h>
#include <lynx/gfx.h>
#include <lynx/joystick.h>
#include <6502.h>
#include <string.h>

/* The two-voice Space Invaders theme, as compiled snd-engine event streams in
** invtheme.s (extracted from the reference cartridge). Voice A is the lead,
** voice B the counter-voice; each self-loops. They play on channels 2 and 3. */
extern const unsigned char invaders_theme_a[];
extern const unsigned char invaders_theme_b[];

#define THEME_CH_A   2
#define THEME_CH_B   3

/* ------------------------------------------------------------------ */
/* Geometry                                                            */
/* ------------------------------------------------------------------ */

#define SCREEN_W    160
#define SCREEN_H    102

#define INV_COLS    8
#define INV_ROWS    5
#define INV_N       (INV_COLS * INV_ROWS)
#define INV_W       8           /* sprite pixels      */
#define INV_H       8
#define CELL_W      13          /* grid spacing       */
#define CELL_H      10
#define INV_STEP    2           /* march h-step       */
#define INV_DROP    5           /* march down-step    */
#define INV_START_X 18
#define INV_START_Y 14

#define PLAYER_W    11
#define PLAYER_H    8
#define PLAYER_Y    92
#define PLAYER_DX   2

#define BULLET_W    2
#define BULLET_H    4
#define BULLET_DY   5

#define NBOMBS      3
#define BOMB_W      2
#define BOMB_H      4
#define BOMB_DY     2

#define NBUNKERS    4
#define BUNKER_W    16
#define BUNKER_H    8
#define BUNKER_Y    74
#define BUNKER_HP   6

#define UFO_W       14
#define UFO_H       7
#define UFO_Y       6
#define UFO_DX      1

/* Hardware pens. 0 is transparent/background. 4..8 are the five
** invader-row pens that the colour cycler repaints. */
#define PEN_BG       0
#define PEN_SHIP     1
#define PEN_PBULLET  2
#define PEN_BOMB     3
#define PEN_ROW0     4
#define PEN_BUNKER   9
#define PEN_UFO     10
#define PEN_TEXT    15

/* Game states */
#define ST_PLAY     0
#define ST_DEAD     1          /* brief death pause   */
#define ST_OVER     2
#define ST_WIN      3          /* wave cleared pause   */

/* ------------------------------------------------------------------ */
/* Sprite image data: 4bpp literal. Pixel value 1 = "on"; the SCB      */
/* penpal maps value 1 to the wanted hardware pen and value 0 stays    */
/* pen 0 (transparent). Each line: byte count incl. itself, then the   */
/* pixel-pair bytes, then a trailing 0x00 pad byte; a 0 count ends the */
/* sprite. The pad works around Suzy's last-pixel bug (it drops the    */
/* final pixel of every literal line); value 0 maps to pen 0 here, so  */
/* the 0x00 pad is transparent. See design/LYNX_SPRITE_PADBYTE_DESIGN.md.*/
/* ------------------------------------------------------------------ */

/* Invader, 8x8, marching frame A. */
static unsigned char inv_a[] = {
    0x06, 0x01, 0x00, 0x00, 0x10, 0x00,
    0x06, 0x00, 0x10, 0x01, 0x00, 0x00,
    0x06, 0x01, 0x11, 0x11, 0x10, 0x00,
    0x06, 0x11, 0x01, 0x10, 0x11, 0x00,
    0x06, 0x11, 0x11, 0x11, 0x11, 0x00,
    0x06, 0x10, 0x11, 0x11, 0x01, 0x00,
    0x06, 0x10, 0x10, 0x01, 0x01, 0x00,
    0x06, 0x00, 0x10, 0x01, 0x00, 0x00,
    0x00
};

/* Invader, 8x8, marching frame B (legs/arms swapped). */
static unsigned char inv_b[] = {
    0x06, 0x01, 0x00, 0x00, 0x10, 0x00,
    0x06, 0x00, 0x10, 0x01, 0x00, 0x00,
    0x06, 0x01, 0x11, 0x11, 0x10, 0x00,
    0x06, 0x11, 0x01, 0x10, 0x11, 0x00,
    0x06, 0x11, 0x11, 0x11, 0x11, 0x00,
    0x06, 0x00, 0x10, 0x01, 0x00, 0x00,
    0x06, 0x01, 0x01, 0x10, 0x10, 0x00,
    0x06, 0x10, 0x00, 0x00, 0x01, 0x00,
    0x00
};

/* Explosion burst, 8x8. */
static unsigned char inv_boom[] = {
    0x06, 0x10, 0x00, 0x00, 0x01, 0x00,
    0x06, 0x01, 0x00, 0x00, 0x10, 0x00,
    0x06, 0x00, 0x01, 0x10, 0x00, 0x00,
    0x06, 0x00, 0x11, 0x11, 0x00, 0x00,
    0x06, 0x00, 0x11, 0x11, 0x00, 0x00,
    0x06, 0x00, 0x01, 0x10, 0x00, 0x00,
    0x06, 0x01, 0x00, 0x00, 0x10, 0x00,
    0x06, 0x10, 0x00, 0x00, 0x01, 0x00,
    0x00
};

/* Cannon, 11x8 (padded to 12 across = 6 bytes/line). */
static unsigned char ship_img[] = {
    0x08, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x00, 0x11, 0x10, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x00, 0x11, 0x10, 0x00, 0x00, 0x00,
    0x08, 0x01, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00,
    0x08, 0x11, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00,
    0x08, 0x11, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00,
    0x08, 0x11, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00,
    0x08, 0x11, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00,
    0x00
};

/* Player bullet, 2x4. */
static unsigned char pbullet_img[] = {
    0x03, 0x11, 0x00,
    0x03, 0x11, 0x00,
    0x03, 0x11, 0x00,
    0x03, 0x11, 0x00,
    0x00
};

/* Enemy bomb, 2x4 (a little zig-zag). */
static unsigned char bomb_img[] = {
    0x03, 0x10, 0x00,
    0x03, 0x01, 0x00,
    0x03, 0x10, 0x00,
    0x03, 0x01, 0x00,
    0x00
};

/* Bunker block, 16x8 with a domed top. */
static unsigned char bunker_img[] = {
    0x0A, 0x00, 0x11, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00, 0x00,
    0x0A, 0x01, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00,
    0x0A, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00,
    0x0A, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00,
    0x0A, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00,
    0x0A, 0x11, 0x11, 0x10, 0x00, 0x11, 0x11, 0x11, 0x10, 0x00,
    0x0A, 0x11, 0x11, 0x00, 0x00, 0x01, 0x11, 0x11, 0x10, 0x00,
    0x0A, 0x11, 0x10, 0x00, 0x00, 0x00, 0x11, 0x11, 0x10, 0x00,
    0x00
};

/* UFO, 14x7 (padded to 14 across = 7 bytes/line). */
static unsigned char ufo_img[] = {
    0x09, 0x00, 0x01, 0x11, 0x11, 0x11, 0x10, 0x00, 0x00,
    0x09, 0x00, 0x11, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00,
    0x09, 0x01, 0x11, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00,
    0x09, 0x11, 0x01, 0x10, 0x11, 0x01, 0x10, 0x11, 0x00,
    0x09, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x00,
    0x09, 0x00, 0x11, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00,
    0x09, 0x00, 0x01, 0x10, 0x00, 0x01, 0x10, 0x00, 0x00,
    0x00
};

/* ------------------------------------------------------------------ */
/* Palette. 32 bytes: 16 green nibbles, then 16 blue/red (GCOLMAP).    */
/* Colours are written as 12-bit $GRB and split into the two halves.   */
/* ------------------------------------------------------------------ */

#define GRB_G(c)    ((unsigned char)(((c) >> 8) & 0x0F))
#define GRB_RB(c)   ((unsigned char)((c) & 0xFF))

static unsigned char pal[32];

/* Base colours per pen (index = pen). Invader pens 4..8 are filled by
** the cycler, so their base value here is just a sane default. */
static const unsigned int pen_base[16] = {
    0x000,      /* 0  background          */
    0x4F2,      /* 1  ship  (lime green)  */
    0xFFF,      /* 2  player bullet (white) */
    0x0F4,      /* 3  bomb  (orange-red)  */
    0x0F0,      /* 4  invader row 0       */
    0xFF0,      /* 5  invader row 1       */
    0xF00,      /* 6  invader row 2       */
    0x00F,      /* 7  invader row 3       */
    0x0FF,      /* 8  invader row 4       */
    0x6F4,      /* 9  bunker (green)      */
    0xFF0,      /* 10 UFO (flashes)       */
    0x888,      /* 11                     */
    0x333,      /* 12                     */
    0x0F8,      /* 13                     */
    0x88F,      /* 14                     */
    0xFFF       /* 15 text (white)        */
};

/* 12-entry hue wheel used for the colour-cycle effect. */
static const unsigned int rainbow[12] = {
    0x0F0, 0x6F0, 0xFF0, 0xF80, 0xF00, 0xF08,
    0xF0F, 0x80F, 0x00F, 0x08F, 0x0FF, 0x6F8
};

static void pal_set (unsigned char pen, unsigned int grb)
{
    pal[pen]      = GRB_G (grb);
    pal[16 + pen] = GRB_RB (grb);
}

static void pal_init (void)
{
    unsigned char i;
    for (i = 0; i < 16; ++i)
        pal_set (i, pen_base[i]);
}

/* ------------------------------------------------------------------ */
/* Sound: direct Mikey audio register access.                         */
/* control = 0x18 | clk -> enable reload (0x08) + count (0x10) + clock */
/* select (bits 0-2: 0=1us,1=2us,...). feedback 0x01 = square tone,    */
/* 0x3F = long tap set = noise. volume is 7-bit + sign.                */
/* ------------------------------------------------------------------ */

#define FB_TONE     0x01
#define FB_NOISE    0x3F

static void snd_voice (struct _mikey_audio* c, unsigned char vol,
                       unsigned char fb, unsigned char clk,
                       unsigned char reload)
{
    c->control  = 0;            /* stop while we set the shifter */
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

/* Per-channel envelope bookkeeping (counted down in frames). */
static unsigned char laser_t;           /* channel A */
static unsigned char laser_p;
static unsigned char boom_t;            /* channel B */
static unsigned char boom_v;
static unsigned char die_t;             /* channel A (shared w/ laser) */
static unsigned char die_p;
static unsigned char ufo_snd;           /* channel A shared, UFO hum */

static void sfx_shoot (void)
{
    laser_p = 40;
    laser_t = 7;
    snd_voice (&MIKEY.channel_a, 0x38, FB_TONE, 2, laser_p);
}

static void sfx_explode (void)
{
    boom_v = 0x44;
    boom_t = 12;
    snd_voice (&MIKEY.channel_b, boom_v, FB_NOISE, 3, 0x30);
}

static void sfx_die (void)
{
    die_p = 60;
    die_t = 40;
    snd_voice (&MIKEY.channel_a, 0x48, FB_TONE, 4, die_p);
    ufo_snd = 0;
}

/* Advance all sound envelopes one frame. */
static void sfx_update (void)
{
    if (laser_t) {
        laser_p += 6;                   /* slide pitch down = "pew" */
        MIKEY.channel_a.reload = laser_p;
        if (--laser_t == 0) snd_silence (&MIKEY.channel_a);
    }
    if (boom_t) {
        if (boom_v > 6) boom_v -= 6;    /* decay */
        MIKEY.channel_b.volume = boom_v;
        if (--boom_t == 0) snd_silence (&MIKEY.channel_b);
    }
    if (die_t) {
        die_p += 3;
        MIKEY.channel_a.reload = die_p;
        if (--die_t == 0) snd_silence (&MIKEY.channel_a);
    } else if (ufo_snd) {
        /* warbling UFO hum on channel A */
        die_p = (die_p + 5) & 0x7F;
        MIKEY.channel_a.reload = 40 + (die_p & 0x1F);
    }
}

static void sfx_ufo_on (void)
{
    if (die_t) return;
    ufo_snd = 1;
    die_p = 0;
    snd_voice (&MIKEY.channel_a, 0x2C, FB_TONE, 4, 48);
}

static void sfx_ufo_off (void)
{
    if (ufo_snd) {
        ufo_snd = 0;
        snd_silence (&MIKEY.channel_a);
    }
}

/* ------------------------------------------------------------------ */
/* Sprite control blocks                                               */
/* ------------------------------------------------------------------ */

static SCB_REHV_PAL inv[INV_N];
static SCB_REHV_PAL ship_scb = {
    BPP_4 | TYPE_NORMAL, LITERAL | REHV, NO_COLLIDE,
    0, ship_img, 0, PLAYER_Y, 0x0100, 0x0100,
    { (PEN_BG << 4) | PEN_SHIP, 0, 0, 0, 0, 0, 0, 0 }
};
static SCB_REHV_PAL pbullet_scb = {
    BPP_4 | TYPE_NORMAL, LITERAL | REHV, NO_COLLIDE,
    0, pbullet_img, 0, 0, 0x0100, 0x0100,
    { (PEN_BG << 4) | PEN_PBULLET, 0, 0, 0, 0, 0, 0, 0 }
};
static SCB_REHV_PAL bomb_scb = {
    BPP_4 | TYPE_NORMAL, LITERAL | REHV, NO_COLLIDE,
    0, bomb_img, 0, 0, 0x0100, 0x0100,
    { (PEN_BG << 4) | PEN_BOMB, 0, 0, 0, 0, 0, 0, 0 }
};
static SCB_REHV_PAL bunker_scb = {
    BPP_4 | TYPE_NORMAL, LITERAL | REHV, NO_COLLIDE,
    0, bunker_img, 0, BUNKER_Y, 0x0100, 0x0100,
    { (PEN_BG << 4) | PEN_BUNKER, 0, 0, 0, 0, 0, 0, 0 }
};
static SCB_REHV_PAL ufo_scb = {
    BPP_4 | TYPE_NORMAL, LITERAL | REHV, NO_COLLIDE,
    0, ufo_img, 0, UFO_Y, 0x0100, 0x0100,
    { (PEN_BG << 4) | PEN_UFO, 0, 0, 0, 0, 0, 0, 0 }
};
static SCB_REHV_PAL boom_scb = {
    BPP_4 | TYPE_NORMAL, LITERAL | REHV, NO_COLLIDE,
    0, inv_boom, 0, 0, 0x0100, 0x0100,
    { (PEN_BG << 4) | PEN_PBULLET, 0, 0, 0, 0, 0, 0, 0 }
};

/* ------------------------------------------------------------------ */
/* Game state                                                          */
/* ------------------------------------------------------------------ */

static int  bx, by;                     /* invader block origin, px  */
static signed char dir;                 /* +1 / -1                   */
static unsigned char alive;             /* invaders left             */
static unsigned char frame_b;           /* march animation toggle    */
static unsigned char march_delay, march_cnt;

static int  px;                         /* cannon x, px              */
static unsigned char bullet_on;
static int  blx, bly;                   /* player bullet, px         */

static int  bombx[NBOMBS], bomby[NBOMBS];
static unsigned char bomb_on[NBOMBS];

static unsigned char bunk_hp[NBUNKERS];

static unsigned char ufo_on;
static int  ufox;
static unsigned char ufo_timer;

static unsigned char boom_on;           /* explosion sprite frames   */
static int  boomx, boomy;

static unsigned int score, hiscore;
static unsigned char lives, wave, state;
static unsigned char dead_timer, win_timer;

static unsigned char rng = 0x2B;
static unsigned char joy, prev_joy, pressed;
static unsigned char cyc_phase, cyc_div;

/* HUD is split so each gfx_outtext stays within the 20-char/line cap
** (the 8x8 font makes 20 glyphs exactly one 160px row). */
static char hud_sc[] = "SCORE 00000";       /* score digits at +6  */
static char hud_lv[] = "SHIPS 3";           /* lives digit  at +6  */
static char hud_hi[] = "HI 00000";          /* hi digits    at +3  */

/* Cheap 8-bit pseudo-random using a Suzy multiply. */
static unsigned char nextrand (void)
{
    rng = (unsigned char)((rng !* 5) + 1);
    return rng;
}

/* Five decimal digits via Suzy unsigned divide and modulo. */
static void fmt5 (unsigned int v, char* p)
{
    unsigned char i;
    for (i = 5; i--; ) {
        p[i] = '0' + (char)(v !% 10);
        v = v !/ 10;
    }
}

/* ------------------------------------------------------------------ */

static void place_invaders (void)
{
    unsigned char r, c, i;
    i = 0;
    for (r = 0; r < INV_ROWS; ++r) {
        for (c = 0; c < INV_COLS; ++c) {
            inv[i].sprctl0 = BPP_4 | TYPE_NORMAL;
            inv[i].sprctl1 = LITERAL | REHV;
            inv[i].sprcoll = NO_COLLIDE;
            inv[i].next    = (char*)&inv[i + 1];
            inv[i].data    = frame_b ? inv_b : inv_a;
            inv[i].hpos    = bx + c !* CELL_W;     /* Suzy multiply */
            inv[i].vpos    = by + r !* CELL_H;
            inv[i].hsize   = 0x0100;
            inv[i].vsize   = 0x0100;
            /* pixel value 1 -> this row's pen */
            inv[i].penpal[0] = (PEN_BG << 4) | (PEN_ROW0 + r);
            ++i;
        }
    }
    inv[INV_N - 1].next = 0;            /* end the chain */
}

static void new_wave (void)
{
    bx = INV_START_X;
    by = INV_START_Y + (wave > 4 ? 8 : (int)(wave << 1));
    dir = 1;
    alive = INV_N;
    frame_b = 0;
    place_invaders ();
    bullet_on = 0;
    state = ST_PLAY;
}

static void new_game (void)
{
    unsigned char i;
    score = 0;
    lives = 3;
    wave  = 0;
    px    = (SCREEN_W - PLAYER_W) / 2;
    for (i = 0; i < NBOMBS; ++i)   bomb_on[i] = 0;
    for (i = 0; i < NBUNKERS; ++i) bunk_hp[i] = BUNKER_HP;
    ufo_on = boom_on = 0;
    ufo_timer = 180;
    sfx_ufo_off ();
    new_wave ();
}

/* Current marching speed: faster as fewer invaders remain (Suzy /). */
static void retune_march (void)
{
    march_delay = 2 + (alive !/ 8);
    if (march_delay > 9) march_delay = 9;
}

/* Live column/row extent of the formation, for edge + reach tests. */
static unsigned char minc, maxc, maxr;
static void formation_bounds (void)
{
    unsigned char r, c, i;
    minc = INV_COLS - 1; maxc = 0; maxr = 0;
    i = 0;
    for (r = 0; r < INV_ROWS; ++r) {
        for (c = 0; c < INV_COLS; ++c, ++i) {
            if (inv[i].sprctl1 & SKIP) continue;
            if (c < minc) minc = c;
            if (c > maxc) maxc = c;
            if (r > maxr) maxr = r;
        }
    }
}

static void march (void)
{
    unsigned char r, c, i;
    int left_edge, right_edge;

    retune_march ();
    if (++march_cnt < march_delay) return;
    march_cnt = 0;

    formation_bounds ();
    left_edge  = bx + minc !* CELL_W;
    right_edge = bx + maxc !* CELL_W + INV_W;

    if ((dir > 0 && right_edge >= SCREEN_W - 3) ||
        (dir < 0 && left_edge <= 3)) {
        dir = -dir;
        by += INV_DROP;
        /* reached the cannon line? game over */
        if (by + maxr !* CELL_H + INV_H >= PLAYER_Y) {
            lives = 0;
            sfx_die ();
            state = ST_OVER;
            return;
        }
    } else {
        bx += dir * INV_STEP;
    }

    frame_b ^= 1;
    i = 0;
    for (r = 0; r < INV_ROWS; ++r)
        for (c = 0; c < INV_COLS; ++c, ++i) {
            inv[i].hpos = bx + c !* CELL_W;
            inv[i].vpos = by + r !* CELL_H;
            inv[i].data = frame_b ? inv_b : inv_a;
        }
}

/* Drop a bomb from a random live column's lowest invader. */
static void maybe_bomb (void)
{
    unsigned char slot, c, r, i, found;

    for (slot = 0; slot < NBOMBS; ++slot)
        if (!bomb_on[slot]) break;
    if (slot == NBOMBS) return;
    if ((nextrand () & 0x1F) != 0) return;     /* ~1/32 per frame */

    c = nextrand () !% INV_COLS;
    found = 0xFF;
    for (r = 0; r < INV_ROWS; ++r) {
        i = r * INV_COLS + c;
        if (!(inv[i].sprctl1 & SKIP)) found = i;
    }
    if (found == 0xFF) return;

    bombx[slot] = inv[found].hpos + INV_W / 2;
    bomby[slot] = inv[found].vpos + INV_H;
    bomb_on[slot] = 1;
}

static void kill_invader (unsigned char i)
{
    inv[i].sprctl1 |= SKIP;
    score += (INV_ROWS - (i / INV_COLS)) !* 10;   /* top rows worth more */
    boomx = inv[i].hpos;
    boomy = inv[i].vpos;
    boom_on = 6;
    sfx_explode ();
    if (--alive == 0) {
        ++wave;
        state = ST_WIN;
        win_timer = 70;
        sfx_ufo_off ();
    }
}

/* Player bullet vs the invader grid: cell lookup with Suzy divide,
** in-sprite test with Suzy modulo (the design doc's n-(n/d)*d path). */
static void bullet_vs_invaders (void)
{
    int rel;
    unsigned char r, c, i;
    int cx = blx + BULLET_W / 2;
    int cy = bly;

    if (cy < by || cy >= by + INV_ROWS * CELL_H) return;
    rel = cx - bx;
    if (rel < 0) return;
    if ((rel !% CELL_W) >= INV_W) return;          /* in the gap */
    if (((cy - by) !% CELL_H) >= INV_H) return;
    c = (unsigned char)(rel !/ CELL_W);
    r = (unsigned char)((cy - by) !/ CELL_H);
    if (c >= INV_COLS || r >= INV_ROWS) return;
    i = r * INV_COLS + c;
    if (inv[i].sprctl1 & SKIP) return;
    kill_invader (i);
    bullet_on = 0;
}

static unsigned char hit_rect (int ax, int ay, int aw, int ah,
                               int rx, int ry, int rw, int rh)
{
    return (ax < rx + rw && ax + aw > rx &&
            ay < ry + rh && ay + ah > ry);
}

static int bunker_x (unsigned char b)
{
    return 16 + b !* 36;                            /* Suzy multiply */
}

static void bullet_vs_bunkers (void)
{
    unsigned char b;
    for (b = 0; b < NBUNKERS; ++b) {
        if (!bunk_hp[b]) continue;
        if (hit_rect (blx, bly, BULLET_W, BULLET_H,
                      bunker_x (b), BUNKER_Y, BUNKER_W, BUNKER_H)) {
            --bunk_hp[b];
            bullet_on = 0;
            return;
        }
    }
}

static void bomb_vs_bunkers (unsigned char s)
{
    unsigned char b;
    for (b = 0; b < NBUNKERS; ++b) {
        if (!bunk_hp[b]) continue;
        if (hit_rect (bombx[s], bomby[s], BOMB_W, BOMB_H,
                      bunker_x (b), BUNKER_Y, BUNKER_W, BUNKER_H)) {
            --bunk_hp[b];
            bomb_on[s] = 0;
            return;
        }
    }
}

static void player_hit (void)
{
    sfx_die ();
    if (--lives == 0) {
        state = ST_OVER;
    } else {
        state = ST_DEAD;
        dead_timer = 40;
    }
}

static void update_play (void)
{
    unsigned char s;

    /* Cannon */
    if (joy & JOY_LEFT_MASK)  { px -= PLAYER_DX; if (px < 2) px = 2; }
    if (joy & JOY_RIGHT_MASK) { px += PLAYER_DX;
        if (px > SCREEN_W - PLAYER_W - 2) px = SCREEN_W - PLAYER_W - 2; }

    if ((pressed & JOY_BTN_1_MASK) && !bullet_on) {
        bullet_on = 1;
        blx = px + (PLAYER_W - BULLET_W) / 2;
        bly = PLAYER_Y - BULLET_H;
        sfx_shoot ();
    }

    /* Player bullet */
    if (bullet_on) {
        bly -= BULLET_DY;
        if (bly <= UFO_Y + UFO_H && ufo_on &&
            hit_rect (blx, bly, BULLET_W, BULLET_H,
                      ufox, UFO_Y, UFO_W, UFO_H)) {
            score += 100;
            ufo_on = 0;
            sfx_ufo_off ();
            sfx_explode ();
            bullet_on = 0;
        } else if (bly < 0) {
            bullet_on = 0;
        } else {
            bullet_vs_invaders ();
            if (bullet_on) bullet_vs_bunkers ();
        }
    }

    march ();
    maybe_bomb ();

    /* Bombs */
    for (s = 0; s < NBOMBS; ++s) {
        if (!bomb_on[s]) continue;
        bomby[s] += BOMB_DY;
        if (bomby[s] > SCREEN_H) { bomb_on[s] = 0; continue; }
        if (hit_rect (bombx[s], bomby[s], BOMB_W, BOMB_H,
                      px, PLAYER_Y, PLAYER_W, PLAYER_H)) {
            bomb_on[s] = 0;
            player_hit ();
            return;
        }
        bomb_vs_bunkers (s);
    }

    /* UFO */
    if (ufo_on) {
        ufox += UFO_DX;
        if (ufox > SCREEN_W) { ufo_on = 0; sfx_ufo_off (); }
    } else if (--ufo_timer == 0) {
        ufo_on = 1;
        ufox = -UFO_W;
        ufo_timer = 150 + (nextrand () & 0x3F);
        sfx_ufo_on ();
    }
}

/* ------------------------------------------------------------------ */
/* Colour cycling: repaint the invader-row pens (and the UFO pen) from */
/* the rainbow LUT, offset per row so a wave of colour flows downward. */
/* ------------------------------------------------------------------ */

static void cycle_colours (void)
{
    unsigned char r, idx;

    if (++cyc_div < 3) return;
    cyc_div = 0;
    ++cyc_phase;

    for (r = 0; r < INV_ROWS; ++r) {
        idx = (cyc_phase + (r << 1)) !% 12;
        pal_set (PEN_ROW0 + r, rainbow[idx]);
    }
    /* UFO flashes through the wheel quickly */
    pal_set (PEN_UFO, rainbow[(cyc_phase << 1) !% 12]);
}

/* ------------------------------------------------------------------ */

static void draw (void)
{
    unsigned char b, s;

    while (gfx_busy ()) {}
    gfx_setcolor (COLOR_BLACK);
    gfx_clear ();

    /* invaders: one chained engine run */
    if (alive) gfx_sprite (&inv[0]);

    /* bunkers (recolour as they take damage: green -> brown -> dark) */
    for (b = 0; b < NBUNKERS; ++b) {
        if (!bunk_hp[b]) continue;
        bunker_scb.hpos = bunker_x (b);
        if      (bunk_hp[b] >= 5) bunker_scb.penpal[0] = (PEN_BG << 4) | PEN_BUNKER;
        else if (bunk_hp[b] >= 3) bunker_scb.penpal[0] = (PEN_BG << 4) | 5; /* yellow */
        else                      bunker_scb.penpal[0] = (PEN_BG << 4) | 6; /* red   */
        gfx_sprite (&bunker_scb);
    }

    /* UFO */
    if (ufo_on) {
        ufo_scb.hpos = ufox;
        gfx_sprite (&ufo_scb);
    }

    /* explosion */
    if (boom_on) {
        boom_scb.hpos = boomx;
        boom_scb.vpos = boomy;
        gfx_sprite (&boom_scb);
        --boom_on;
    }

    /* bombs */
    for (s = 0; s < NBOMBS; ++s) {
        if (!bomb_on[s]) continue;
        bomb_scb.hpos = bombx[s];
        bomb_scb.vpos = bomby[s];
        gfx_sprite (&bomb_scb);
    }

    /* cannon (hidden during the death pause flash) */
    if (state != ST_DEAD || (dead_timer & 4)) {
        ship_scb.hpos = px;
        gfx_sprite (&ship_scb);
    }

    /* player bullet */
    if (bullet_on) {
        pbullet_scb.hpos = blx;
        pbullet_scb.vpos = bly;
        gfx_sprite (&pbullet_scb);
    }

    /* HUD (top row) */
    if (score > hiscore) hiscore = score;
    fmt5 (score, hud_sc + 6);
    hud_lv[6] = '0' + lives;
    gfx_setcolor (COLOR_WHITE);
    gfx_outtextxy (0, 0, hud_sc);
    gfx_outtextxy (104, 0, hud_lv);

    if (state == ST_OVER) {
        fmt5 (hiscore, hud_hi + 3);
        gfx_setcolor (COLOR_RED);
        gfx_outtextxy (44, 40, "GAME OVER");
        gfx_setcolor (COLOR_YELLOW);
        gfx_outtextxy (48, 54, hud_hi);
        gfx_setcolor (COLOR_WHITE);
        gfx_outtextxy (28, 66, "A = NEW GAME");
    } else if (state == ST_WIN) {
        gfx_setcolor (COLOR_GREEN);
        gfx_outtextxy (36, 44, "WAVE CLEAR");
    }

    gfx_updatedisplay ();
}

void main (void)
{
    gfx_init ();
    CLI ();
    while (gfx_busy ()) {}

    /* sound: enable all channels to both ears */
    MIKEY.mstereo = 0x00;
    snd_silence (&MIKEY.channel_a);
    snd_silence (&MIKEY.channel_b);
    snd_silence (&MIKEY.channel_c);
    snd_silence (&MIKEY.channel_d);

    /* Start the two theme voices on channels C and D. snd_init() installs the
    ** sound-timer IRQ that walks the streams; each stream self-loops, so no
    ** retrigger is needed. Channels A and B are left for the hand-driven SFX. */
    snd_init ();
    snd_play (THEME_CH_A, invaders_theme_a);
    snd_play (THEME_CH_B, invaders_theme_b);

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

        switch (state) {
            case ST_PLAY:
                update_play ();
                break;
            case ST_DEAD:
                if (--dead_timer == 0) {
                    unsigned char k;
                    bullet_on = 0;
                    for (k = 0; k < NBOMBS; ++k) bomb_on[k] = 0;
                    px = (SCREEN_W - PLAYER_W) / 2;
                    state = ST_PLAY;
                }
                break;
            case ST_WIN:
                if (--win_timer == 0) new_wave ();
                break;
            case ST_OVER:
                sfx_ufo_off ();
                if (pressed & JOY_BTN_1_MASK) new_game ();
                break;
        }

        sfx_update ();
        cycle_colours ();
        gfx_setpalette (pal);
        draw ();
    }
}
