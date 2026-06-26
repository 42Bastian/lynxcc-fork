;
; Karri Kaksonen, 2004
;
; Extracted and adapted from the lynx-160-102-16 TGI driver
; (libsrc/lynx/tgi/lynx-160-102-16.s) that Karri Kaksonen wrote for cc65 in
; 2004. This is original cc65 work and stays under the cc65 package license in
; the root LICENSE file, not the SDK's MPL-2.0. See doc/licenses.html.

;
; Lynx graphics: shared sprite-drawing core.
;
; This is the only module that every Lynx graphics program links. It owns the
; current drawing pen and the draw buffer address, and provides the one
; routine that talks to the Suzy sprite engine.
;

        .include "lynx/lynx.inc"
        .include        "gfx.inc"

        .export         gfx_draw_sprite
        .export         gfx_drawindex
        .export         gfx_drawpage

; void __fastcall__ gfx_sprite (const void* sprite);
;
; The C entry point is a direct alias for the drawing core: the SCB
; pointer arrives in A/X exactly as gfx_draw_sprite expects it. It lives
; here (not in its own module, as design/LYNX_GFX_DESIGN.md 2.6 sketches)
; because an exported symbol cannot alias an import, and a trampoline
; would put a jmp back on the hot path; the core is linked by every Lynx
; graphics program anyway.

        .export         _gfx_sprite

.data

; Address of the current draw buffer. Page 0 by default; gfx-page.s
; changes it, gfx-init.s resets it. Statically initialized so that
; sprite-only programs need no page setup code at all.

gfx_drawpage:   .byte   <GFX_PAGE0_ADDR, >GFX_PAGE0_ADDR

.bss

gfx_drawindex:  .res    1       ; Current drawing pen (set white by gfx_init)

.code

;-----------------------------------------------------------------------------
; gfx_draw_sprite: Draw the SCB (chain) pointed to by A/X into the draw
; buffer. Synchronous: busy-waits (sleeping) until the sprite engine is done,
; so on return - and therefore on entry to every Lynx graphics function - the engine
; is provably idle. This is the library-wide invariant that makes unguarded
; Suzy SCB register access legal (spec ch. 3.1.1; design/LYNX_GFX_DESIGN.md sec. 5)
; and that the Suzy math helpers rely on (design/LYNX_CODEGEN_DESIGN.md sec. 2.6).
;
; Suzy registers are written with plain STA only - never RMW opcodes
; (spec ch. 3.1.2).

_gfx_sprite:
gfx_draw_sprite:
        sta     SCBNEXTL
        stx     SCBNEXTH
        lda     gfx_drawpage
        ldx     gfx_drawpage+1
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
