;
; Lynx static TGI: tgi_clear.
;
; void tgi_clear (void);
;
; Clears the draw page by drawing one screen-sized pen-0 sprite.
; tgi_cls_sprite is exported because tgi-collision.s rewrites its type
; and collision bytes when collision detection is toggled.
;
; Note: a pen-0 fill writes $00 bytes, which read back as pen 0 at any
; display depth, so tgi_clear stays valid even in 2bpp mode
; (LYNX_TGI_DESIGN.md sec. 2.7).
;

        .include        "lynx.inc"
        .include        "tgi-kernel.inc"

        .import         tgi_draw_sprite

        .export         _tgi_clear
        .export         tgi_cls_sprite

.rodata

; A 1x1 pixel, 1bpp source bitmap, hardware-scaled to cover the screen.

pixel_bitmap:
        .byte   3, %10000100, %00000000, $0

.data

cls_coll:
        .byte   0
tgi_cls_sprite:
        .byte   %00000001               ; SPRCTL0 (collision off default)
        .byte   %00010000               ; SPRCTL1
        .byte   %00100000               ; SPRCOLL: no-collide (default)
        .addr   0, pixel_bitmap
        .word   0                       ; x
        .word   0                       ; y
        .word   $a000                   ; sx = 160.0
        .word   $6600                   ; sy = 102.0
        .byte   $00                     ; pen 0

.code

_tgi_clear:
        lda     #<tgi_cls_sprite
        ldx     #>tgi_cls_sprite
        jmp     tgi_draw_sprite
