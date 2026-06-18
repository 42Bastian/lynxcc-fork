;
; Lynx static TGI: tgi_init.
;
; void tgi_init (void);
;
; tgi_init absorbs the old INSTALL+INIT driver entries and the kernel's
; tgi_init: it enables the VBL timer interrupt, sets up the collision
; buffer, resets the display to a known state (4bpp, unflipped, page 0
; viewed and drawn), loads the default palette and selects black (pen 0)
; as the drawing color, so that tgi_init + tgi_clear yields a black screen
; (design/LYNX_TGI_DESIGN.md sec. 2.8; the old driver defaulted to white).
; The hardware is fixed, so it cannot fail and returns void.
;
; Text style and the collision-detection setting are owned by their own
; modules and statically initialized to their defaults there; tgi_init
; does not reference them, so a program that never uses text never links
; the font (design/LYNX_TGI_DESIGN.md sec. 2.6).
;
; Note on re-init: tgi-page.s statically initializes its view-page shadow
; and swap state to the page-0 defaults. A program that swaps pages and
; then calls tgi_init again should not assume a pending swap survives the
; re-init.
;

        .include        "lynx.inc"
        .include        "../extzp.inc"
        .include        "tgi-kernel.inc"

        .import         tgi_drawindex
        .import         tgi_drawpage

        .export         _tgi_init
        .export         tgi_defpalette

.rodata

; Default palette, green bytes first, then blue/red (GCOLMAP layout).

tgi_defpalette: .byte   >$011
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

_tgi_init:

; Enable interrupts for VBL. RMW on Mikey is legal (the spec's RMW ban
; applies to Suzy only); never touch B6 'reset timer done' here - it is
; a level signal that can stream interrupts (spec ch. 3.3).

        lda     #$80
        tsb     VTIMCTLA

; Set up the collision buffer; put the collision index just before the
; sprite data.

        lda     #<TGI_COLLBUF_ADDR
        sta     COLLBASL
        lda     #>TGI_COLLBUF_ADDR
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

        lda     #<TGI_PAGE0_ADDR
        sta     DISPADRL
        sta     tgi_drawpage
        lda     #>TGI_PAGE0_ADDR
        sta     DISPADRH
        sta     tgi_drawpage+1

; Load the default palette.

        ldy     #31
@L1:    lda     tgi_defpalette,y
        sta     GCOLMAP,y       ; $FDA0
        dey
        bpl     @L1

; Draw in black (pen 0) - tgi_clear fills with the draw color (sec. 2.8).

        stz     tgi_drawindex
        rts
