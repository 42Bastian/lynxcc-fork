/*****************************************************************************/
/*                                                                           */
/*                                   gfx.h                                   */
/*                                                                           */
/*                  Lynx graphics interface (static library)                 */
/*                                                                           */
/*                                                                           */
/* Originally the Tiny Graphics Interface by Ullrich von Bassewitz,          */
/* (C) 2002-2013. This Lynx-only tree replaces the loadable-driver TGI       */
/* with a direct-call static library: there is exactly one display mode      */
/* (160x102x16) and all drawing is done by the Suzy sprite engine. See       */
/* design/LYNX_GFX_DESIGN.md. Geometric primitives, vector fonts, the driver */
/* loader and the error model are gone (clean API break).                    */
/*                                                                           */
/* This software is provided 'as-is', without any expressed or implied       */
/* warranty. Use and redistribution are free for any purpose; see the        */
/* original cc65 license terms.                                              */
/*                                                                           */
/*****************************************************************************/



#ifndef _GFX_H
#define _GFX_H



/*****************************************************************************/
/*                                Definitions                                */
/*****************************************************************************/



/* Compile-time facts of the one and only display mode */
#define GFX_XRES        160
#define GFX_YRES        102
#define GFX_COLORCOUNT  16
#define GFX_PAGECOUNT   2

/* The old query functions become zero-cost macros */
#define gfx_getxres()           GFX_XRES
#define gfx_getmaxx()           (GFX_XRES - 1)
#define gfx_getyres()           GFX_YRES
#define gfx_getmaxy()           (GFX_YRES - 1)
#define gfx_getcolorcount()     GFX_COLORCOUNT
#define gfx_getmaxcolor()       (GFX_COLORCOUNT - 1)
#define gfx_getpagecount()      GFX_PAGECOUNT

/* Font constants for use with gfx_setfont */
#define GFX_FONT_BITMAP         0       /* System 8x8 font            */
#define GFX_FONT_COMPACT        1       /* Transparent 5x5 font, 6px  */
#define GFX_FONT_VARIABLE       2       /* Proportional caps, 1..5px  */

/* Direction constants for use with gfx_settextstyle */
#define GFX_TEXT_HORIZONTAL     0
#define GFX_TEXT_VERTICAL       1



/*****************************************************************************/
/*                          Initialization, clearing                         */
/*****************************************************************************/



void gfx_init (void);
/* Initialize graphics: enable the VBL timer interrupt, set up the collision
** buffer, reset the display (4bpp, unflipped, page 0 viewed and drawn), load
** the default palette and select black (pen 0) as the drawing color, so
** gfx_init + gfx_clear yields a black screen. (The old loadable driver
** defaulted to white: code that drew after init without an explicit
** gfx_setcolor must now set its color first.) The hardware is fixed, so
** this cannot fail.
*/

void gfx_clear (void);
/* Clear the draw page in the CURRENT drawing color (gfx_setcolor). After
** gfx_init the drawing color is black, so init + clear gives a black
** screen. (The old driver always cleared with pen 0 regardless of color.)
*/

void __fastcall__ gfx_clearrows (unsigned char first, unsigned char count);
/* Clear rows [first, first+count) of the draw page in the current drawing
** color. Bands reaching below the screen are clipped; count 0 is a no-op.
*/



/*****************************************************************************/
/*                              Sprites, display                             */
/*****************************************************************************/



void __fastcall__ gfx_sprite (const void* sprite);
/* Draw a sprite control block (or SCB chain) into the draw page. Returns
** after the sprite engine has finished (synchronous). Note: an SCB pen-index
** palette of 8 bytes must not begin at address $xxFA - a hardware bug loses
** its last 2 bytes (pens C-F keep stale values). Align or pad the SCB.
** Do not interleave raw game-cart strobes with sprite calls: after a cart
** write, Suzy must not be accessed for 12 ticks.
*/

void gfx_flip (void);
/* Rotate the display by 180 degrees (left-handed mode). This is NOT a page
** flip; see gfx_updatedisplay for double buffering.
*/

void __fastcall__ gfx_setviewpage (unsigned char page);
/* Set the visible page (0 or 1). Best done during vertical blank; see
** gfx_updatedisplay.
*/

void __fastcall__ gfx_setdrawpage (unsigned char page);
/* Set the page (0 or 1) that drawing operations render into. */

unsigned char gfx_busy (void);
/* Return nonzero while a swap requested by gfx_updatedisplay is still
** pending. (Reports the pending swap request, not sprite-engine activity;
** all drawing is synchronous anyway.)
*/

void gfx_updatedisplay (void);
/* Request a draw/view page swap at the next VBL interrupt. */

unsigned char __fastcall__ gfx_setframerate (unsigned char rate);
/* Set the display refresh rate: 50, 60 or 75 (Hz). Returns 0 on success,
** nonzero for an invalid rate.
*/

void __fastcall__ gfx_setcollisiondetection (unsigned char active);
/* Enable or disable Suzy's sprite collision detection (default: off). When
** enabled, gfx_clear/gfx_clearrows also erase the (corresponding rows of
** the) collision buffer.
*/

void __fastcall__ gfx_setbpp (unsigned char bpp);
/* Select the display depth: 4 (default) or 2 bits per pixel. 2bpp is a
** CPU-rendered framebuffer mode: Mikey scans out 40 bytes/line (4080 bytes
** per page, upper 4080 bytes of each page free), but the sprite engine
** always renders 4bpp, so gfx_sprite/gfx_outtext output displays garbled
** in 2bpp (gfx_clear stays valid only for colors 0, 5, 10 and 15, which
** map to the 2bpp pens 0-3). The mode relies on a DISPCTL
** bit outside spec guarantees and is unverified on real hardware.
*/



/*****************************************************************************/
/*                              Color, palette                               */
/*****************************************************************************/



void __fastcall__ gfx_setcolor (unsigned char color);
/* Set the drawing pen (0-15). */

unsigned char gfx_getcolor (void);
/* Return the current drawing pen. */

void __fastcall__ gfx_setbgcolor (unsigned char color);
/* Set the background pen (0-15) used for text output. */

void __fastcall__ gfx_setpalette (const unsigned char* palette);
/* Set the palette: 32 bytes, 16 green bytes followed by 16 blue/red bytes. */

const unsigned char* gfx_getpalette (void);
/* Return the current palette (the readable hardware palette). */

const unsigned char* gfx_getdefpalette (void);
/* Return the default palette. */



/*****************************************************************************/
/*                                   Text                                    */
/*****************************************************************************/



void __fastcall__ gfx_gotoxy (int x, int y);
/* Set the text cursor for gfx_outtext. */

void __fastcall__ gfx_outtext (const char* s);
/* Output text at the text cursor position; at most 20 characters are drawn
** per call. The cursor is moved to the end of the text.
*/

void __fastcall__ gfx_outtextxy (int x, int y, const char* s);
/* Output text at the given position; see gfx_outtext. */

void __fastcall__ gfx_settextscale (unsigned width, unsigned height);
/* Set the text scaling. The factors are 8.8 fixed point ($100 = 1.0) and
** fully fractional: Suzy scales the text sprite natively in 8.8.
*/

void __fastcall__ gfx_settextstyle (unsigned width, unsigned height,
                                    unsigned char dir, unsigned char font);
/* Set scaling and direction for text output. The font argument is ignored;
** use gfx_setfont to choose a font.
*/

void __fastcall__ gfx_setfont (unsigned char font);
/* Select the active text font: one of the GFX_FONT_XXX constants.
** GFX_FONT_BITMAP is the system 8x8 font (the default); GFX_FONT_COMPACT is
** the 5x5 font with a transparent background, drawn in the current pen at a
** 6-px pitch; GFX_FONT_VARIABLE is the proportional all-caps font (transparent
** background, current pen, glyphs 1..5 px wide with a 1-px gap, so gfx_outtext
** and gfx_gettextwidth space each character by its own width). Linking
** gfx_setfont pulls in the compact and proportional fonts; programs that stay
** with the 8x8 font need not call it.
*/

void __fastcall__ gfx_settextdir (unsigned char dir);
/* Set the direction for text output: one of the GFX_TEXT_XXX constants.
** Affects only how the cursor advances; glyphs are not rotated.
*/

unsigned __fastcall__ gfx_gettextwidth (const char* s);
/* Width of s in pixels at the current text scale. */

unsigned __fastcall__ gfx_gettextheight (const char* s);
/* Height of text in pixels at the current text scale. */



/* End of gfx.h */
#endif
