;
; Karri Kaksonen, 2004
;
; Extracted and adapted from the lynx-160-102-16 TGI driver
; (libsrc/lynx/tgi/lynx-160-102-16.s) that Karri Kaksonen wrote for cc65 in
; 2004. This is original cc65 work and stays under the cc65 package license in
; the root LICENSE file, not the SDK's MPL-2.0. See doc/licenses.html.

;
; Lynx graphics: palette.
;
; void __fastcall__ gfx_setpalette (const unsigned char* palette);
; void __fastcall__ gfx_setpalette16 (const unsigned short* palette);
; const unsigned char* gfx_getpalette (void);
;
; gfx_setpalette takes the 32-byte wire format: 16 green bytes, then 16
; blue/red bytes (GCOLMAP hardware layout). gfx_setpalette16 takes the
; condensed form the palette generator also emits - 16 packed 12-bit
; entries, one unsigned short per pen of the form 0x0GBR (green nibble in
; bits 8-11, blue in 4-7, red in 0-3) - and splits each into the two
; GCOLMAP halves as it loads it. Both describe the same colours; use
; whichever matches the array palgen gives you. GCOLMAP is readable, so
; gfx_getpalette returns the hardware palette itself. The built-in default
; palette and the calls that use it (gfx_setdefpalette, gfx_getdefpalette)
; live in gfx-defpalette.s so they only link when a program uses them.
;

        .include "lynx/lynx.inc"

        .importzp       ptr1

        .export         _gfx_setpalette
        .export         _gfx_setpalette16
        .export         _gfx_getpalette

.code

_gfx_setpalette:
        sta     ptr1
        stx     ptr1+1
        ldy     #31
@L1:    lda     (ptr1),y
        sta     GCOLMAP,y       ; $FDA0
        dey
        bpl     @L1
        rts

; Load the condensed 16-entry palette. Each unsigned short is little-endian
; 0x0GBR, so its low byte is the blue/red pair and its high byte is the
; green nibble. Pen i's low byte goes to GCOLMAP+16+i, its high byte to
; GCOLMAP+i. y walks the source (0..31), x the pen index (0..15).

_gfx_setpalette16:
        sta     ptr1
        stx     ptr1+1
        ldx     #0
        ldy     #0
@L1:    lda     (ptr1),y        ; low byte: blue/red
        sta     GCOLMAP+16,x    ; $FDB0 + pen
        iny
        lda     (ptr1),y        ; high byte: green nibble
        sta     GCOLMAP,x       ; $FDA0 + pen
        iny
        inx
        cpx     #16
        bne     @L1
        rts

_gfx_getpalette:
        lda     #<GCOLMAP
        ldx     #>GCOLMAP
        rts
