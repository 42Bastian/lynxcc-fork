/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in this directory.
*/

/*
** sprpck input formats: BMP and SPS (Atari Lynx).
**
** Where spritetest.c packs sprites offline with sp65, this sample does the same
** with sprpck - the vendored Lynx sprite packer (tools/extern/sprpck) - to show
** its two most distinctive input formats:
**
**   * logo.bmp   a Windows BMP (indexed, uncompressed). The Makefile runs
**                  sprpck -t6 -s4 logo.bmp logo.spr
**                to pack it as 4-bpp Lynx sprite data. sp65 reads only PCX, so
**                sprpck is the in-tree way to convert a BMP.
**
**   * icon.sps   an SPS file: plain ASCII, one hex digit per pixel, a space for
**                pen 0. The star in icon.sps is literally typed out as text and
**                packed with
**                  sprpck -t2 -i016016 -s4 icon.sps icon.spr
**                (-i gives the width/height an SPS has no header for).
**
** sprpck emits the packed Suzy sprite-data block as a raw binary .spr; both are
** pulled in verbatim by sprpckdata.s (.incbin) and reach C as extern arrays.
** They are drawn side by side under their format labels with one shared 16-pen
** palette. sprpck packs a BMP by compacting its palette to a dense pen range
** (logo.bmp's used colours become pens 1-3) and writes the matching Lynx palette
** to logo.pal; an SPS keeps its pen digits verbatim (the star is pens 6 and 8).
** So the crystal is pens 1-3, the star pens 6 and 8, and pen 0 is the
** transparent background of a TYPE_NORMAL sprite. sprpck's default output is
** PACKED, so the SCB carries the PACKED (not LITERAL) SPRCTL1 bit.
**
** Build:  cl65 -Ors -o sprpcktest.lnx sprpcktest.c sprpckdata.s
**         (after sprpck has generated logo.spr and icon.spr). See
**         doc/sprpck.html and doc/samples.html.
*/

#include <lynx/lynx.h>
#include <lynx/gfx.h>
#include <6502.h>

/* Packed sprite data, embedded by sprpckdata.s via .incbin. */
extern const unsigned char logo_data[];   /* from logo.bmp,  40x26 */
extern const unsigned char icon_data[];   /* from icon.sps,  16x16 */

/* Shared 16-pen palette. Lynx format: 16 green nibbles, then 16 (blue<<4|red)
** bytes. pen 0 transparent; 1-3 = blue crystal (BMP, matching logo.pal), 6 gold
** + 8 white (SPS star).
*/
static const unsigned char palette[32] = {
    /* green nibble per pen 0..15 (pen 15 = white, used for text) */
    0x0, 0x2, 0x6, 0xD, 0x0, 0x0, 0xD, 0x0,
    0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xF,
    /* (blue<<4 | red) per pen 0..15 */
    0x00, 0x71, 0xF3, 0xFA, 0x00, 0x00, 0x2F, 0x00,
    0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF
};

/* 4-bpp packed sprite, identity penpal (pixel value v -> pen v, 0 transparent),
** no stretch/tilt. data/size/pos are filled in per draw.
*/
static SCB_REHV_PAL scb = {
    BPP_4 | TYPE_NORMAL, PACKED | REHV, NO_COLLIDE,
    0, 0, 0, 0, 0x0100, 0x0100,
    { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF }
};

static void show (const unsigned char *data, signed int x, signed int y,
                  unsigned int scale)
{
    scb.data  = (unsigned char *)data;
    scb.hpos  = x;
    scb.vpos  = y;
    scb.hsize = scb.vsize = scale;
    gfx_sprite (&scb);
}

static void draw (void)
{
    gfx_setcolor (0);
    gfx_clear ();

    gfx_setcolor (15);
    gfx_outtextxy (56, 6, "sprpck");

    gfx_outtextxy (28, 24, "BMP");            /* over the crystal */
    gfx_outtextxy (112, 24, "SPS");           /* over the star    */

    show (logo_data,  4, 40, 0x0200);         /* 40x26 BMP at 2x  */
    show (icon_data, 104, 40, 0x0300);        /* 16x16 SPS at 3x  */
}

void main (void)
{
    gfx_init ();
    CLI ();
    gfx_setframerate (60);
    gfx_setpalette (palette);

    for (;;) {
        while (gfx_busy ()) {}
        draw ();
        gfx_updatedisplay ();
    }
}
