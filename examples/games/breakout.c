/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in this directory.
*/

/*
** breakout.c - Single-file Breakout for the Atari Lynx.
**
** Demonstrates this fork's Suzy hardware math operators !* !/ !%
** (design/LYNX_CODEGEN_DESIGN.md section 2.6) in real game code: paddle
** deflection, brick grid hit testing and score formatting all run
** on Suzy's 16x16 multiply and 32/16 divide instead of the software
** runtime loops.
**
** All sprite image data, pen palettes and sprite control blocks
** (SCBs) are defined below in this file. The whole scene - 32
** bricks, paddle and ball - is one SCB chain drawn with a single
** tgi_sprite() call; dead bricks are skipped with the SKIP bit.
**
** Suzy math contract (section 2.6): all !* !/ !% sites are in the
** main loop only - never in IRQ context - and the TGI driver draws
** sprites synchronously, so the math unit is never contended.
**
** Controls: pad left/right moves, A serves/restarts.
**
** Build:  cl65 -Ors -o breakout.lnx breakout.c
*/

#include <lynx/lynx.h>
#include <lynx/tgi.h>
#include <lynx/joystick.h>
#include <6502.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Geometry and tuning. Positions use 12.4 fixed point.                */
/* ------------------------------------------------------------------ */

#define FP(n)       ((n) << 4)
#define PIX(n)      ((n) >> 4)

#define SCREEN_W    160
#define SCREEN_H    102

#define BRICK_COLS  8
#define BRICK_ROWS  4
#define BRICK_W     20          /* grid cell; sprite is 19 wide */
#define BRICK_H     6           /* grid cell; sprite is 5 tall  */
#define BRICK_TOP   14

#define PADDLE_W    24
#define PADDLE_H    4
#define PADDLE_Y    96
#define PADDLE_DX   3

#define BALL_W      4

#define SPD_BASE    18          /* serve speed, 12.4 fixed      */
#define SPD_MAX     40
#define VX_MAX      28

#define ST_SERVE    0
#define ST_PLAY     1
#define ST_OVER     2

/* ------------------------------------------------------------------ */
/* Sprite image data: 4bpp totally literal. Each line starts with its  */
/* byte count (including itself); a count of 0 ends the sprite.        */
/* ------------------------------------------------------------------ */

/* Sprite image data: 4bpp literal. Each line is a byte count (incl.
** itself), the pixel-pair bytes, then a trailing 0x00 pad byte; a 0
** count ends the sprite. The pad works around Suzy's last-pixel bug
** (it drops the final pixel of every literal line); value 0 maps to
** pen 0 (transparent) in every penpal below, so the 0x00 pad is
** invisible. See design/LYNX_SPRITE_PADBYTE_DESIGN.md.
*/

/* Brick, 19x5: pen 2 top highlight, pen 1 face, pen 3 shadow. */
static unsigned char brick_img[] = {
    0x0C, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x20, 0x00,
    0x0C, 0x21, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x30, 0x00,
    0x0C, 0x21, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x30, 0x00,
    0x0C, 0x21, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x30, 0x00,
    0x0C, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x30, 0x00,
    0x00
};

/* Paddle, 8x2, hardware-scaled to 24x4: pen 2 top, pen 1 body. */
static unsigned char paddle_img[] = {
    0x06, 0x22, 0x22, 0x22, 0x22, 0x00,
    0x06, 0x11, 0x11, 0x11, 0x11, 0x00,
    0x00
};

/* Ball, 4x4: pen 0 corners transparent, pen 2 highlight. */
static unsigned char ball_img[] = {
    0x04, 0x01, 0x10, 0x00,
    0x04, 0x21, 0x11, 0x00,
    0x04, 0x11, 0x11, 0x00,
    0x04, 0x01, 0x10, 0x00,
    0x00
};

/* Pen palettes: two pens per byte, even pen in the high nibble.       */
/* Bricks are recolored per row through the SCB penpal alone.          */
static unsigned char row_pens[BRICK_ROWS][8] = {
    { 0x01, 0x2B, 0, 0, 0, 0, 0, 0 },   /* red    / pink   / d.brown */
    { 0x08, 0x76, 0, 0, 0, 0, 0, 0 },   /* yellow / peach  / brown   */
    { 0x0A, 0x95, 0, 0, 0, 0, 0, 0 },   /* green  / l.green/ d.grey  */
    { 0x0D, 0xE4, 0, 0, 0, 0, 0, 0 }    /* blue   / l.blue / grey    */
};

/* ------------------------------------------------------------------ */
/* Sprite control blocks. One chain: bricks -> paddle -> ball.         */
/* ------------------------------------------------------------------ */

static SCB_REHV_PAL ball_scb = {
    BPP_4 | TYPE_NORMAL, LITERAL | REHV, NO_COLLIDE,
    0, ball_img, 0, 0, 0x0100, 0x0100,
    { 0x03, 0xF0, 0, 0, 0, 0, 0, 0 }    /* l.grey body, white spot */
};

static SCB_REHV_PAL paddle_scb = {
    BPP_4 | TYPE_NORMAL, LITERAL | REHV, NO_COLLIDE,
    (char*)&ball_scb, paddle_img, 0, PADDLE_Y, 0x0300, 0x0200,
    { 0x03, 0xF0, 0, 0, 0, 0, 0, 0 }
};

static SCB_REHV_PAL bricks[BRICK_ROWS * BRICK_COLS];

/* ------------------------------------------------------------------ */
/* Game state                                                          */
/* ------------------------------------------------------------------ */

static int x, y, vx, vy;                /* ball, 12.4 fixed     */
static int px;                          /* paddle, pixels       */
static unsigned spd;                    /* serve speed magnitude */
static unsigned score;
static unsigned char lives, level, hits, left, state;
static unsigned char joy, prev_joy, pressed;

static char hud[] = "SCORE 00000  *3 L1";

/* ------------------------------------------------------------------ */

/* Five decimal digits via Suzy unsigned divide and modulo. */
static void fmt5 (unsigned v, char* p)
{
    unsigned char i;
    for (i = 5; i--; ) {
        p[i] = '0' + (char)(v !% 10);
        v = v !/ 10;
    }
}

static void init_bricks (void)
{
    unsigned char r, c, i;

    i = 0;
    for (r = 0; r < BRICK_ROWS; ++r) {
        for (c = 0; c < BRICK_COLS; ++c) {
            bricks[i].sprctl0 = BPP_4 | TYPE_NORMAL;
            bricks[i].sprctl1 = LITERAL | REHV;
            bricks[i].sprcoll = NO_COLLIDE;
            bricks[i].next    = (char*)&bricks[i + 1];
            bricks[i].data    = brick_img;
            /* Suzy multiply for the grid layout */
            bricks[i].hpos    = c !* BRICK_W;
            bricks[i].vpos    = BRICK_TOP + r !* BRICK_H;
            bricks[i].hsize   = 0x0100;
            bricks[i].vsize   = 0x0100;
            memcpy (bricks[i].penpal, row_pens[r], 8);
            ++i;
        }
    }
    bricks[i - 1].next = (char*)&paddle_scb;
    left = BRICK_ROWS * BRICK_COLS;
}

static void serve (void)
{
    x = FP (px + (PADDLE_W - BALL_W) / 2);
    y = FP (PADDLE_Y - BALL_W);
    state = ST_SERVE;
}

static void new_game (void)
{
    score = 0;
    lives = 3;
    level = 1;
    hits  = 0;
    spd   = SPD_BASE;
    px    = (SCREEN_W - PADDLE_W) / 2;
    init_bricks ();
    serve ();
}

static void launch (void)
{
    /* Deflection from the paddle's distance to screen center:
    ** signed Suzy multiply and divide. */
    vx = ((px - (SCREEN_W - PADDLE_W) / 2) !* 5) !/ 16;
    if (vx == 0) vx = 5;
    vy = -(int)spd;
    state = ST_PLAY;
}

static void move_ball (void)
{
    int bcx, bcy, off;
    unsigned char r, c, i;

    x += vx;
    y += vy;

    /* Walls */
    if (x <= 0)                     { x = 0;                   vx = -vx; }
    if (x >= FP (SCREEN_W - BALL_W)) { x = FP (SCREEN_W - BALL_W); vx = -vx; }
    if (y <= 0)                     { y = 0;                   vy = -vy; }

    bcx = PIX (x) + BALL_W / 2;
    bcy = PIX (y) + BALL_W / 2;

    /* Brick field: cell lookup with Suzy divide, gap test with Suzy
    ** modulo (the hardware remainder register is buggy, so the
    ** runtime computes n - (n/d)*d as per the design doc). */
    if (bcy >= BRICK_TOP && bcy < BRICK_TOP + BRICK_ROWS * BRICK_H) {
        off = bcy - BRICK_TOP;
        if (off !% BRICK_H < 5 && bcx !% BRICK_W < 19) {
            r = (unsigned char)(off !/ BRICK_H);
            c = (unsigned char)(bcx !/ BRICK_W);
            i = r * BRICK_COLS + c;
            if (!(bricks[i].sprctl1 & SKIP)) {
                bricks[i].sprctl1 |= SKIP;
                score += (BRICK_ROWS - r) !* 10;
                vy = -vy;
                if (--left == 0) {
                    ++level;
                    if (spd < SPD_MAX) spd += 3;
                    init_bricks ();
                    serve ();
                    return;
                }
            }
        }
    }

    /* Paddle */
    if (vy > 0 && PIX (y) + BALL_W >= PADDLE_Y
                && PIX (y) + BALL_W <= PADDLE_Y + PADDLE_H
                && bcx >= px && bcx < px + PADDLE_W) {
        /* Bounce angle from impact offset: Suzy signed mul/div */
        off = bcx - (px + PADDLE_W / 2);
        vx = (off !* 7) !/ 4;
        if (vx >  VX_MAX) vx =  VX_MAX;
        if (vx < -VX_MAX) vx = -VX_MAX;
        /* Every third paddle hit speeds the ball up */
        ++hits;
        if (hits !% 3 == 0 && spd < SPD_MAX) ++spd;
        vy = -(int)spd;
        y  = FP (PADDLE_Y - BALL_W);
    }

    /* Lost */
    if (PIX (y) > SCREEN_H) {
        if (--lives == 0) {
            state = ST_OVER;
        } else {
            serve ();
        }
    }
}

static void draw (void)
{
    while (tgi_busy ()) {}
    tgi_setcolor (COLOR_BLACK);         /* tgi_clear fills in draw color */
    tgi_clear ();

    paddle_scb.hpos = px;
    ball_scb.hpos   = PIX (x);
    ball_scb.vpos   = PIX (y);

    /* Whole scene: one chained sprite engine run */
    tgi_sprite (&bricks[0]);

    fmt5 (score, hud + 6);
    hud[14] = '0' + lives;
    hud[17] = '0' + (char)(level !% 10);
    tgi_setcolor (COLOR_WHITE);
    tgi_outtextxy (0, 0, hud);

    if (state == ST_SERVE) {
        tgi_outtextxy (52, 60, "PRESS A");
    } else if (state == ST_OVER) {
        tgi_setcolor (COLOR_RED);
        tgi_outtextxy (44, 50, "GAME OVER");
        tgi_setcolor (COLOR_WHITE);
        tgi_outtextxy (28, 64, "A = NEW GAME");
    }

    tgi_updatedisplay ();
}

void main (void)
{
    tgi_init ();
    CLI ();
    while (tgi_busy ()) {}
    tgi_setpalette (tgi_getdefpalette ());
    tgi_setframerate (60);
    tgi_setcollisiondetection (0);

    new_game ();
    prev_joy = 0;

    for (;;) {
        joy = joy_read ();
        pressed = joy & (unsigned char)~prev_joy;
        prev_joy = joy;

        if (state != ST_OVER) {
            if (joy & JOY_LEFT_MASK) {
                px -= PADDLE_DX;
                if (px < 0) px = 0;
            }
            if (joy & JOY_RIGHT_MASK) {
                px += PADDLE_DX;
                if (px > SCREEN_W - PADDLE_W) px = SCREEN_W - PADDLE_W;
            }
        }

        switch (state) {
            case ST_SERVE:
                x = FP (px + (PADDLE_W - BALL_W) / 2);
                if (pressed & JOY_BTN_1_MASK) launch ();
                break;
            case ST_PLAY:
                move_ball ();
                break;
            case ST_OVER:
                if (pressed & JOY_BTN_1_MASK) new_game ();
                break;
        }

        draw ();
    }
}
