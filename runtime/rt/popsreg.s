;
; Ullrich von Bassewitz, 1998-08-21, 2009-02-22
;
; CC65 runtime: Pop TOS into sreg
;

        .export         popsreg
        .import         incsp2
        .importzp       sp, sreg

popsreg:
        pha                     ; save A
        ldy     #1
        lda     (sp),y          ; get hi byte
        sta     sreg+1          ; store it
        lda     (sp)            ; get lo byte
        sta     sreg            ; store it
        pla                     ; get A back
        jmp     incsp2          ; bump stack and return

