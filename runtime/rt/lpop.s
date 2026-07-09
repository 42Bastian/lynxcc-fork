;
; Ullrich von Bassewitz, 29.12.1999
; Christian Krueger, 11-Mar-2017, added 65SC02 optimization
;
; CC65 runtime: long pop
;

        .export         popeax
        .import         incsp4
        .importzp       sp, sreg

popeax: ldy     #3
        lda     (sp),y
        sta     sreg+1
        dey
        lda     (sp),y
        sta     sreg
        dey
        lda     (sp),y
        tax
        lda     (sp)
        jmp     incsp4

