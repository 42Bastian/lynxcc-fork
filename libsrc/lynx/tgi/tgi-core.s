;
; Lynx static TGI: shared sprite-drawing core.
;
; This is the only module that every TGI-using program links. It owns the
; current drawing pen and the draw buffer address, and provides the one
; routine that talks to the Suzy sprite engine.
;
; Derived from the lynx-160-102-16 TGI driver by Karri Kaksonen, 2004.
;

        .include "lynx/lynx.inc"
        .include        "tgi-kernel.inc"

        .export         tgi_draw_sprite
        .export         tgi_drawindex
        .export         tgi_drawpage

; void __fastcall__ tgi_sprite (const void* sprite);
;
; The C entry point is a direct alias for the drawing core: the SCB
; pointer arrives in A/X exactly as tgi_draw_sprite expects it. It lives
; here (not in its own module, as design/LYNX_TGI_DESIGN.md 2.6 sketches)
; because an exported symbol cannot alias an import, and a trampoline
; would put a jmp back on the hot path; the core is linked by every TGI
; program anyway.

        .export         _tgi_sprite

.data

; Address of the current draw buffer. Page 0 by default; tgi-page.s
; changes it, tgi-init.s resets it. Statically initialized so that
; sprite-only programs need no page setup code at all.

tgi_drawpage:   .byte   <TGI_PAGE0_ADDR, >TGI_PAGE0_ADDR

.bss

tgi_drawindex:  .res    1       ; Current drawing pen (set white by tgi_init)

.code

;-----------------------------------------------------------------------------
; tgi_draw_sprite: Draw the SCB (chain) pointed to by A/X into the draw
; buffer. Synchronous: busy-waits (sleeping) until the sprite engine is done,
; so on return - and therefore on entry to every TGI function - the engine
; is provably idle. This is the library-wide invariant that makes unguarded
; Suzy SCB register access legal (spec ch. 3.1.1; design/LYNX_TGI_DESIGN.md sec. 5)
; and that the Suzy math helpers rely on (design/LYNX_CODEGEN_DESIGN.md sec. 2.6).
;
; Suzy registers are written with plain STA only - never RMW opcodes
; (spec ch. 3.1.2).

_tgi_sprite:
tgi_draw_sprite:
        sta     SCBNEXTL
        stx     SCBNEXTH
        lda     tgi_drawpage
        ldx     tgi_drawpage+1
        sta     VIDBASL
        stx     VIDBASH
        lda     #1
        sta     SPRGO
        stz     SDONEACK
@L0:    stz     CPUSLEEP        ; Sleep until Suzy is done
        bit     SPRSYS          ; A still #1 from SPRGO: tests bit 0 (busy)
        bne     @L0
        stz     SDONEACK
        rts
