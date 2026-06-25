/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in this directory.
*/

/*
** Packed vs literal sprite confirmation for cc65 (Atari Lynx).
**
** Suzy sprites carry their pixel data in one of two encodings, chosen by
** the LITERAL bit (SPRCTL1 bit 7):
**
**   - LITERAL: raw pen-index pixels back-to-back, padded to a byte per
**     scan line. Every hand-built sprite in this tree uses this form.
**   - PACKED: a per-line bit-stream of RLE packets - each headed by a
**     1-bit literal/run flag plus a 4-bit count - terminated by a 00000
**     marker. This is the encoding sprpck-style tools emit and the one
**     the hardware spec documents the pad-byte bug against.
**
** This sample proves the two encodings are interchangeable. For each of
** the four Suzy depths (1/2/3/4 bpp) it draws the SAME 16x16 source
** image twice at 1x scale: the LITERAL copy on the left as the control,
** and a PACKED copy on the right built at runtime by pack_line() from
** the identical source. The source is deliberately adversarial - a
** vertical rainbow in the top half (every pen value, all adjacent
** pixels distinct: exercises literal packets and the value->pen map) and
** solid horizontal bands in the bottom half (exercises RLE runs). If
** packing is correct the left and right blocks are pixel-identical; any
** bit-order, count or pad error in the packed path shows as a visible
** difference between the columns.
**
** Both copies use a TYPE_NORMAL sprite with an identity penpal, so pixel
** value v draws pen v and value 0 maps to pen 0 (transparent). The
** literal copy carries the per-line pad byte required by Suzy's
** last-pixel bug; the packed copy's 00000 terminator supplies the same
** trailing slack, so neither column loses its rightmost pixel. See
** design/LYNX_SPRITE_PADBYTE_DESIGN.md sec. 1-2 and 5.
**
** Build:  cl65 -Ors -o packtest.lnx packtest.c
*/

#include <lynx/lynx.h>
#include <lynx/tgi.h>
#include <6502.h>
#include <string.h>

#define SIDE    16              /* source sprite is SIDE x SIDE pixels   */

/* The SCBs below carry an identity penpal {0x01,0x23,...,0xEF}: value 2k
** -> pen 2k (high nibble of byte k), value 2k+1 -> pen 2k+1 (low nibble).
** Suzy reads only the leading 2^bpp nibbles, so one table serves every
** depth, and value 0 always maps to pen 0 (transparent).
**
** 16-entry rainbow palette so each pen value is visually distinct.
** Lynx format: 16 green nibbles, then 16 (blue<<4|red) bytes. pen 0 is
** black (the transparent background).
*/
static const unsigned char rainbow[32] = {
    /* green nibble per pen 0..15 */
    0x0, 0x0, 0x8, 0xF, 0xF, 0xF, 0xF, 0xF,
    0x8, 0x0, 0x0, 0x0, 0x8, 0x8, 0xF, 0xA,
    /* blue<<4 | red per pen 0..15 */
    0x00, 0x0F, 0x0F, 0x0F, 0x08, 0x00, 0x80, 0xF0,
    0xF0, 0xF0, 0xF8, 0xFF, 0xCF, 0x88, 0xFF, 0xF4
};

/* Per-depth encoded sprite data, built at startup. Generous fixed size:
** worst case is a fully-literal 4bpp line (~11 bytes) over 16 lines plus
** offsets and the end marker.
*/
static unsigned char litdata[4][256];
static unsigned char packdata[4][256];

static SCB_REHV_PAL lit = {
    BPP_4 | TYPE_NORMAL, LITERAL | REHV, NO_COLLIDE,
    0, 0, 0, 0, 0x0100, 0x0100,         /* 1x scale: 1 source px = 1 screen px */
    { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF }
};
static SCB_REHV_PAL pak = {
    BPP_4 | TYPE_NORMAL, PACKED | REHV, NO_COLLIDE,
    0, 0, 0, 0, 0x0100, 0x0100,
    { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF }
};

/* sprctl0 depth selector per bpp index 0..3 (1/2/3/4 bpp). */
static const unsigned char bppctl[4] = { BPP_1, BPP_2, BPP_3, BPP_4 };

/* ---- MSB-first bit writer over a caller buffer ---- */
static unsigned char *bw_buf;
static unsigned int   bw_byte;
static unsigned char  bw_bit;

static void bw_init (unsigned char *b)
{
    bw_buf = b; bw_byte = 0; bw_bit = 0; b[0] = 0;
}

static void bw_put (unsigned int val, unsigned char n)
{
    signed char i;
    for (i = (signed char)(n - 1); i >= 0; --i) {
        if ((val >> i) & 1)
            bw_buf[bw_byte] |= (unsigned char)(0x80 >> bw_bit);
        if (++bw_bit == 8) {
            bw_bit = 0;
            bw_buf[++bw_byte] = 0;
        }
    }
}

static unsigned int bw_bytes (void)
{
    return bw_byte + (bw_bit ? 1 : 0);
}

/* Source pixel value at (x,y) for a given depth: vertical rainbow over
** the top half (stresses literal packets / the value->pen map), solid
** horizontal bands over the bottom half (stresses RLE runs).
*/
static unsigned char src_px (unsigned char x, unsigned char y, unsigned char nval)
{
    return (y < SIDE / 2) ? (unsigned char)(x % nval)
                          : (unsigned char)(y % nval);
}

/* Encode one packed line of SIDE pixels at depth bpp into out (after its
** offset byte). Returns the data byte count (excludes offset). Mirrors
** the host-validated encoder in the design notes.
*/
static unsigned int pack_line (const unsigned char *px, unsigned char bpp,
                               unsigned char *out)
{
    unsigned char i = 0, run, j, lit_n, k;
    bw_init (out);
    while (i < SIDE) {
        run = 1;
        while (i + run < SIDE && run < 16 && px[i + run] == px[i]) ++run;
        if (run >= 2) {                         /* RLE: 0, count-1, pixel    */
            bw_put (0, 1);
            bw_put ((unsigned int)(run - 1), 4);
            bw_put (px[i], bpp);
            i += run;
        } else {                                /* literal: 1, count-1, px[] */
            j = i; lit_n = 0;
            while (j < SIDE && lit_n < 16) {
                if (j + 1 < SIDE && px[j + 1] == px[j]) break;
                ++j; ++lit_n;
            }
            bw_put (1, 1);
            bw_put ((unsigned int)(lit_n - 1), 4);
            for (k = 0; k < lit_n; ++k) bw_put (px[i + k], bpp);
            i += lit_n;
        }
    }
    bw_put (0, 5);                              /* 00000 end-of-line marker  */
    return bw_bytes ();
}

/* Encode one literal line: SIDE pixels packed MSB-first, then one pen-0
** pad byte (0x00) for the last-pixel bug. Returns data byte count
** (excludes offset, includes pad).
*/
static unsigned int lit_line (const unsigned char *px, unsigned char bpp,
                              unsigned char *out)
{
    unsigned char k;
    bw_init (out);
    for (k = 0; k < SIDE; ++k) bw_put (px[k], bpp);
    /* SIDE*bpp is a multiple of 8 for all depths here, so the stream is
    ** already byte-aligned; add the trailing pen-0 pad byte. */
    out[bw_bytes ()] = 0x00;
    return bw_bytes () + 1;
}

/* Build both encodings for one depth into litdata[b]/packdata[b]. */
static void build_depth (unsigned char b)
{
    unsigned char line[SIDE];
    unsigned char nval = (unsigned char)(1u << (b + 1));   /* 2,4,8,16      */
    unsigned char y, x;
    unsigned int lp = 0, pp = 0, n;

    for (y = 0; y < SIDE; ++y) {
        for (x = 0; x < SIDE; ++x) line[x] = src_px (x, y, nval);

        n = lit_line (line, (unsigned char)(b + 1), &litdata[b][lp + 1]);
        litdata[b][lp] = (unsigned char)(1 + n);           /* offset byte   */
        lp += 1 + n;

        n = pack_line (line, (unsigned char)(b + 1), &packdata[b][pp + 1]);
        packdata[b][pp] = (unsigned char)(1 + n);
        pp += 1 + n;
    }
    litdata[b][lp] = 0;                                     /* end of sprite */
    packdata[b][pp] = 0;
}

static void draw (void)
{
    unsigned char b;
    signed int    y;

    tgi_setcolor (0);
    tgi_clear ();

    tgi_setcolor (1);
    tgi_outtextxy (40, 1, "LITERAL");
    tgi_outtextxy (100, 1, "PACKED");

    for (b = 0; b < 4; ++b) {
        y = 12 + b * 22;

        lit.sprctl0 = (unsigned char)(bppctl[b] | TYPE_NORMAL);
        lit.data    = litdata[b];
        lit.hpos    = 44; lit.vpos = y;

        pak.sprctl0 = (unsigned char)(bppctl[b] | TYPE_NORMAL);
        pak.data    = packdata[b];
        pak.hpos    = 104; pak.vpos = y;

        tgi_sprite (&lit);
        tgi_sprite (&pak);

        tgi_setcolor (14);
        switch (b) {
            case 0: tgi_outtextxy (4, y + 4, "1BPP"); break;
            case 1: tgi_outtextxy (4, y + 4, "2BPP"); break;
            case 2: tgi_outtextxy (4, y + 4, "3BPP"); break;
            case 3: tgi_outtextxy (4, y + 4, "4BPP"); break;
        }
    }
}

void main (void)
{
    unsigned char b;

    tgi_init ();
    CLI ();
    tgi_setframerate (60);
    tgi_setpalette (rainbow);

    for (b = 0; b < 4; ++b) build_depth (b);

    for (;;) {
        while (tgi_busy ()) {}          /* let the pending swap finish first, */
        draw ();                        /* so we never draw the visible page  */
        tgi_updatedisplay ();           /* (avoids tearing/flicker)           */
    }
}
