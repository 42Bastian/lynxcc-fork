;
; Ullrich von Bassewitz, 05.08.1998
; Christian Krueger, 11-Mar-2017, added 65SC02 optimization
;
; CC65 runtime: sub ints reversed
;

        .export         tosrsuba0, tosrsubax
        .import         addysp1
        .importzp       sp, tmp1

;
; AX = AX - TOS
;

tosrsuba0:
        ldx     #0
tosrsubax:
        sec
        sbc     (sp)
        ldy     #1
        sta     tmp1            ; save lo byte
        txa
        sbc     (sp),y          ; hi byte
        tax
        lda     tmp1
        jmp     addysp1         ; drop TOS, set condition codes

