;
; Ullrich von Bassewitz, 06.08.1998
; Christian Krueger, 11-Mar-2017, added 65SC02 optimization
;
; CC65 runtime: xor on longs
;

        .export         tosxor0ax, tosxoreax
        .import         addysp1
        .importzp       sp, sreg, tmp1

tosxor0ax:
        stz     sreg
        stz     sreg+1

tosxoreax:
        eor     (sp)            ; byte 0
        ldy     #1
        sta     tmp1
        txa
        eor     (sp),y          ; byte 1
        tax
        iny
        lda     sreg
        eor     (sp),y          ; byte 2
        sta     sreg
        iny
        lda     sreg+1
        eor     (sp),y          ; byte 3
        sta     sreg+1

        lda     tmp1
        jmp     addysp1

