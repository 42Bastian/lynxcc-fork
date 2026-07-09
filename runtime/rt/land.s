;
; Ullrich von Bassewitz, 06.08.1998
; Christian Krueger, 11-Mar-2017, added 65SC02 optimization
;
; CC65 runtime: and on longs
;

        .export         tosand0ax, tosandeax
        .import         addysp1
        .importzp       sp, sreg, tmp1

                                   
tosand0ax:
        stz     sreg
        stz     sreg+1

tosandeax:
        and     (sp)            ; byte 0
        ldy     #1
        sta     tmp1
        txa
        and     (sp),y          ; byte 1
        tax
        iny
        lda     sreg
        and     (sp),y          ; byte 2
        sta     sreg
        iny
        lda     sreg+1
        and     (sp),y          ; byte 3
        sta     sreg+1

        lda     tmp1
        jmp     addysp1

