/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in this directory.
*/

/*
** Minimal Atari Lynx sample for cc65.
**
** Shows the static TGI library (design/LYNX_TGI_DESIGN.md): no driver install,
** just tgi_init(). A hardware-scaled sprite bounces around the screen,
** double buffered, with fractionally scaled text on top.
**
** Controls: A grows the ball, B shrinks it, pad left/right changes
** its color.
**
** Build:  cl65 -Ors -o lynxdemo.lnx lynxdemo.c
*/

#include <lynx/lynx.h>
#include <lynx/tgi.h>
#include <lynx/joystick.h>
#include <6502.h>

/* Ball, 8x8, 4bpp literal. Each sprite data line starts with its byte
** count (including itself) and ends with a trailing pad byte; a 0 count
** ends the sprite.
**
** Pixel values: 0 = transparent (the rounded corners), 1 = body (the
** selectable colour, set into penpal[0] low nibble each frame), 2 = a
** light-grey shine. value 0 maps to pen 0, which a normal sprite leaves
** transparent, so the corners drop out and the ball reads as round.
**
** Suzy drops the last source pixel of every literal scan line (confirmed
** on GearLynx and real hardware), so each line carries one extra pad
** pixel for the engine to drop instead of real imagery. The pad must
** resolve to pen 0; value 0 already does, so the pad byte is 0x00.
** See design/LYNX_SPRITE_PADBYTE_DESIGN.md.
*/
static unsigned char ball_img[] = {
    0x06, 0x00, 0x11, 0x11, 0x00, 0x00,
    0x06, 0x01, 0x21, 0x11, 0x10, 0x00,
    0x06, 0x12, 0x11, 0x11, 0x11, 0x00,
    0x06, 0x11, 0x11, 0x11, 0x11, 0x00,
    0x06, 0x11, 0x11, 0x11, 0x11, 0x00,
    0x06, 0x11, 0x11, 0x11, 0x11, 0x00,
    0x06, 0x01, 0x11, 0x11, 0x10, 0x00,
    0x06, 0x00, 0x11, 0x11, 0x00, 0x00,
    0x00
};

static SCB_REHV_PAL ball = {
    BPP_4 | TYPE_NORMAL, LITERAL | REHV, NO_COLLIDE,
    0, ball_img, 76, 47, 0x0100, 0x0100,
    /* value 0 -> pen 0 (transparent), value 1 -> body (set per frame),
    ** value 2 -> pen 3 (light-grey shine). */
    { 0x00, 0x30, 0, 0, 0, 0, 0, 0 }
};

void main (void)
{
    int x = 76, y = 47;
    int dx = 2, dy = 1;
    unsigned scale = 0x0200;            /* ball size, 8.8 fixed */
    unsigned char pen = COLOR_WHITE;
    unsigned char joy, prev = 0, pressed;
    unsigned char size;                 /* ball size in pixels  */

    tgi_init ();
    CLI ();

    tgi_setframerate (60);
    tgi_settextscale (0x0180, 0x0180);  /* 1.5x text: true 8.8 scaling */

    for (;;) {
        joy = joy_read ();
        pressed = joy & (unsigned char)~prev;
        prev = joy;

        if ((joy & JOY_BTN_A_MASK) && scale < 0x0800) scale += 0x10;
        if ((joy & JOY_BTN_B_MASK) && scale > 0x0080) scale -= 0x10;
        if (pressed & JOY_RIGHT_MASK) pen = (pen + 1) & 0x0F;
        if (pressed & JOY_LEFT_MASK)  pen = (pen - 1) & 0x0F;

        /* Move and bounce */
        size = (unsigned char)(scale >> 5);     /* 8 * scale / 256 */
        x += dx;
        y += dy;
        if (x <= 0)                  { x = 0; dx = 2; }
        if (x >= TGI_XRES - size)    { x = TGI_XRES - size; dx = -2; }
        if (y <= 10)                 { y = 10; dy = 1; }
        if (y >= TGI_YRES - size)    { y = TGI_YRES - size; dy = -1; }

        ball.hpos  = x;
        ball.vpos  = y;
        ball.hsize = scale;
        ball.vsize = scale;
        ball.penpal[0] = pen;           /* value 1 (body) -> selected pen */

        /* Render into the back buffer, then request the swap */
        while (tgi_busy ()) {}
        tgi_setcolor (COLOR_BLACK);     /* tgi_clear fills in draw color */
        tgi_clear ();
        tgi_sprite (&ball);
        tgi_setcolor (COLOR_WHITE);
        tgi_outtextxy (4, 0, "HELLO, LYNX!");
        tgi_updatedisplay ();
    }
}
