;
; Karri Kaksonen, 2004
;
; Extracted and adapted from the lynx-160-102-16 TGI driver
; (libsrc/lynx/tgi/lynx-160-102-16.s) that Karri Kaksonen wrote for cc65 in
; 2004. This is original cc65 work and stays under the cc65 package license in
; the root LICENSE file, not the SDK's MPL-2.0. See doc/licenses.html.

;
; Lynx graphics: the built-in default palette.
;
; void gfx_setdefpalette (void);
; const unsigned char* gfx_getdefpalette (void);
;
; The 32-byte default palette table and the two calls that use it live in
; their own module so a program that installs its own palette never links
; the table. gfx_init no longer loads it - it clears the hardware palette to
; black - so a program that wants the historical 16-colour default calls
; gfx_setdefpalette after gfx_init (see design/LYNX_GFX_DESIGN.md sec. 2.8).
;
; The palette is 32 bytes: 16 green bytes, then 16 blue/red bytes (blue in
; the high nibble, red in the low nibble), matching Mikey's GCOLMAP layout.
;

        .include "lynx/lynx.inc"

        .export         _gfx_setdefpalette
        .export         _gfx_getdefpalette

.rodata

; Default palette, green bytes first, then blue/red (GCOLMAP layout).

gfx_defpalette: .byte   >$011
                .byte   >$34d
                .byte   >$9af
                .byte   >$9b8
                .byte   >$777
                .byte   >$335
                .byte   >$448
                .byte   >$75e
                .byte   >$d5f
                .byte   >$c53
                .byte   >$822
                .byte   >$223
                .byte   >$484
                .byte   >$8e5
                .byte   >$cf5
                .byte   >$fff
                .byte   <$011
                .byte   <$34d
                .byte   <$9af
                .byte   <$9b8
                .byte   <$777
                .byte   <$335
                .byte   <$448
                .byte   <$75e
                .byte   <$d5f
                .byte   <$c53
                .byte   <$822
                .byte   <$223
                .byte   <$484
                .byte   <$8e5
                .byte   <$cf5
                .byte   <$fff

.code

; Load the built-in default palette into the hardware palette (GCOLMAP).

_gfx_setdefpalette:
        ldy     #31
@L1:    lda     gfx_defpalette,y
        sta     GCOLMAP,y       ; $FDA0
        dey
        bpl     @L1
        rts

; Return a pointer to the built-in default palette table.

_gfx_getdefpalette:
        lda     #<gfx_defpalette
        ldx     #>gfx_defpalette
        rts
