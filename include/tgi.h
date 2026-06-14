/*****************************************************************************/
/*                                                                           */
/*                                   tgi.h                                   */
/*                                                                           */
/*                  Lynx graphics interface (static library)                 */
/*                                                                           */
/*                                                                           */
/* Originally the Tiny Graphics Interface by Ullrich von Bassewitz,          */
/* (C) 2002-2013. This Lynx-only tree replaces the loadable-driver TGI       */
/* with a direct-call static library: there is exactly one display mode      */
/* (160x102x16) and all drawing is done by the Suzy sprite engine. See       */
/* LYNX_TGI_DESIGN.md. Geometric primitives, vector fonts, the driver        */
/* loader and the error model are gone (clean API break).                    */
/*                                                                           */
/* This software is provided 'as-is', without any expressed or implied       */
/* warranty. Use and redistribution are free for any purpose; see the        */
/* original cc65 license terms.                                              */
/*                                                                           */
/*****************************************************************************/



#ifndef _TGI_H
#define _TGI_H



/*****************************************************************************/
/*                                Definitions                                */
/*****************************************************************************/



/* Compile-time facts of the one and only display mode */
#define TGI_XRES        160
#define TGI_YRES        102
#define TGI_COLORCOUNT  16
#define TGI_PAGECOUNT   2
#define TGI_FONTWIDTH   8
#define TGI_FONTHEIGHT  8

/* The old query functions become zero-cost macros */
#define tgi_getxres()           TGI_XRES
#define tgi_getmaxx()           (TGI_XRES - 1)
#define tgi_getyres()           TGI_YRES
#define tgi_getmaxy()           (TGI_YRES - 1)
#define tgi_getcolorcount()     TGI_COLORCOUNT
#define tgi_getmaxcolor()       (TGI_COLORCOUNT - 1)
#define tgi_getpagecount()      TGI_PAGECOUNT

/* Font constants for use with tgi_setfont */
#define TGI_FONT_BITMAP         0       /* System 8x8 font            */
#define TGI_FONT_COMPACT        1       /* Transparent 5x5 font, 6px  */

/* Direction constants for use with tgi_settextstyle */
#define TGI_TEXT_HORIZONTAL     0
#define TGI_TEXT_VERTICAL       1



/*****************************************************************************/
/*                          Initialization, clearing                         */
/*****************************************************************************/



void tgi_init (void);
/* Initialize graphics: enable the VBL timer interrupt, set up the collision
** buffer, reset the display (4bpp, unflipped, page 0 viewed and drawn), load
** the default palette and select black (pen 0) as the drawing color, so
** tgi_init + tgi_clear yields a black screen. (The old loadable driver
** defaulted to white: code that drew after init without an explicit
** tgi_setcolor must now set its color first.) The hardware is fixed, so
** this cannot fail.
*/

void tgi_done (void);
/* End graphics mode. (There is no text mode on the Lynx; this only resets
** the internal mode flag.)
*/

void tgi_clear (void);
/* Clear the draw page in the CURRENT drawing color (tgi_setcolor). After
** tgi_init the drawing color is black, so init + clear gives a black
** screen. (The old driver always cleared with pen 0 regardless of color.)
*/

void __fastcall__ tgi_clearrows (unsigned char first, unsigned char count);
/* Clear rows [first, first+count) of the draw page in the current drawing
** color. Bands reaching below the screen are clipped; count 0 is a no-op.
*/



/*****************************************************************************/
/*                              Sprites, display                             */
/*****************************************************************************/



void __fastcall__ tgi_sprite (const void* sprite);
/* Draw a sprite control block (or SCB chain) into the draw page. Returns
** after the sprite engine has finished (synchronous). Note: an SCB pen-index
** palette of 8 bytes must not begin at address $xxFA - a hardware bug loses
** its last 2 bytes (pens C-F keep stale values). Align or pad the SCB.
** Do not interleave raw game-cart strobes with sprite calls: after a cart
** write, Suzy must not be accessed for 12 ticks.
*/

void tgi_flip (void);
/* Rotate the display by 180 degrees (left-handed mode). This is NOT a page
** flip; see tgi_updatedisplay for double buffering.
*/

void __fastcall__ tgi_setviewpage (unsigned char page);
/* Set the visible page (0 or 1). Best done during vertical blank; see
** tgi_updatedisplay.
*/

void __fastcall__ tgi_setdrawpage (unsigned char page);
/* Set the page (0 or 1) that drawing operations render into. */

unsigned char tgi_busy (void);
/* Return nonzero while a swap requested by tgi_updatedisplay is still
** pending. (Reports the pending swap request, not sprite-engine activity;
** all drawing is synchronous anyway.)
*/

void tgi_updatedisplay (void);
/* Request a draw/view page swap at the next VBL interrupt. */

unsigned char __fastcall__ tgi_setframerate (unsigned char rate);
/* Set the display refresh rate: 50, 60 or 75 (Hz). Returns 0 on success,
** nonzero for an invalid rate.
*/

void __fastcall__ tgi_setcollisiondetection (unsigned char active);
/* Enable or disable Suzy's sprite collision detection (default: off). When
** enabled, tgi_clear/tgi_clearrows also erase the (corresponding rows of
** the) collision buffer.
*/

void __fastcall__ tgi_setbpp (unsigned char bpp);
/* Select the display depth: 4 (default) or 2 bits per pixel. 2bpp is a
** CPU-rendered framebuffer mode: Mikey scans out 40 bytes/line (4080 bytes
** per page, upper 4080 bytes of each page free), but the sprite engine
** always renders 4bpp, so tgi_sprite/tgi_outtext output displays garbled
** in 2bpp (tgi_clear stays valid only for colors 0, 5, 10 and 15, which
** map to the 2bpp pens 0-3). The mode relies on a DISPCTL
** bit outside spec guarantees and is unverified on real hardware.
*/



/*****************************************************************************/
/*                              Color, palette                               */
/*****************************************************************************/



void __fastcall__ tgi_setcolor (unsigned char color);
/* Set the drawing pen (0-15). */

unsigned char tgi_getcolor (void);
/* Return the current drawing pen. */

void __fastcall__ tgi_setbgcolor (unsigned char color);
/* Set the background pen (0-15) used for text output. */

void __fastcall__ tgi_setpalette (const unsigned char* palette);
/* Set the palette: 32 bytes, 16 green bytes followed by 16 blue/red bytes. */

const unsigned char* tgi_getpalette (void);
/* Return the current palette (the readable hardware palette). */

const unsigned char* tgi_getdefpalette (void);
/* Return the default palette. */



/*****************************************************************************/
/*                                   Text                                    */
/*****************************************************************************/



void __fastcall__ tgi_gotoxy (int x, int y);
/* Set the text cursor for tgi_outtext. */

void __fastcall__ tgi_outtext (const char* s);
/* Output text at the text cursor position; at most 20 characters are drawn
** per call. The cursor is moved to the end of the text.
*/

void __fastcall__ tgi_outtextxy (int x, int y, const char* s);
/* Output text at the given position; see tgi_outtext. */

void __fastcall__ tgi_settextscale (unsigned width, unsigned height);
/* Set the text scaling. The factors are 8.8 fixed point ($100 = 1.0) and
** fully fractional: Suzy scales the text sprite natively in 8.8.
*/

void __fastcall__ tgi_settextstyle (unsigned width, unsigned height,
                                    unsigned char dir, unsigned char font);
/* Set scaling and direction for text output. The font argument is ignored;
** use tgi_setfont to choose a font.
*/

void __fastcall__ tgi_setfont (unsigned char font);
/* Select the active text font: one of the TGI_FONT_XXX constants.
** TGI_FONT_BITMAP is the system 8x8 font (the default); TGI_FONT_COMPACT is
** the 5x5 font with a transparent background, drawn in the current pen at a
** 6-px pitch. Linking tgi_setfont pulls in the compact font; programs that
** stay with the 8x8 font need not call it.
*/

void __fastcall__ tgi_settextdir (unsigned char dir);
/* Set the direction for text output: one of the TGI_TEXT_XXX constants.
** Affects only how the cursor advances; glyphs are not rotated.
*/

unsigned __fastcall__ tgi_gettextwidth (const char* s);
/* Width of s in pixels at the current text scale. */

unsigned __fastcall__ tgi_gettextheight (const char* s);
/* Height of text in pixels at the current text scale. */



/* End of tgi.h */
#endif
