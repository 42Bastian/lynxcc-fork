;
; Ullrich von Bassewitz, 05.08.1998
;
; CC65 runtime: long add
;

        .export         tosadd0ax, tosaddeax
        .import         addysp1
        .importzp       sp, sreg, tmp1

; EAX = TOS + EAX

tosadd0ax:
        ldy     #$00
        sty     sreg
        sty     sreg+1

tosaddeax:
        clc
        adc     (sp)            ; 65SC02 version - saves 2 cycles
        ldy     #1
        sta     tmp1            ; use as temp storage
        txa
        adc     (sp),y          ; byte 1
        tax
        iny
        lda     sreg
        adc     (sp),y          ; byte 2
        sta     sreg
        iny
        lda     sreg+1
        adc     (sp),y          ; byte 3
        sta     sreg+1
        lda     tmp1            ; load byte 0
        jmp     addysp1         ; drop TOS

