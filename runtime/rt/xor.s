;
; Ullrich von Bassewitz, 05.08.1998
; Christian Krueger, 11-Mar-2017, added 65SC02 optimization
;
; CC65 runtime: xor on ints
;

        .export         tosxora0, tosxorax
        .import         addysp1
        .importzp       sp, tmp1

tosxora0:
        ldx     #$00
tosxorax:
        eor     (sp)
        ldy     #1
        sta     tmp1
        txa
        eor     (sp),y
        tax
        lda     tmp1
        jmp     addysp1         ; drop TOS, set condition codes

