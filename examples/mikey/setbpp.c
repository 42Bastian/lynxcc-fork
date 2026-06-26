/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in this directory.
*/

/*
** gfx_setbpp() demo for cc65: the Lynx 2-bit/monochrome display mode.
**
** DISPCTL B2 selects how Mikey's display DMA interprets the frame
** buffer: 4bpp (normal, Suzy-rendered) or 2bpp. In 2bpp Mikey scans
** out 40 bytes/line x 102 lines = 4080 bytes/page, 4 pixels/byte
** (MSB-first, like the 4bpp nibble order), and the 2-bit pen numbers
** index palette entries 0-3. Suzy is unaffected: the sprite engine
** always renders 4bpp, so gfx_sprite/gfx_outtext output scans out
** garbled in 2bpp - the mode is a CPU-rendered framebuffer, and this
** program writes the buffer itself. The upper 4080 bytes of each page
** are free for application use. See design/LYNX_GFX_DESIGN.md sec. 2.7; the
** mode relies on a DISPCTL bit outside spec guarantees and is
** unverified on real hardware.
**
** The demo boots into the normal 4bpp sprite/text screen, then
** toggles into 2bpp on demand. A greyscale palette is used so 2bpp
** acts as a monochrome mode: pens 0-3 = black, dark grey, light
** grey, white. The 2bpp screen shows the four shades as solid bands,
** three ordered-dither fields (25/50/75% white from pens 0/3 only -
** classic monochrome shading), and a CPU-animated bouncing block.
** The page-swap machinery (gfx_updatedisplay/gfx_busy) is depth-
** independent, so the 2bpp screen is double buffered like any other.
**
** Controls: A toggles 4bpp/2bpp, pad up rotates the display 180
** degrees (gfx_flip honors the 2bpp end-of-buffer offset).
**
** Build:  cl65 -Ors -o setbpp.lnx setbpp.c
*/

#include <lynx/lynx.h>
#include <lynx/gfx.h>
#include <lynx/joystick.h>
#include <6502.h>
#include <string.h>

/* 2bpp framebuffer geometry */
#define LINE2BPP        40                      /* Bytes per line   */
#define SIZE2BPP        (LINE2BPP * GFX_YRES)   /* 4080 bytes/page  */

/* Lynx graphics page base addresses (same in both depths) */
static unsigned char* const page[2] = {
    (unsigned char*) 0xE018,                    /* Lynx graphics page 0 */
    (unsigned char*) 0xC038                     /* Lynx graphics page 1 */
};

/* Greyscale ramp: pen n = grey n/15. Makes 2bpp a monochrome mode
** (pens 0-3 = black, dark grey, light grey, white) and keeps the
** 4bpp screen consistent. 16 green bytes, then 16 blue/red bytes.
*/
static const unsigned char mono_pal[32] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
};

/* Solid 4x4 literal sprite; scaled up to draw the shade bands on the
** 4bpp screen. The body is pixel value 0, recolored per band through
** the penpal high nibble (render4 sets penpal[0] = (pen << 4) | 3),
** exactly like lynxdemo recolors its ball - so the four bands show
** pens 0..3, the same ramp the 2bpp screen draws.
**
** Each line ends with a trailing pad byte (offset incremented to
** match) for Suzy's last-pixel bug: it drops the final pixel of every
** literal line, so without the pad the 40x-stretched band would lose
** its rightmost slice. Because value 0 is the (opaque) band colour, a
** 0x00 pad would show; pixel value 2 maps to pen 0 here, so the pad
** byte is 0x22. See design/LYNX_SPRITE_PADBYTE_DESIGN.md.
*/
static unsigned char band_img[] = {
    0x04, 0x00, 0x00, 0x22,
    0x04, 0x00, 0x00, 0x22,
    0x04, 0x00, 0x00, 0x22,
    0x04, 0x00, 0x00, 0x22,
    0x00
};

static SCB_REHV_PAL band = {
    BPP_4 | TYPE_NORMAL, LITERAL | REHV, NO_COLLIDE,
    0, band_img, 0, 0, 0x2800, 0x0280,          /* 4x4 -> 160x10 */
    { 0x03, 0x00, 0, 0, 0, 0, 0, 0 }
};

/* Set one 2bpp pixel: 4 pixels/byte, leftmost pixel in the two most
** significant bits (matching the 4bpp nibble order).
*/
static void setpixel2 (unsigned char* scrn, unsigned char x,
                       unsigned char y, unsigned char pen)
{
    unsigned char* p = scrn + (unsigned) y * LINE2BPP + (x >> 2);
    unsigned char shift = (unsigned char) ((3 - (x & 3)) << 1);
    *p = (unsigned char) ((*p & (unsigned char) ~(0x03 << shift)) |
                          ((pen & 0x03) << shift));
}

/* Draw an 8x8 block at any pixel position */
static void block2 (unsigned char* scrn, unsigned char x,
                    unsigned char y, unsigned char pen)
{
    unsigned char i, j;
    for (j = 0; j < 8; ++j) {
        for (i = 0; i < 8; ++i) {
            setpixel2 (scrn, x + i, y + j, pen);
        }
    }
}

/* CPU-render the static 2bpp screen into one page. A solid byte of
** pen p is p * $55 (the pen repeated in all four 2-bit pixels).
*/
static void render2 (unsigned char* scrn)
{
    unsigned char* p = scrn;
    unsigned char row;

    /* Four solid shade bands: pens 0-3, 10 lines each */
    for (row = 0; row < 4; ++row) {
        memset (p, row * 0x55, LINE2BPP * 10);
        p += LINE2BPP * 10;
    }

    /* White rule */
    memset (p, 0xFF, LINE2BPP * 2);
    p += LINE2BPP * 2;

    /* Monochrome shading by ordered dither, pens 0/3 only:
    ** 25% white ($CC = pens 3,0,3,0 on even lines, black odd lines),
    ** 50% checkerboard ($CC/$33), 75% ($33 even, solid white odd).
    */
    for (row = 0; row < 16; ++row, p += LINE2BPP) {
        memset (p, (row & 1) ? 0x00 : 0xCC, LINE2BPP);
    }
    for (row = 0; row < 16; ++row, p += LINE2BPP) {
        memset (p, (row & 1) ? 0x33 : 0xCC, LINE2BPP);
    }
    for (row = 0; row < 16; ++row, p += LINE2BPP) {
        memset (p, (row & 1) ? 0xFF : 0x33, LINE2BPP);
    }

    /* White rule */
    memset (p, 0xFF, LINE2BPP * 2);
    p += LINE2BPP * 2;

    /* Animation strip: black; main loop bounces a block here */
    memset (p, 0x00, LINE2BPP * 10);
}

/* Render one 4bpp frame (Suzy sprites + text) into the draw page */
static void render4 (void)
{
    unsigned char pen;

    gfx_setcolor (0);                   /* gfx_clear fills in draw color */
    gfx_clear ();

    /* The same four shades as the 2bpp screen, drawn by the sprite
    ** engine this time
    */
    for (pen = 0; pen < 4; ++pen) {
        band.vpos = pen * 10;
        band.penpal[0] = (unsigned char) (pen << 4) | 0x03;
        gfx_sprite (&band);
    }

    gfx_setcolor (15);                  /* White in the grey ramp */
    gfx_outtextxy (8, 48, "GFX_SETBPP DEMO");
    gfx_setcolor (10);
    gfx_outtextxy (8, 62, "MODE: 4BPP SPRITES");
    gfx_outtextxy (8, 76, "A: 2BPP  UP: FLIP");
}

void main (void)
{
    unsigned char bpp = 4;
    unsigned char back = 0;             /* CPU mirror of the back page  */
    unsigned char joy, prev = 0, pressed;
    unsigned char x = 0, y = 93;        /* Block position (anim strip)  */
    signed char dx = 2;
    unsigned char bpen = 3;             /* Block pen, cycles on bounce  */
    unsigned char prevx[2], prevy[2];   /* Block position per page      */
    unsigned char i;

    gfx_init ();                        /* 4bpp, page 0 viewed + drawn  */
    CLI ();

    gfx_setframerate (60);
    gfx_setpalette (mono_pal);

    for (;;) {
        joy = joy_read ();
        pressed = joy & (unsigned char) ~prev;
        prev = joy;

        if (pressed & JOY_BTN_A_MASK) {
            /* Don't change depth or touch buffers mid-swap */
            while (gfx_busy ()) {}

            if (bpp == 4) {
                bpp = 2;
                gfx_setbpp (2);
                /* From here on Mikey reads 40 bytes/line; the CPU
                ** owns the buffer. Prebuild the scene in both pages.
                */
                for (i = 0; i < 2; ++i) {
                    render2 (page[i]);
                    block2 (page[i], x, y, bpen);
                    prevx[i] = x;
                    prevy[i] = y;
                }
            } else {
                bpp = 4;
                gfx_setbpp (4);         /* Suzy output is valid again */
            }
        }
        if (pressed & JOY_UP_MASK) {
            gfx_flip ();                /* Honors the 2bpp flip offset */
        }

        /* Wait for the previous swap, then render into the back page */
        while (gfx_busy ()) {}

        if (bpp == 4) {
            render4 ();
        } else {
            /* Erase the block where this page last drew it, move,
            ** redraw - all by CPU; no Suzy in 2bpp
            */
            block2 (page[back], prevx[back], prevy[back], 0);
            x += dx;
            if (x == 0 || x >= GFX_XRES - 8) {
                dx = -dx;
                bpen = (bpen == 3) ? 1 : bpen + 1;
            }
            block2 (page[back], x, y, bpen);
            prevx[back] = x;
            prevy[back] = y;
        }

        /* Depth-independent: swap pages at the next VBL either way */
        gfx_updatedisplay ();
        back ^= 1;
    }
}
