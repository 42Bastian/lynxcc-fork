;
; Ullrich von Bassewitz, 26.10.2000
;
; CC65 runtime: Push value in a/x onto the stack
;

        .export         push0, pusha0, pushax
        .importzp       sp

        .macpack        cpu

push0:  lda     #0
pusha0: ldx     #0

; This function is used *a lot*, so don't call any subroutines here.
; Beware: The value in ax must not be changed by this function!
; Beware^2: The optimizer knows about the value of Y after the function
;           returns! Both code paths exit with Y = 0.

.proc   pushax

        pha                     ; (3)
        lda     sp              ; (6)
        sec                     ; (8)
        sbc     #2              ; (10)
        sta     sp              ; (13)
        bcs     @L1             ; (17)
        dec     sp+1            ; (+5)
@L1:    ldy     #1              ; (19)
        txa                     ; (21)
        sta     (sp),y          ; (27)
        pla                     ; (31)
        dey                     ; (33)
.if (.cpu .bitand ::CPU_ISET_65SC02)
        sta     (sp)            ; (38) one cycle less than sta (sp),y
        rts                     ; (44)
.else
        sta     (sp),y          ; (39)
        rts                     ; (45)
.endif

.endproc
