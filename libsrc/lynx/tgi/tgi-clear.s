;
; Lynx static TGI: tgi_clear, tgi_clearrows.
;
; void tgi_clear (void);
; void __fastcall__ tgi_clearrows (unsigned char first, unsigned char count);
;
; Clears the whole draw page, or the rows [first, first+count), by drawing
; one hardware-scaled pen-fill sprite. Both fill with the CURRENT draw
; color (tgi_setcolor), not a hard-wired pen (design/LYNX_TGI_DESIGN.md sec. 2.8).
; The color is written into both nibbles of the 1bpp pen table so the
; result does not depend on which pixel index the stretched source pixel
; carries.
;
; tgi_cls_sprite is exported because tgi-collision.s rewrites its type
; and collision bytes when collision detection is toggled; since both
; entry points share the sprite, a partial clear also erases the matching
; rows of the collision buffer when collision detection is on.
;
; Bands reaching below the screen are clipped by Suzy; count == 0 is an
; explicit no-op (Suzy's behavior at sy == 0 is not trusted).
;
; Note: at 2bpp display depth (sec. 2.7) a color-c clear writes bytes $cc,
; which scan out uniformly only for c in {0, 5, 10, 15} (-> 2bpp pens
; 0-3). A pen-0 fill writes $00 bytes and stays valid at any depth.
;

        .include        "lynx.inc"
        .include        "tgi-kernel.inc"

        .import         tgi_draw_sprite
        .import         tgi_drawindex
        .import         popa

        .export         _tgi_clear
        .export         _tgi_clearrows
        .export         tgi_cls_sprite

.rodata

; A 1x1 pixel, 1bpp source bitmap, hardware-scaled to cover the band.

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
cls_y:  .word   0                       ; y (low byte set per call)
        .word   $a000                   ; sx = 160.0
cls_sy: .word   $6600                   ; sy (8.8, integer part per call)
cls_pen:
        .byte   $00                     ; pen, both nibbles set per call

.code

; void __fastcall__ tgi_clearrows (unsigned char first, unsigned char count);

_tgi_clearrows:
        tax                             ; X = count
        jsr     popa                    ; A = first row
        bra     common

; void tgi_clear (void);

_tgi_clear:
        lda     #0                      ; first row 0
        ldx     #102                    ; full height

common: sta     cls_y                   ; y = first (high byte stays 0)
        txa
        beq     done                    ; count 0: draw nothing
        stx     cls_sy+1                ; sy = count.0 (8.8)
        stz     cls_sy
        lda     tgi_drawindex           ; pen byte = (c << 4) | c
        asl     a                       ; (c is 0..15, so its high nibble is
        asl     a                       ; 0 and the low nibble survives the
        asl     a                       ; OR with the unshifted variable)
        asl     a
        ora     tgi_drawindex
        sta     cls_pen
        lda     #<tgi_cls_sprite
        ldx     #>tgi_cls_sprite
        jmp     tgi_draw_sprite

done:   rts
