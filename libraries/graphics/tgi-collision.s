;
; Lynx static TGI: collision detection.
;
; void __fastcall__ tgi_setcollisiondetection (unsigned char active);
;
; Toggles Suzy's collision logic (SPRSYS no-collide bit, via the
; __sprsys shadow - SPRSYS reads back differently than written, spec
; ch. 3.1.3) and adjusts the tgi_clear sprite so that it erases the
; collision buffer exactly when collision detection is on. Default is
; off (crt0 seeds __sprsys = $24; the cls sprite's static initializers
; match).
;

        .include "lynx/lynx.inc"
        .include        "lynx/extzp.inc"

        .import         tgi_cls_sprite

        .export         _tgi_setcollisiondetection

.code

_tgi_setcollisiondetection:
        tay
        bne     @L0
        lda     #%00000001      ; Off: tgi_clear does not erase coll buffer
        sta     tgi_cls_sprite
        lda     #%00100000      ; SPRCOLL: no-collide
        sta     tgi_cls_sprite+2
        lda     __sprsys
        ora     #$20
        bra     @L1
@L0:    lda     #%00000000      ; On: tgi_clear erases the collision buffer
        sta     tgi_cls_sprite
        sta     tgi_cls_sprite+2
        lda     __sprsys
        and     #$DF
@L1:    sta     __sprsys
        sta     SPRSYS
        rts
