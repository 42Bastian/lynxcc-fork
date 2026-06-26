;
; Karri Kaksonen, 2004
;
; Extracted and adapted from the lynx-160-102-16 TGI driver
; (libsrc/lynx/tgi/lynx-160-102-16.s) that Karri Kaksonen wrote for cc65 in
; 2004. This is original cc65 work and stays under the cc65 package license in
; the root LICENSE file, not the SDK's MPL-2.0. See doc/licenses.html.

;
; Lynx graphics: gfx_init.
;
; void gfx_init (void);
;
; gfx_init absorbs the old INSTALL+INIT driver entries and the kernel's
; gfx_init: it enables the VBL timer interrupt, sets up the collision
; buffer, resets the display to a known state (4bpp, unflipped, page 0
; viewed and drawn), loads the default palette and selects black (pen 0)
; as the drawing color, so that gfx_init + gfx_clear yields a black screen
; (design/LYNX_GFX_DESIGN.md sec. 2.8; the old driver defaulted to white).
; The hardware is fixed, so it cannot fail and returns void.
;
; Text style and the collision-detection setting are owned by their own
; modules and statically initialized to their defaults there; gfx_init
; does not reference them, so a program that never uses text never links
; the font (design/LYNX_GFX_DESIGN.md sec. 2.6).
;
; Note on re-init: gfx-page.s statically initializes its view-page shadow
; and swap state to the page-0 defaults. A program that swaps pages and
; then calls gfx_init again should not assume a pending swap survives the
; re-init.
;

        .include "lynx/lynx.inc"
        .include        "lynx/extzp.inc"
        .include        "gfx.inc"

        .import         gfx_drawindex
        .import         gfx_drawpage

        .export         _gfx_init
        .export         gfx_defpalette

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

_gfx_init:

; Enable interrupts for VBL. RMW on Mikey is legal (the spec's RMW ban
; applies to Suzy only); never touch B6 'reset timer done' here - it is
; a level signal that can stream interrupts (spec ch. 3.3).

        lda     #$80
        tsb     VTIMCTLA

; Set up the collision buffer; put the collision index just before the
; sprite data.

        lda     #<GFX_COLLBUF_ADDR
        sta     COLLBASL
        lda     #>GFX_COLLBUF_ADDR
        sta     COLLBASH
        lda     #$FF
        sta     COLLOFFL
        sta     COLLOFFH

; Known display state: 4bpp, unflipped, DMA on ($0D is the value the spec
; prescribes and crt0.s seeds). DISPCTL is write-only: update the shadow
; first, then store (spec ch. 3.1.3).

        lda     #$0D
        sta     __viddma
        sta     DISPCTL

; Clear a possibly left-over left-handed flip of the sprite coordinate
; system. Plain STA to SPRSYS via its shadow.

        lda     __sprsys
        and     #$F7
        sta     __sprsys
        sta     SPRSYS

; View and draw page 0.

        lda     #<GFX_PAGE0_ADDR
        sta     DISPADRL
        sta     gfx_drawpage
        lda     #>GFX_PAGE0_ADDR
        sta     DISPADRH
        sta     gfx_drawpage+1

; Load the default palette.

        ldy     #31
@L1:    lda     gfx_defpalette,y
        sta     GCOLMAP,y       ; $FDA0
        dey
        bpl     @L1

; Draw in black (pen 0) - gfx_clear fills with the draw color (sec. 2.8).

        stz     gfx_drawindex
        rts
