;
; Lynx static TGI: pages, flipping, double buffering, display depth.
;
; void __fastcall__ tgi_setviewpage (unsigned char page);
; void __fastcall__ tgi_setdrawpage (unsigned char page);
; void tgi_flip (void);
; unsigned char tgi_busy (void);
; void tgi_updatedisplay (void);
; void __fastcall__ tgi_setbpp (unsigned char bpp);
;
; The VBL page-swap handler lives here as an .interruptor, so it enters
; the interrupt chain exactly when a program links this module - i.e.
; when it actually uses pages/double buffering. tgi_init does not
; reference this module (design/LYNX_TGI_DESIGN.md sec. 2.4).
;
; tgi_flip is a 180-degree screen rotation (left-handed mode), not a page
; flip. tgi_busy reports a *pending swap request*, not sprite-engine
; business (pre-existing semantics, kept).
;
; tgi_setbpp selects how Mikey's display DMA interprets the buffer via
; DISPCTL B2: 4 (default) or 2 bits/pixel. 2bpp is a CPU-rendered frame-
; buffer mode: Suzy always renders 4bpp, so sprite/text output scans out
; garbled in 2bpp - caller responsibility, by design unguarded. The mode
; uses a DISPCTL bit outside spec guarantees and is unverified on
; hardware (design/LYNX_TGI_DESIGN.md sec. 2.7).
;
; DISPCTL and SPRSYS are write-only/read-back-differently: all writes go
; through the __viddma/__sprsys shadows first (spec ch. 3.1.3).
;

        .include "lynx/lynx.inc"
        .include        "../extzp.inc"
        .include        "tgi-kernel.inc"

        .import         tgi_drawpage

        .export         _tgi_setviewpage
        .export         _tgi_setdrawpage
        .export         _tgi_flip
        .export         _tgi_busy
        .export         _tgi_updatedisplay
        .export         _tgi_setbpp

        .interruptor    tgi_vbl_irq     ; Page-swap VBL handler

.data

; View page address shadow (DISPADR is write-only). Statically page 0,
; matching tgi_init's display reset.

viewpage:       .byte   <TGI_PAGE0_ADDR, >TGI_PAGE0_ADDR

drawpagenum:    .byte   0       ; Number (0/1) of the current draw page
swaprequest:    .byte   0       ; Flag: swap pages at next VBL

.code

;-----------------------------------------------------------------------------
; tgi_setviewpage: Set the visible page (0/1). Calling this in the middle of
; the screen update tears; prefer tgi_updatedisplay for clean swaps.

_tgi_setviewpage:
        cmp     #1
        beq     @L1
        ldy     #<TGI_PAGE0_ADDR
        ldx     #>TGI_PAGE0_ADDR
        bra     @L2
@L1:    ldy     #<TGI_PAGE1_ADDR
        ldx     #>TGI_PAGE1_ADDR
@L2:    sty     viewpage
        stx     viewpage+1

; Fall through: write DISPADR for the current view page, honoring the
; flip state and display depth (a flipped display scans backward from
; the end of the buffer, whose offset depends on bits/pixel).

set_dispadr:
        ldy     viewpage
        ldx     viewpage+1
        lda     __viddma
        and     #2              ; Flipped?
        beq     @store
        lda     __viddma
        and     #4              ; 4bpp?
        beq     @bpp2
        clc
        tya
        adc     #<TGI_FLIPOFFS_4BPP
        tay
        txa
        adc     #>TGI_FLIPOFFS_4BPP
        tax
        bra     @store
@bpp2:  clc
        tya
        adc     #<TGI_FLIPOFFS_2BPP
        tay
        txa
        adc     #>TGI_FLIPOFFS_2BPP
        tax
@store: sty     DISPADRL        ; $FD94
        stx     DISPADRH        ; $FD95
        rts

;-----------------------------------------------------------------------------
; tgi_setdrawpage: Set the page (0/1) that sprites are rendered into.

_tgi_setdrawpage:
        cmp     #1
        beq     @L1
        lda     #<TGI_PAGE0_ADDR
        ldx     #>TGI_PAGE0_ADDR
        bra     @L2
@L1:    lda     #<TGI_PAGE1_ADDR
        ldx     #>TGI_PAGE1_ADDR
@L2:    sta     tgi_drawpage
        stx     tgi_drawpage+1
        rts

;-----------------------------------------------------------------------------
; tgi_flip: Rotate the display 180 degrees (left-handed mode). Toggles the
; sprite coordinate flip and the display DMA direction, then re-issues the
; view page address.

_tgi_flip:
        lda     __sprsys
        eor     #8
        sta     __sprsys
        sta     SPRSYS
        lda     __viddma
        eor     #2
        sta     __viddma
        sta     DISPCTL
        bra     set_dispadr

;-----------------------------------------------------------------------------
; tgi_setbpp: Select the display depth, 4 (default) or 2 bits/pixel.

_tgi_setbpp:
        cmp     #2
        beq     @L1
        lda     __viddma        ; 4bpp: set DISPCTL B2
        ora     #$04
        bra     @L2
@L1:    lda     __viddma        ; 2bpp: clear DISPCTL B2
        and     #$FB
@L2:    sta     __viddma
        sta     DISPCTL
        bra     set_dispadr     ; Re-issue DISPADR (flip offset depends on bpp)

;-----------------------------------------------------------------------------
; tgi_busy: Return the pending-swap flag.

_tgi_busy:
        lda     swaprequest
        ldx     #0
        rts

;-----------------------------------------------------------------------------
; tgi_updatedisplay: Request a draw/view page swap at the next VBL.

_tgi_updatedisplay:
        lda     #1
        sta     swaprequest
        rts

;-----------------------------------------------------------------------------
; VBL interruptor: perform a pending page swap. Linked (via the interruptor
; table) only when this module is.

tgi_vbl_irq:
        lda     INTSET          ; Poll all pending interrupts
        and     #VBL_INTERRUPT
        beq     @L0             ; Exit if not a VBL interrupt

        lda     swaprequest
        beq     @L0
        lda     drawpagenum     ; View the page just drawn
        jsr     _tgi_setviewpage
        lda     drawpagenum     ; Draw into the other one
        eor     #1
        sta     drawpagenum
        jsr     _tgi_setdrawpage
        stz     swaprequest
@L0:    clc                     ; Never claim the interrupt
        rts
