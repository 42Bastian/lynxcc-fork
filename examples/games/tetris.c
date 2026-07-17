/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in this directory.
*/

/*
** tetris.c - Single-file falling-block puzzle ("TETROIDS") for the Atari Lynx.
**
** A complete little game built on the fork's static Lynx graphics library:
** a title menu, an instructions screen and a playfield with the seven
** tetromino shapes, hard-drop, wall-kick rotation, line clearing, scoring
** and a level curve that speeds the drop up as rows are cleared.
**
** Rendering: every filled well cell, the falling piece, the well frame and
** the next-piece preview are emitted as one SCB chain drawn with a single
** gfx_sprite() call per frame. One tiny 4x4 block image is reused for every
** cell; its colour comes from the per-SCB pen palette, so the same image
** paints all seven piece colours plus the grey frame (compare breakout.c).
**
** Suzy math: score formatting (five decimal digits) and the per-line score
** award use this fork's Suzy hardware operators !* !/ !% (see
** design/LYNX_CODEGEN_DESIGN.md section 2.6). As required by the Suzy math
** contract, all those sites live in the main loop, never in IRQ context.
**
** Controls:
**   Pad Left/Right  move the piece
**   Pad Down        soft drop (faster fall, +1 point per row)
**   Pad Up          hard drop (slam to the bottom, +2 points per row)
**   Button A        rotate clockwise
**   Button B        rotate counter-clockwise
**   Pause           pause / resume
**   On the menu: Up/Down choose, A selects; on instructions: B returns.
**
** Build:  cl65 -Ors -o tetris.lnx tetris.c
*/

#include <lynx/lynx.h>
#include <lynx/gfx.h>
#include <lynx/joystick.h>
#include <6502.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Well geometry. The well is COLS x ROWS cells at a CELL-pixel pitch;  */
/* each block sprite is 4 px, leaving a 1 px grid gap between cells.     */
/* ------------------------------------------------------------------ */

#define COLS        10
#define ROWS        20
#define CELL        5           /* cell pitch in pixels               */
#define FIELD_X     5           /* well top-left, pixels              */
#define FIELD_Y     1

#define PANEL_X     64          /* HUD column, pixels                 */

/* Game states */
#define ST_MENU     0
#define ST_INSTR    1
#define ST_PLAY     2
#define ST_PAUSE    3
#define ST_OVER     4

/* Input tuning (frames at 60 Hz) */
#define DAS_DELAY   10          /* hold before auto-repeat            */
#define DAS_RATE    3           /* auto-repeat period                 */
#define SOFT_FRAMES 2           /* soft-drop step period              */

/* ------------------------------------------------------------------ */
/* Block image: 4x4, 4bpp literal, with the trailing pad byte that     */
/* works around Suzy's last-pixel bug (see                             */
/* design/LYNX_SPRITE_PADBYTE_DESIGN.md). Pen 1 is the block face,      */
/* pen 2 the top/left highlight, pen 3 the bottom/right shadow; pen 0   */
/* is transparent. Each SCB recolours these through its pen palette.    */
/* ------------------------------------------------------------------ */

static unsigned char block_img[] = {
    0x04, 0x22, 0x23, 0x00,     /* 2 2 2 3  (highlight row) */
    0x04, 0x21, 0x13, 0x00,     /* 2 1 1 3                  */
    0x04, 0x21, 0x13, 0x00,     /* 2 1 1 3                  */
    0x04, 0x33, 0x33, 0x00,     /* 3 3 3 3  (shadow row)    */
    0x00
};

/* Per-piece body colours (default-palette indices): I, O, T, S, Z, J, L. */
static const unsigned char piece_color[7] = {
    COLOR_LIGHTBLUE,    /* I */
    COLOR_YELLOW,       /* O */
    COLOR_VIOLET,       /* T */
    COLOR_GREEN,        /* S */
    COLOR_RED,          /* Z */
    COLOR_BLUE,         /* J */
    COLOR_PEACH         /* L */
};

/* Tetromino cells: [piece][rotation][4 cells], each cell (col<<4)|row
** inside a 4x4 box. Rotations are pre-baked so no rotation math runs. */
static const unsigned char shapes[7][4][4] = {
    /* I */
    { {0x01,0x11,0x21,0x31}, {0x20,0x21,0x22,0x23},
      {0x02,0x12,0x22,0x32}, {0x10,0x11,0x12,0x13} },
    /* O */
    { {0x10,0x20,0x11,0x21}, {0x10,0x20,0x11,0x21},
      {0x10,0x20,0x11,0x21}, {0x10,0x20,0x11,0x21} },
    /* T */
    { {0x10,0x01,0x11,0x21}, {0x10,0x11,0x21,0x12},
      {0x01,0x11,0x21,0x12}, {0x10,0x01,0x11,0x12} },
    /* S */
    { {0x10,0x20,0x01,0x11}, {0x10,0x11,0x21,0x22},
      {0x11,0x21,0x02,0x12}, {0x00,0x01,0x11,0x12} },
    /* Z */
    { {0x00,0x10,0x11,0x21}, {0x20,0x11,0x21,0x12},
      {0x01,0x11,0x12,0x22}, {0x10,0x01,0x11,0x02} },
    /* J */
    { {0x00,0x01,0x11,0x21}, {0x10,0x20,0x11,0x12},
      {0x01,0x11,0x21,0x22}, {0x10,0x11,0x02,0x12} },
    /* L */
    { {0x20,0x01,0x11,0x21}, {0x10,0x11,0x12,0x22},
      {0x01,0x11,0x21,0x02}, {0x00,0x10,0x11,0x12} }
};

#define CELLCOL(b)  ((signed char)((b) >> 4))
#define CELLROW(b)  ((signed char)((b) & 0x0F))

/* Wall-kick column offsets tried when a rotation is blocked. */
static const signed char kicks[5] = { 0, -1, 1, -2, 2 };

/* Line-clear award (before the level multiplier). */
static const unsigned line_pts[4] = { 100, 300, 500, 800 };

/* ------------------------------------------------------------------ */
/* SCB pool. One chain per frame: frame blocks -> well cells -> the      */
/* falling piece -> next-piece preview. Sized for the worst case.        */
/* ------------------------------------------------------------------ */

#define POOL (COLS * ROWS + 2 * ROWS + COLS + 2 + 4 + 4)
static SCB_REHV_PAL pool[POOL];

/* ------------------------------------------------------------------ */
/* Game state                                                          */
/* ------------------------------------------------------------------ */

static unsigned char board[ROWS][COLS];     /* 0 empty, else colour+1 */
static int      cur_x, cur_y;               /* piece origin, cells    */
static unsigned char cur_type, cur_rot, next_type;
static unsigned score, lines_total;
static unsigned char level, fall_frames, grav, das, state, menu_sel;
static signed char last_dir;
static unsigned rng, tick;
static unsigned joy, prev_joy, pressed;

static char hud[6];                         /* five digits + NUL      */

/* ------------------------------------------------------------------ */

/* Five decimal digits via Suzy unsigned divide and modulo. */
static void fmt5 (unsigned v, char* p)
{
    unsigned char i;
    for (i = 5; i--; ) {
        p[i] = '0' + (char)(v !% 10);
        v = v !/ 10;
    }
    p[5] = '\0';
}

static unsigned char rand7 (void)
{
    rng = rng * 33797u + 1u;
    return (unsigned char)((rng >> 9) % 7);
}

static void set_speed (void)
{
    int f = 10 - (int)(level - 1);
    if (f < 3) f = 3;
    fall_frames = (unsigned char)f;
}

/* True if piece cur_type at rotation rot, origin (ox,oy), overlaps a
** wall, the floor or a settled block. */
static unsigned char collides (int ox, int oy, unsigned char rot)
{
    const unsigned char* s = shapes[cur_type][rot];
    unsigned char k;
    int c, r;

    for (k = 0; k < 4; ++k) {
        c = ox + CELLCOL (s[k]);
        r = oy + CELLROW (s[k]);
        if (c < 0 || c >= COLS || r >= ROWS)  return 1;
        if (r >= 0 && board[r][c])            return 1;
    }
    return 0;
}

static void spawn (void)
{
    cur_type  = next_type;
    next_type = rand7 ();
    cur_rot   = 0;
    cur_x     = 3;
    cur_y     = 0;
    grav      = 0;
    if (collides (cur_x, cur_y, cur_rot))
        state = ST_OVER;
}

static void new_game (void)
{
    memset (board, 0, sizeof board);
    score       = 0;
    lines_total = 0;
    level       = 1;
    das         = 0;
    last_dir    = 0;
    set_speed ();
    rng += tick + 1;            /* seed from when the player started */
    next_type = rand7 ();
    spawn ();
    state = ST_PLAY;
}

/* Settle the piece, clear full rows, award score and level up. */
static void lock_piece (void)
{
    const unsigned char* s = shapes[cur_type][cur_rot];
    unsigned char k, c, cleared;
    int r, rr;

    for (k = 0; k < 4; ++k) {
        c = cur_x + CELLCOL (s[k]);
        r = cur_y + CELLROW (s[k]);
        if (r >= 0)
            board[r][c] = cur_type + 1;
    }

    cleared = 0;
    r = ROWS - 1;
    while (r >= 0) {
        for (c = 0; c < COLS && board[r][c]; ++c)
            ;
        if (c == COLS) {                    /* full row: drop the stack */
            for (rr = r; rr > 0; --rr)
                memcpy (board[rr], board[rr - 1], COLS);
            memset (board[0], 0, COLS);
            ++cleared;                      /* recheck this row index   */
        } else {
            --r;
        }
    }

    if (cleared) {
        /* Suzy multiply for the level-scaled line award. */
        score       += line_pts[cleared - 1] !* level;
        lines_total += cleared;
        if (lines_total / 10 + 1 > level) {
            level = (unsigned char)(lines_total / 10 + 1);
            set_speed ();
        }
    }

    spawn ();
}

static void try_move (signed char dir)
{
    if (!collides (cur_x + dir, cur_y, cur_rot))
        cur_x += dir;
}

static void try_rotate (signed char d)
{
    unsigned char nr = (unsigned char)((cur_rot + (d > 0 ? 1 : 3)) & 3);
    unsigned char i;
    for (i = 0; i < 5; ++i) {
        if (!collides (cur_x + kicks[i], cur_y, nr)) {
            cur_x  += kicks[i];
            cur_rot = nr;
            return;
        }
    }
}

static void hard_drop (void)
{
    while (!collides (cur_x, cur_y + 1, cur_rot)) {
        ++cur_y;
        score += 2;
    }
    lock_piece ();
}

/* ------------------------------------------------------------------ */
/* Rendering                                                           */
/* ------------------------------------------------------------------ */

/* Fill one pooled SCB as a block at pixel (hp,vp) tinted to a body pen. */
static SCB_REHV_PAL* block (unsigned n, int hp, int vp, unsigned char body)
{
    SCB_REHV_PAL* s = &pool[n];
    s->sprctl0 = BPP_4 | TYPE_NORMAL;
    s->sprctl1 = LITERAL | REHV;
    s->sprcoll = NO_COLLIDE;
    s->data    = block_img;
    s->hpos    = hp;
    s->vpos    = vp;
    s->hsize   = 0x0100;
    s->vsize   = 0x0100;
    s->penpal[0] = body;                    /* pen0 clear, pen1 body   */
    s->penpal[1] = (COLOR_WHITE << 4) | COLOR_DARKGREY;   /* pen2,pen3 */
    s->penpal[2] = 0; s->penpal[3] = 0;
    s->penpal[4] = 0; s->penpal[5] = 0;
    s->penpal[6] = 0; s->penpal[7] = 0;
    return s;
}

static void draw_playfield (void)
{
    unsigned n = 0;
    unsigned char r, c;
    SCB_REHV_PAL* prev = (SCB_REHV_PAL*)0;
    const unsigned char* s;
    unsigned char k;

    while (gfx_busy ()) {}
    gfx_setcolor (COLOR_BLACK);
    gfx_clear ();

    /* Grey well frame: side rails plus a floor. */
    for (r = 0; r < ROWS; ++r) {
        block (n, FIELD_X - CELL, FIELD_Y + r * CELL, COLOR_GREY);
        if (prev) prev->next = (char*)&pool[n]; prev = &pool[n]; ++n;
        block (n, FIELD_X + COLS * CELL, FIELD_Y + r * CELL, COLOR_GREY);
        prev->next = (char*)&pool[n]; prev = &pool[n]; ++n;
    }
    for (c = 0; c < COLS + 2; ++c) {
        block (n, FIELD_X - CELL + c * CELL, FIELD_Y + ROWS * CELL, COLOR_GREY);
        prev->next = (char*)&pool[n]; prev = &pool[n]; ++n;
    }

    /* Settled cells. */
    for (r = 0; r < ROWS; ++r) {
        for (c = 0; c < COLS; ++c) {
            if (board[r][c]) {
                block (n, FIELD_X + c * CELL, FIELD_Y + r * CELL,
                       piece_color[board[r][c] - 1]);
                prev->next = (char*)&pool[n]; prev = &pool[n]; ++n;
            }
        }
    }

    /* Falling piece (skip cells above the top of the well). */
    if (state == ST_PLAY || state == ST_PAUSE) {
        s = shapes[cur_type][cur_rot];
        for (k = 0; k < 4; ++k) {
            int cc = cur_x + CELLCOL (s[k]);
            int rr = cur_y + CELLROW (s[k]);
            if (rr >= 0) {
                block (n, FIELD_X + cc * CELL, FIELD_Y + rr * CELL,
                       piece_color[cur_type]);
                prev->next = (char*)&pool[n]; prev = &pool[n]; ++n;
            }
        }
    }

    /* Next-piece preview in the HUD panel. */
    s = shapes[next_type][0];
    for (k = 0; k < 4; ++k) {
        block (n, PANEL_X + 24 + CELLCOL (s[k]) * CELL,
               84 + CELLROW (s[k]) * CELL, piece_color[next_type]);
        prev->next = (char*)&pool[n]; prev = &pool[n]; ++n;
    }

    prev->next = (char*)0;
    gfx_sprite (&pool[0]);

    /* HUD text. */
    gfx_setcolor (COLOR_WHITE);
    gfx_outtextxy (PANEL_X, 2,  "SCORE");
    fmt5 (score, hud);
    gfx_outtextxy (PANEL_X, 11, hud);
    gfx_outtextxy (PANEL_X, 26, "LEVEL");
    fmt5 (level, hud);
    gfx_outtextxy (PANEL_X, 35, hud + 3);       /* last two digits */
    gfx_outtextxy (PANEL_X, 50, "LINES");
    fmt5 (lines_total, hud);
    gfx_outtextxy (PANEL_X, 59, hud);
    gfx_outtextxy (PANEL_X, 74, "NEXT");

    if (state == ST_PAUSE) {
        gfx_setcolor (COLOR_YELLOW);
        gfx_outtextxy (12, 46, "PAUSED");
    } else if (state == ST_OVER) {
        gfx_setcolor (COLOR_RED);
        gfx_outtextxy (6, 38, "GAME OVER");
        gfx_setcolor (COLOR_WHITE);
        gfx_outtextxy (2, 52, "A-AGAIN");
        gfx_outtextxy (2, 62, "B-MENU");
    }

    gfx_updatedisplay ();
}

static void draw_menu (void)
{
    while (gfx_busy ()) {}
    gfx_setcolor (COLOR_BLACK);
    gfx_clear ();

    gfx_settextscale (0x0200, 0x0200);
    gfx_setcolor (COLOR_LIGHTBLUE);
    gfx_outtextxy (16, 14, "TETROIDS");
    gfx_settextscale (0x0100, 0x0100);

    gfx_setcolor (menu_sel == 0 ? COLOR_YELLOW : COLOR_WHITE);
    gfx_outtextxy (40, 50, menu_sel == 0 ? "> START GAME" : "  START GAME");
    gfx_setcolor (menu_sel == 1 ? COLOR_YELLOW : COLOR_WHITE);
    gfx_outtextxy (40, 62, menu_sel == 1 ? "> INSTRUCTIONS" : "  INSTRUCTIONS");

    gfx_setcolor (COLOR_GREY);
    gfx_outtextxy (14, 88, "UP/DOWN, A=SELECT");

    gfx_updatedisplay ();
}

static void draw_instructions (void)
{
    while (gfx_busy ()) {}
    gfx_setcolor (COLOR_BLACK);
    gfx_clear ();

    gfx_setcolor (COLOR_LIGHTBLUE);
    gfx_outtextxy (28, 1, "HOW TO PLAY");

    gfx_setcolor (COLOR_WHITE);
    gfx_outtextxy (2, 14, "Stack blocks to fill");
    gfx_outtextxy (2, 23, "and clear full rows.");

    gfx_setcolor (COLOR_YELLOW);
    gfx_outtextxy (2, 37, "CONTROLS");
    gfx_setcolor (COLOR_WHITE);
    gfx_outtextxy (2, 48, "LEFT/RIGHT  MOVE");
    gfx_outtextxy (2, 57, "DOWN   SOFT DROP");
    gfx_outtextxy (2, 66, "UP     HARD DROP");
    gfx_outtextxy (2, 75, "A / B  ROTATE");
    gfx_outtextxy (2, 84, "PAUSE  PAUSE/RESUME");

    gfx_setcolor (COLOR_GREY);
    gfx_outtextxy (2, 94, "PRESS B TO GO BACK");

    gfx_updatedisplay ();
}

/* ------------------------------------------------------------------ */

void main (void)
{
    gfx_init ();
    CLI ();
    while (gfx_busy ()) {}
    gfx_setdefpalette ();
    gfx_setframerate (60);
    gfx_setcollisiondetection (0);
    gfx_setfont (GFX_FONT_BITMAP);

    state    = ST_MENU;
    menu_sel = 0;
    rng      = 0x1234;
    prev_joy = 0;

    for (;;) {
        signed char dir;

        ++tick;
        joy     = joy_read ();
        pressed = joy & ~prev_joy;
        prev_joy = joy;

        switch (state) {

        case ST_MENU:
            if (pressed & (JOY_UP_MASK | JOY_DOWN_MASK))
                menu_sel ^= 1;
            if (pressed & JOY_BTN_1_MASK) {
                if (menu_sel == 0) new_game ();
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
            /* Horizontal move with delayed auto-repeat. */
            dir = (joy & JOY_LEFT_MASK) ? -1 : (joy & JOY_RIGHT_MASK) ? 1 : 0;
            if (dir) {
                if (dir != last_dir) { try_move (dir); das = 0; }
                else if (++das >= DAS_DELAY &&
                         (das - DAS_DELAY) % DAS_RATE == 0)
                    try_move (dir);
            } else {
                das = 0;
            }
            last_dir = dir;

            if (pressed & JOY_BTN_1_MASK) try_rotate (1);
            if (pressed & JOY_BTN_2_MASK) try_rotate (-1);
            if (pressed & JOY_UP_MASK)    hard_drop ();
            if (pressed & JOY_PAUSE_MASK) state = ST_PAUSE;

            /* Gravity: soft drop while Down is held, else the level rate. */
            if (state == ST_PLAY) {
                unsigned char due = (joy & JOY_DOWN_MASK) ? SOFT_FRAMES
                                                          : fall_frames;
                if (++grav >= due) {
                    grav = 0;
                    if (!collides (cur_x, cur_y + 1, cur_rot)) {
                        ++cur_y;
                        if (joy & JOY_DOWN_MASK) ++score;
                    } else {
                        lock_piece ();
                    }
                }
            }
            draw_playfield ();
            break;

        case ST_PAUSE:
            if (pressed & JOY_PAUSE_MASK) state = ST_PLAY;
            draw_playfield ();
            break;

        case ST_OVER:
            if (pressed & JOY_BTN_1_MASK) new_game ();
            if (pressed & JOY_BTN_2_MASK) { state = ST_MENU; menu_sel = 0; }
            draw_playfield ();
            break;
        }
    }
}
