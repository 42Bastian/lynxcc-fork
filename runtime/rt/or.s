;
; Ullrich von Bassewitz, 05.08.1998
; Christian Krueger, 11-Mar-2017, added 65SC02 optimization
;
; CC65 runtime: or on ints
;

        .export         tosora0, tosorax
        .import         addysp1
        .importzp       sp, tmp1

tosora0:
        ldx     #$00
tosorax:
        ora     (sp)
        ldy     #1
        sta     tmp1
        txa
        ora     (sp),y
        tax
        lda     tmp1
        jmp     addysp1         ; drop TOS, set condition codes

