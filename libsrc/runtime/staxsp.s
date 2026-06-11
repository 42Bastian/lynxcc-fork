;
; Ullrich von Bassewitz, 31.08.1998
;
; CC65 runtime: Store ax at (sp),y
;

        .export         staxysp, stax0sp
        .importzp       sp

        .macpack        cpu

.if (.cpu .bitand ::CPU_ISET_65SC02)
; 65SC02: store the low byte through (sp) and skip the LDY #0. Exits with
; Y = 1 and A/X preserved - same as the 6502 version.
stax0sp:
        sta     (sp)
        pha
        txa
        ldy     #1
        sta     (sp),y
        pla
        rts
.else
stax0sp:
        ldy     #0
.endif
staxysp:
        sta     (sp),y
        iny
        pha
        txa
        sta     (sp),y
        pla
        rts

