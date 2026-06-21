/*
** sybil.c - Single-screen platformer for the Atari Lynx, starring Sybil.
**
** A small Super-Mario-Bros.-style platformer that fits on one 160x102
** screen (no scrolling). It is built to show three things the Lynx does
** well, and to round out the games shelf next to breakout / invaders /
** raycaster:
**
**   1. Multi-colour hardware sprites with true transparency. Sybil is a
**      16x16, 4bpp sprite lifted straight from the supplied idle frames.
**      The SCB penpal is the identity map (sprite pixel value N -> screen
**      pen N) so her ten colours come straight out of the 12-bit Mikey
**      palette; pixel value 0 is pen 0, which TYPE_NORMAL sprites treat as
**      transparent, so she composites cleanly over the level. Left-facing
**      frames are pre-mirrored arrays (no HFLIP anchor juggling), and the
**      idle/walk/jump animation just swaps the data pointer each frame.
**
**   2. Sprite scaling as a fill primitive. Every platform is one 8x8 solid
**      block sprite stretched to size with the SCB hsize/vsize fields -
**      a brown body plus a green cap, two stretched sprites per ledge, no
**      tile maps. (The same trick the raycaster uses for wall slices.)
**
**   3. Direct Mikey audio (no sound driver): channel A = jump, B = coin /
**      stomp, C = win / game-over jingle, D = ouch. Per-frame envelopes
**      run in the main loop.
**
** Suzy hardware math (!* !/ !%, design/LYNX_CODEGEN_DESIGN.md section 2.6)
** formats the score and computes coin scoring; all sites are in the main
** loop, never in IRQ, honouring the Suzy math contract.
**
** Gameplay: collect every coin to clear the level. Stomp an enemy from
** above to squash it (you bounce); touch one from the side and you lose a
** life. Clearing a level adds another enemy and a little more speed.
**
** Controls: pad left/right runs, A jumps, A restarts after GAME OVER.
**
** Build:  cl65 -Ors -o sybil.lnx sybil.c
*/

#include <lynx.h>
#include <tgi.h>
#include <joystick.h>
#include <6502.h>

/* ------------------------------------------------------------------ */
/* Geometry                                                            */
/* ------------------------------------------------------------------ */

#define SCREEN_W    160
#define SCREEN_H    102

#define SYB_W       16          /* sprite art is 16x16            */
#define SYB_DRAW    2           /* draw x = hitbox x - SYB_DRAW    */
#define SYB_HW      12          /* hitbox width                   */
#define SYB_HH      16          /* hitbox height                  */

#define WALK        2           /* run speed, px/frame            */
#define GRAV        1           /* gravity, px/frame^2            */
#define VMAX        8           /* terminal fall speed            */
#define JUMP        (-9)        /* initial jump velocity          */
#define BOUNCE      (-6)        /* stomp rebound                  */

#define EN_W        12          /* enemy sprite/hitbox            */
#define EN_H        10
#define COIN_W      8
#define COIN_H      8

#define START_X     8
#define START_Y     (94 - SYB_HH)

/* ------------------------------------------------------------------ */
/* Hardware pens. Pens 0,1,3,4,7,10..15 carry Sybil's own palette       */
/* (index == pen, identity penpal). The five "free" pens 2,5,6,8,9 are  */
/* the level/enemy/coin colours.                                        */
/* ------------------------------------------------------------------ */

#define PEN_TRANS    0          /* transparent (also = sky behind her) */
#define PEN_BLACK    1
#define PEN_BRICK    2          /* platform body  (free)               */
#define PEN_SKY      5          /* background     (free)               */
#define PEN_ENEMY    6          /* enemy body     (free)               */
#define PEN_COIN     8          /* coin gold      (free)               */
#define PEN_GRASS    9          /* platform cap   (free)               */
#define PEN_TEXT    15          /* HUD / Sybil outline                 */

/* Game states */
#define ST_PLAY     0
#define ST_CLEAR    1           /* level-clear banner pause */
#define ST_OVER     2

/* ------------------------------------------------------------------ */
/* Sprite image data: 4bpp LITERAL. Each line is a byte count (incl.    */
/* itself) followed by the packed pixel-pair bytes (high nibble = left)  */
/* and a trailing 0x00 pad byte; a 0 count ends the sprite. The pad     */
/* works around Suzy's last-pixel bug (it drops the final pixel of every */
/* literal line). Sybil's frames use the identity penpal, so the nibble  */
/* values are palette pens directly and value 0 = pen 0 (transparent),   */
/* making the 0x00 pad invisible. See design/LYNX_SPRITE_PADBYTE_DESIGN.md.*/
/* ------------------------------------------------------------------ */

/* Sybil idle/run frames, 16x16, right-facing then pre-mirrored left.   */
static unsigned char syb0[] = {
    0x0A, 0x00, 0x00, 0x00, 0xFF, 0xF0, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x0F, 0x77, 0x7F, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xF7, 0x71, 0x33, 0xF0, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xF7, 0xA1, 0x11, 0xF0, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xF7, 0x7A, 0xBF, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x0F, 0xED, 0x4A, 0xFF, 0xFF, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x0F, 0xEB, 0xB4, 0x41, 0xCC, 0xF0, 0x00, 0x00,
    0x0A, 0x00, 0x0F, 0xDA, 0xAB, 0x3A, 0x1C, 0xCF, 0x00, 0x00,
    0x0A, 0x00, 0x0F, 0xDC, 0x3A, 0xBB, 0xBF, 0xF0, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xFC, 0x31, 0xFF, 0x1F, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xF3, 0x14, 0x4F, 0xF0, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x0F, 0x33, 0x34, 0xF0, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xF1, 0x3F, 0x14, 0xF0, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x0F, 0x13, 0xF0, 0xF1, 0x3F, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x0F, 0x11, 0x1F, 0xF1, 0x11, 0xF0, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xFF, 0xF0, 0x0F, 0xFF, 0x00, 0x00, 0x00,
    0x00
};
static unsigned char syb0l[] = {
    0x0A, 0x00, 0x00, 0x00, 0x0F, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0xF7, 0x77, 0xF0, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x0F, 0x33, 0x17, 0x7F, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x0F, 0x11, 0x1A, 0x7F, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0xFB, 0xA7, 0x7F, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xFF, 0xFF, 0xA4, 0xDE, 0xF0, 0x00, 0x00,
    0x0A, 0x00, 0x0F, 0xCC, 0x14, 0x4B, 0xBE, 0xF0, 0x00, 0x00,
    0x0A, 0x00, 0xFC, 0xC1, 0xA3, 0xBA, 0xAD, 0xF0, 0x00, 0x00,
    0x0A, 0x00, 0x0F, 0xFB, 0xBB, 0xA3, 0xCD, 0xF0, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xF1, 0xFF, 0x13, 0xCF, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x0F, 0xF4, 0x41, 0x3F, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x0F, 0x43, 0x33, 0xF0, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x0F, 0x41, 0xF3, 0x1F, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xF3, 0x1F, 0x0F, 0x31, 0xF0, 0x00, 0x00,
    0x0A, 0x00, 0x0F, 0x11, 0x1F, 0xF1, 0x11, 0xF0, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xFF, 0xF0, 0x0F, 0xFF, 0x00, 0x00, 0x00,
    0x00
};
static unsigned char syb1[] = {
    0x0A, 0x00, 0x00, 0x00, 0xFF, 0xF0, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x0F, 0x77, 0x7F, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xF7, 0x71, 0x33, 0xF0, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xF7, 0xA1, 0x11, 0xF0, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xF7, 0x7A, 0xBF, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x0F, 0xED, 0x4A, 0xF0, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x0F, 0xEB, 0xB4, 0x4F, 0xFF, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x0F, 0xDA, 0xAB, 0x31, 0xCC, 0xF0, 0x00, 0x00,
    0x0A, 0x00, 0x0F, 0xDC, 0x3A, 0xBA, 0x1C, 0xCF, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xFC, 0x31, 0xFB, 0xBF, 0xF0, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xF3, 0x14, 0x4F, 0x1F, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x0F, 0x33, 0x34, 0xF0, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xF1, 0x3F, 0x14, 0xF0, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x0F, 0x13, 0xF0, 0xF1, 0x3F, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x0F, 0x11, 0x1F, 0xF1, 0x11, 0xF0, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xFF, 0xF0, 0x0F, 0xFF, 0x00, 0x00, 0x00,
    0x00
};
static unsigned char syb1l[] = {
    0x0A, 0x00, 0x00, 0x00, 0x0F, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0xF7, 0x77, 0xF0, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x0F, 0x33, 0x17, 0x7F, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x0F, 0x11, 0x1A, 0x7F, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0xFB, 0xA7, 0x7F, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0x0F, 0xA4, 0xDE, 0xF0, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xFF, 0xF4, 0x4B, 0xBE, 0xF0, 0x00, 0x00,
    0x0A, 0x00, 0x0F, 0xCC, 0x13, 0xBA, 0xAD, 0xF0, 0x00, 0x00,
    0x0A, 0x00, 0xFC, 0xC1, 0xAB, 0xA3, 0xCD, 0xF0, 0x00, 0x00,
    0x0A, 0x00, 0x0F, 0xFB, 0xBF, 0x13, 0xCF, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xF1, 0xF4, 0x41, 0x3F, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x0F, 0x43, 0x33, 0xF0, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x0F, 0x41, 0xF3, 0x1F, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xF3, 0x1F, 0x0F, 0x31, 0xF0, 0x00, 0x00,
    0x0A, 0x00, 0x0F, 0x11, 0x1F, 0xF1, 0x11, 0xF0, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xFF, 0xF0, 0x0F, 0xFF, 0x00, 0x00, 0x00,
    0x00
};
static unsigned char syb2[] = {
    0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0xFF, 0xF0, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x0F, 0x77, 0x7F, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xF7, 0x11, 0x33, 0xF0, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xF7, 0xA1, 0x11, 0xF0, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xF7, 0x7A, 0xBF, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x0F, 0xED, 0x4A, 0xFF, 0xF0, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x0F, 0xED, 0xBB, 0x4C, 0xCF, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x0F, 0xDD, 0xAB, 0x3F, 0xFF, 0xFF, 0x00, 0x00,
    0x0A, 0x00, 0x0F, 0xDC, 0x3A, 0xBF, 0xF1, 0xCC, 0xF0, 0x00,
    0x0A, 0x00, 0x00, 0xFC, 0x11, 0xAB, 0xBB, 0xFF, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xF3, 0x33, 0x44, 0xF1, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xFF, 0x33, 0x33, 0x4F, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x0F, 0x11, 0x3F, 0xF1, 0x13, 0xF0, 0x00, 0x00,
    0x0A, 0x00, 0x0F, 0x11, 0x1F, 0x0F, 0x11, 0x1F, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xFF, 0xF0, 0x00, 0xFF, 0xF0, 0x00, 0x00,
    0x00
};
static unsigned char syb2l[] = {
    0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0x0F, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0xF7, 0x77, 0xF0, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x0F, 0x33, 0x11, 0x7F, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x0F, 0x11, 0x1A, 0x7F, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0xFB, 0xA7, 0x7F, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x0F, 0xFF, 0xA4, 0xDE, 0xF0, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xFC, 0xC4, 0xBB, 0xDE, 0xF0, 0x00, 0x00,
    0x0A, 0x00, 0xFF, 0xFF, 0xF3, 0xBA, 0xDD, 0xF0, 0x00, 0x00,
    0x0A, 0x0F, 0xCC, 0x1F, 0xFB, 0xA3, 0xCD, 0xF0, 0x00, 0x00,
    0x0A, 0x00, 0xFF, 0xBB, 0xBA, 0x11, 0xCF, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x1F, 0x44, 0x33, 0x3F, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0xF4, 0x33, 0x33, 0xFF, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x0F, 0x31, 0x1F, 0xF3, 0x11, 0xF0, 0x00, 0x00,
    0x0A, 0x00, 0xF1, 0x11, 0xF0, 0xF1, 0x11, 0xF0, 0x00, 0x00,
    0x0A, 0x00, 0x0F, 0xFF, 0x00, 0x0F, 0xFF, 0x00, 0x00, 0x00,
    0x00
};

/* Enemy critter, 12x10, two walk frames. Pixel value 1 = body (PEN_ENEMY),
** 2 = eye white (PEN_TEXT), 3 = pupil (PEN_BLACK). */
static unsigned char en_a[] = {
    0x08, 0x00, 0x00, 0x11, 0x11, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00,
    0x08, 0x01, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00,
    0x08, 0x01, 0x12, 0x21, 0x12, 0x21, 0x10, 0x00,
    0x08, 0x01, 0x12, 0x31, 0x12, 0x31, 0x10, 0x00,
    0x08, 0x01, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00,
    0x08, 0x01, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00,
    0x08, 0x00, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00,
    0x08, 0x01, 0x11, 0x00, 0x00, 0x11, 0x10, 0x00,
    0x08, 0x11, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00,
    0x00
};
static unsigned char en_b[] = {
    0x08, 0x00, 0x00, 0x11, 0x11, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00,
    0x08, 0x01, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00,
    0x08, 0x01, 0x12, 0x21, 0x12, 0x21, 0x10, 0x00,
    0x08, 0x01, 0x12, 0x31, 0x12, 0x31, 0x10, 0x00,
    0x08, 0x01, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00,
    0x08, 0x01, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00,
    0x08, 0x00, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00,
    0x08, 0x00, 0x11, 0x10, 0x01, 0x11, 0x00, 0x00,
    0x08, 0x01, 0x10, 0x00, 0x00, 0x01, 0x10, 0x00,
    0x00
};

/* Coin, 8x8. Value 1 = gold (PEN_COIN), 2 = shine (PEN_TEXT). */
static unsigned char coin_img[] = {
    0x06, 0x00, 0x11, 0x11, 0x00, 0x00,
    0x06, 0x01, 0x11, 0x11, 0x10, 0x00,
    0x06, 0x11, 0x21, 0x11, 0x11, 0x00,
    0x06, 0x11, 0x21, 0x11, 0x11, 0x00,
    0x06, 0x11, 0x21, 0x11, 0x11, 0x00,
    0x06, 0x11, 0x11, 0x11, 0x11, 0x00,
    0x06, 0x01, 0x11, 0x11, 0x10, 0x00,
    0x06, 0x00, 0x11, 0x11, 0x00, 0x00,
    0x00
};

/* 8x8 solid block; stretched to any rectangle for platforms. Value 1
** is recoloured per draw via the SCB penpal. */
static unsigned char blk_img[] = {
    0x06, 0x11, 0x11, 0x11, 0x11, 0x00,
    0x06, 0x11, 0x11, 0x11, 0x11, 0x00,
    0x06, 0x11, 0x11, 0x11, 0x11, 0x00,
    0x06, 0x11, 0x11, 0x11, 0x11, 0x00,
    0x06, 0x11, 0x11, 0x11, 0x11, 0x00,
    0x06, 0x11, 0x11, 0x11, 0x11, 0x00,
    0x06, 0x11, 0x11, 0x11, 0x11, 0x00,
    0x06, 0x11, 0x11, 0x11, 0x11, 0x00,
    0x00
};

/* ------------------------------------------------------------------ */
/* Palette. 32 bytes: 16 green nibbles then 16 red/blue bytes (GCOLMAP).*/
/* Colours are 12-bit $GRB, split into the two halves.                 */
/* ------------------------------------------------------------------ */

#define GRB_G(c)    ((unsigned char)(((c) >> 8) & 0x0F))
#define GRB_RB(c)   ((unsigned char)((c) & 0xFF))

static unsigned char pal[32];

/* Pen colours. Sybil's ten come from her source palette; the five free
** pens are the world. Pen 0 doubles as the sky colour behind her. */
static const unsigned int pen_base[16] = {
    0xB7F,      /* 0  transparent / sky               */
    0x000,      /* 1  black (Sybil outline + text)    */
    0x5A2,      /* 2  brick brown (platform body)     */
    0x555,      /* 3  Sybil grey                      */
    0x888,      /* 4  Sybil light grey                */
    0xB7F,      /* 5  sky blue (background)           */
    0x29C,      /* 6  enemy purple                    */
    0x080,      /* 7  Sybil dark red (hair)           */
    0xDF0,      /* 8  coin gold                       */
    0xC55,      /* 9  grass green (platform cap)      */
    0x8F4,      /* 10 Sybil skin                      */
    0xBF9,      /* 11 Sybil light skin                */
    0x00F,      /* 12 Sybil blue                      */
    0x73F,      /* 13 Sybil mid blue                  */
    0xA5F,      /* 14 Sybil light blue                */
    0xC9F       /* 15 Sybil outline / HUD text        */
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
/* Sound: direct Mikey audio register access (see invaders.c for the    */
/* register notes). feedback 0x01 = square tone, 0x3F = noise.          */
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

/* Per-effect frame counters. */
static unsigned char jmp_t,  jmp_p;     /* channel A */
static unsigned char blip_t, blip_v;    /* channel B (coin + stomp) */
static unsigned char hurt_t, hurt_p;    /* channel D */

/* A tiny channel-C sequencer for the win / game-over jingles. */
static const unsigned char tune_win[]  = { 150, 120, 100, 80, 64, 0 };
static const unsigned char tune_over[] = { 80, 100, 130, 170, 210, 0 };
static const unsigned char* tune;
static unsigned char tune_t;

static void play_tune (const unsigned char* t) { tune = t; tune_t = 0; }

static void sfx_jump (void)
{
    jmp_p = 130;
    jmp_t = 9;
    snd_voice (&MIKEY.channel_a, 0x32, FB_TONE, 3, jmp_p);
}

static void sfx_coin (void)
{
    blip_v = 0x40;
    blip_t = 6;
    snd_voice (&MIKEY.channel_b, blip_v, FB_TONE, 2, 70);
}

static void sfx_stomp (void)
{
    blip_v = 0x50;
    blip_t = 7;
    snd_voice (&MIKEY.channel_b, blip_v, FB_NOISE, 3, 0x24);
}

static void sfx_hurt (void)
{
    hurt_p = 70;
    hurt_t = 26;
    snd_voice (&MIKEY.channel_d, 0x46, FB_TONE, 4, hurt_p);
}

/* Advance every sound envelope one frame. */
static void sfx_update (void)
{
    if (jmp_t) {
        if (jmp_p > 10) jmp_p -= 10;        /* pitch slides up */
        MIKEY.channel_a.reload = jmp_p;
        if (--jmp_t == 0) snd_silence (&MIKEY.channel_a);
    }
    if (blip_t) {
        if (blip_v > 8) blip_v -= 8;        /* decay */
        MIKEY.channel_b.volume = blip_v;
        if (blip_t == 3) MIKEY.channel_b.reload = 48;   /* coin second tone */
        if (--blip_t == 0) snd_silence (&MIKEY.channel_b);
    }
    if (hurt_t) {
        hurt_p += 4;                        /* pitch slides down */
        MIKEY.channel_d.reload = hurt_p;
        if (--hurt_t == 0) snd_silence (&MIKEY.channel_d);
    }
    if (tune) {
        if (tune_t == 0) {
            if (*tune == 0) { tune = 0; snd_silence (&MIKEY.channel_c); }
            else { snd_voice (&MIKEY.channel_c, 0x3E, FB_TONE, 5, *tune++);
                   tune_t = 11; }
        } else --tune_t;
    }
}

/* ------------------------------------------------------------------ */
/* Level geometry. Platform 0 is the full-width ground; the rest are     */
/* one-way ledges (land on the top, jump up through them).               */
/* ------------------------------------------------------------------ */

#define NPLAT 6
static const int plat_x[NPLAT] = {   0,  12, 116,  60,   4, 124 };
static const int plat_y[NPLAT] = {  94,  72,  72,  52,  34,  34 };
static const int plat_w[NPLAT] = { 160,  40,  40,  40,  30,  30 };
static const int plat_h[NPLAT] = {   8,   6,   6,   6,   6,   6 };

#define NCOIN 8
static const int coin_sx[NCOIN] = { 26, 130, 76, 14, 134, 50, 100, 78 };
static const int coin_sy[NCOIN] = { 60,  60, 40, 22,  22, 82,  82, 82 };
static unsigned char coin_on[NCOIN];
static unsigned char coins_left;

/* Up to four enemies; how many are live scales with the level. Each one
** patrols the top of one platform. */
#define MAXEN 4
static const int           en_spawn[MAXEN] = { 40, 120, 70, 18 };
static const unsigned char en_plat[MAXEN]  = {  0,   0,  3,  4 };
static int          en_x[MAXEN], en_y[MAXEN], en_lo[MAXEN], en_hi[MAXEN];
static signed char  en_dir[MAXEN];
static unsigned char en_alive[MAXEN];
static unsigned char nen, en_speed, en_left;

/* ------------------------------------------------------------------ */
/* Sprite control blocks                                                */
/* ------------------------------------------------------------------ */

/* Sybil: identity penpal (sprite pixel value N -> screen pen N). */
static SCB_REHV_PAL syb_scb = {
    BPP_4 | TYPE_NORMAL, LITERAL | REHV, NO_COLLIDE,
    0, syb0, 0, 0, 0x0100, 0x0100,
    { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF }
};
static SCB_REHV_PAL en_scb = {
    BPP_4 | TYPE_NORMAL, LITERAL | REHV, NO_COLLIDE,
    0, en_a, 0, 0, 0x0100, 0x0100,
    { (PEN_TRANS << 4) | PEN_ENEMY, (PEN_TEXT << 4) | PEN_BLACK, 0, 0, 0, 0, 0, 0 }
};
static SCB_REHV_PAL coin_scb = {
    BPP_4 | TYPE_NORMAL, LITERAL | REHV, NO_COLLIDE,
    0, coin_img, 0, 0, 0x0100, 0x0100,
    { (PEN_TRANS << 4) | PEN_COIN, (PEN_TEXT << 4) | PEN_TRANS, 0, 0, 0, 0, 0, 0 }
};
static SCB_REHV_PAL blk_scb = {
    BPP_4 | TYPE_NORMAL, LITERAL | REHV, NO_COLLIDE,
    0, blk_img, 0, 0, 0x0100, 0x0100,
    { (PEN_TRANS << 4) | PEN_BRICK, 0, 0, 0, 0, 0, 0, 0 }
};

/* ------------------------------------------------------------------ */
/* Game state                                                          */
/* ------------------------------------------------------------------ */

static int  px, py;             /* Sybil hitbox top-left            */
static int  vy;                 /* vertical velocity                */
static unsigned char on_ground;
static unsigned char face;      /* 0 = right, 1 = left              */
static unsigned char anim, anim_t;     /* frame + timer             */

static unsigned int  score;
static unsigned char lives, level, state;
static unsigned char clear_t;
static unsigned char invuln;    /* post-hit blink/grace, frames     */
static unsigned char joy, prev_joy, pressed;
static unsigned char shine;     /* coin colour cycle phase          */

static char hud_score[] = "00000";
static char hud_coin[]  = "COIN 0";
static char hud_life[]  = "LIFE 3";

static void fmt5 (unsigned int v, char* p)
{
    unsigned char i;
    for (i = 5; i--; ) {
        p[i] = '0' + (char)(v !% 10);          /* Suzy modulo */
        v = v !/ 10;                            /* Suzy divide */
    }
}

/* ------------------------------------------------------------------ */

static void place_sybil (void)
{
    px = START_X;
    py = START_Y;
    vy = 0;
    on_ground = 1;
    face = 0;
}

static void start_level (void)
{
    unsigned char i;

    for (i = 0; i < NCOIN; ++i) coin_on[i] = 1;
    coins_left = NCOIN;

    nen = (unsigned char)(2 + level);
    if (nen > MAXEN) nen = MAXEN;
    en_left = nen;
    en_speed = (unsigned char)(1 + (level >> 1));
    if (en_speed > 3) en_speed = 3;

    for (i = 0; i < MAXEN; ++i) {
        unsigned char p = en_plat[i];
        en_lo[i]    = plat_x[p];
        en_hi[i]    = plat_x[p] + plat_w[p];
        en_x[i]     = en_spawn[i];
        en_y[i]     = plat_y[p] - EN_H;
        en_dir[i]   = (i & 1) ? -1 : 1;
        en_alive[i] = (i < nen) ? 1 : 0;
    }

    place_sybil ();
    invuln = 40;
    state = ST_PLAY;
}

static void new_game (void)
{
    score = 0;
    lives = 3;
    level = 0;
    start_level ();
}

/* Land Sybil on the highest one-way platform her feet cross this frame. */
static void resolve_platforms (void)
{
    unsigned char i;
    int feet_prev = py + SYB_HH - vy;
    int feet_new  = py + SYB_HH;
    int best = -1, besty = 0;

    on_ground = 0;
    if (vy < 0) return;                         /* rising: pass through */

    for (i = 0; i < NPLAT; ++i) {
        int top = plat_y[i];
        if (px + SYB_HW > plat_x[i] && px < plat_x[i] + plat_w[i] &&
            feet_prev <= top && feet_new >= top) {
            if (best < 0 || top < besty) { best = (int)i; besty = top; }
        }
    }
    if (best >= 0) { py = besty - SYB_HH; vy = 0; on_ground = 1; }
}

static unsigned char overlap (int ax, int ay, int aw, int ah,
                              int bx, int by, int bw, int bh)
{
    return (ax < bx + bw && ax + aw > bx &&
            ay < by + bh && ay + ah > by);
}

static void hurt (void)
{
    sfx_hurt ();
    if (--lives == 0) {
        state = ST_OVER;
        play_tune (tune_over);
    } else {
        place_sybil ();
        invuln = 80;
    }
}

static void update_play (void)
{
    unsigned char i, moving = 0;

    /* Horizontal run */
    if (joy & JOY_LEFT_MASK)  { px -= WALK; face = 1; moving = 1; }
    if (joy & JOY_RIGHT_MASK) { px += WALK; face = 0; moving = 1; }
    if (px < 0) px = 0;
    if (px > SCREEN_W - SYB_HW) px = SCREEN_W - SYB_HW;

    /* Jump / gravity */
    if (on_ground && (pressed & JOY_BTN_1_MASK)) { vy = JUMP; sfx_jump (); }
    vy += GRAV;
    if (vy > VMAX) vy = VMAX;
    py += vy;
    resolve_platforms ();

    if (py > SCREEN_H) { hurt (); return; }     /* fell off the world */

    /* Animation: jump pose airborne, run cycle moving, slow idle else. */
    if (++anim_t >= (on_ground ? (moving ? 5 : 16) : 250)) {
        anim_t = 0;
        if (++anim > 2) anim = 0;
    }

    /* Enemies */
    for (i = 0; i < nen; ++i) {
        if (!en_alive[i]) continue;
        en_x[i] += en_dir[i] * (int)en_speed;
        if (en_x[i] <= en_lo[i])          { en_x[i] = en_lo[i];          en_dir[i] = 1; }
        if (en_x[i] + EN_W >= en_hi[i])   { en_x[i] = en_hi[i] - EN_W;   en_dir[i] = -1; }

        if (overlap (px, py, SYB_HW, SYB_HH, en_x[i], en_y[i], EN_W, EN_H)) {
            int feet_prev = py + SYB_HH - vy;
            if (vy > 0 && feet_prev <= en_y[i] + 5) {   /* stomp */
                en_alive[i] = 0;
                score += 3 !* 50;                       /* 150, via Suzy */
                vy = BOUNCE;
                sfx_stomp ();
                --en_left;
            } else if (!invuln) {
                hurt ();
                return;
            }
        }
    }

    /* Coins */
    for (i = 0; i < NCOIN; ++i) {
        if (!coin_on[i]) continue;
        if (overlap (px, py, SYB_HW, SYB_HH,
                     coin_sx[i], coin_sy[i], COIN_W, COIN_H)) {
            coin_on[i] = 0;
            score += 50;
            sfx_coin ();
            if (--coins_left == 0) {
                score += 500;
                state = ST_CLEAR;
                clear_t = 90;
                play_tune (tune_win);
                return;
            }
        }
    }

    if (invuln) --invuln;
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */

/* One platform = a stretched brown body with a green cap on top. */
static void draw_platform (unsigned char i)
{
    int w = plat_w[i], h = plat_h[i];

    blk_scb.hpos    = plat_x[i];
    blk_scb.hsize   = (unsigned int)(w << 5);   /* w/8 * 256 = w*32 */

    blk_scb.penpal[0] = (PEN_TRANS << 4) | PEN_BRICK;
    blk_scb.vpos    = plat_y[i] + 3;
    blk_scb.vsize   = (unsigned int)((h - 3) << 5);
    tgi_sprite (&blk_scb);

    blk_scb.penpal[0] = (PEN_TRANS << 4) | PEN_GRASS;
    blk_scb.vpos    = plat_y[i];
    blk_scb.vsize   = (unsigned int)(3 << 5);
    tgi_sprite (&blk_scb);
}

static void draw (void)
{
    unsigned char i;
    unsigned char* frame;

    while (tgi_busy ()) {}
    tgi_setcolor (PEN_SKY);
    tgi_clear ();

    for (i = 0; i < NPLAT; ++i) draw_platform (i);

    /* Coins */
    for (i = 0; i < NCOIN; ++i) {
        if (!coin_on[i]) continue;
        coin_scb.hpos = coin_sx[i];
        coin_scb.vpos = coin_sy[i];
        tgi_sprite (&coin_scb);
    }

    /* Enemies */
    en_scb.data = (shine & 8) ? en_b : en_a;
    for (i = 0; i < nen; ++i) {
        if (!en_alive[i]) continue;
        en_scb.hpos = en_x[i];
        en_scb.vpos = en_y[i];
        tgi_sprite (&en_scb);
    }

    /* Sybil (blink while invulnerable) */
    if (!(invuln & 2)) {
        if (!on_ground) frame = face ? syb2l : syb2;   /* jump pose */
        else if (face)  frame = (anim == 0) ? syb0l : (anim == 1) ? syb1l : syb2l;
        else            frame = (anim == 0) ? syb0  : (anim == 1) ? syb1  : syb2;
        syb_scb.data = frame;
        syb_scb.hpos = px - SYB_DRAW;
        syb_scb.vpos = py;
        tgi_sprite (&syb_scb);
    }

    /* HUD */
    fmt5 (score, hud_score);
    hud_coin[5] = '0' + (NCOIN - coins_left);
    hud_life[5] = '0' + lives;
    tgi_setcolor (PEN_BLACK);
    tgi_outtextxy (0, 0, hud_score);
    tgi_outtextxy (56, 0, hud_coin);
    tgi_outtextxy (112, 0, hud_life);

    if (state == ST_CLEAR) {
        tgi_setcolor (PEN_COIN);
        tgi_outtextxy (36, 44, "LEVEL CLEAR");
    } else if (state == ST_OVER) {
        tgi_setcolor (PEN_ENEMY);
        tgi_outtextxy (44, 40, "GAME OVER");
        tgi_setcolor (PEN_BLACK);
        tgi_outtextxy (28, 56, "A = NEW GAME");
    }

    tgi_updatedisplay ();
}

/* ------------------------------------------------------------------ */

void main (void)
{
    tgi_init ();
    CLI ();
    while (tgi_busy ()) {}

    MIKEY.mstereo = 0x00;
    snd_silence (&MIKEY.channel_a);
    snd_silence (&MIKEY.channel_b);
    snd_silence (&MIKEY.channel_c);
    snd_silence (&MIKEY.channel_d);

    pal_init ();
    tgi_setpalette (pal);
    tgi_setframerate (60);
    tgi_setcollisiondetection (0);

    new_game ();
    prev_joy = 0;

    for (;;) {
        joy = (unsigned char)joy_read (JOY_1);
        pressed = joy & (unsigned char)~prev_joy;
        prev_joy = joy;

        switch (state) {
            case ST_PLAY:
                update_play ();
                break;
            case ST_CLEAR:
                if (--clear_t == 0) { ++level; start_level (); }
                break;
            case ST_OVER:
                if (pressed & JOY_BTN_1_MASK) new_game ();
                break;
        }

        ++shine;
        /* Coin gold shimmers between two tones. */
        pal_set (PEN_COIN, (shine & 16) ? 0xFE0 : 0xDF0);

        sfx_update ();
        tgi_setpalette (pal);
        draw ();
    }
}
