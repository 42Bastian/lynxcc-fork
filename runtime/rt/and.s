;
; Ullrich von Bassewitz, 05.08.1998
;
; CC65 runtime: and on ints
;

        .export         tosanda0, tosandax
        .import         addysp1
        .importzp       sp, ptr4

tosanda0:
        ldx     #$00
tosandax:
        and     (sp)            ; 65SC02 version, saves 2 cycles and 1 byte
        ldy     #1
        pha
        txa
        and     (sp),y
        tax
        pla
        jmp     addysp1         ; drop TOS, set condition codes

