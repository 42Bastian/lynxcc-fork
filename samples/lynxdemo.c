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
** Build:  cl65 -t lynx -Ors -o lynxdemo.lnx lynxdemo.c
*/

#include <lynx.h>
#include <tgi.h>
#include <joystick.h>
#include <6502.h>

/* Ball, 8x8, 4bpp literal: pen 1 body, pen 2 highlight. Each sprite
** data line starts with its byte count (including itself); 0 ends the
** sprite.
*/
static unsigned char ball_img[] = {
    0x05, 0x00, 0x11, 0x11, 0x00,
    0x05, 0x01, 0x21, 0x11, 0x10,
    0x05, 0x12, 0x11, 0x11, 0x11,
    0x05, 0x11, 0x11, 0x11, 0x11,
    0x05, 0x11, 0x11, 0x11, 0x11,
    0x05, 0x11, 0x11, 0x11, 0x11,
    0x05, 0x01, 0x11, 0x11, 0x10,
    0x05, 0x00, 0x11, 0x11, 0x00,
    0x00
};

static SCB_REHV_PAL ball = {
    BPP_4 | TYPE_NORMAL, LITERAL | REHV, NO_COLLIDE,
    0, ball_img, 76, 47, 0x0100, 0x0100,
    { 0xF3, 0x00, 0, 0, 0, 0, 0, 0 }    /* pen1 = white, pen2 = l.grey */
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
        joy = joy_read (JOY_1);
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
        ball.penpal[0] = (unsigned char)(pen << 4) | 0x03;

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
