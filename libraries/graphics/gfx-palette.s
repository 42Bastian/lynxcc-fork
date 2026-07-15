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
; const unsigned char* gfx_getpalette (void);
;
; The palette is 32 bytes: 16 green bytes, then 16 blue/red bytes
; (GCOLMAP hardware layout). GCOLMAP is readable, so gfx_getpalette
; returns the hardware palette itself. The built-in default palette and
; the calls that use it (gfx_setdefpalette, gfx_getdefpalette) live in
; gfx-defpalette.s so they only link when a program uses them.
;

        .include "lynx/lynx.inc"

        .importzp       ptr1

        .export         _gfx_setpalette
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

_gfx_getpalette:
        lda     #<GCOLMAP
        ldx     #>GCOLMAP
        rts
