;
; Ullrich von Bassewitz, 05.08.1998
;
; CC65 runtime: Increment ax by 1
;

        .export         incax1

        .macpack        generic

.proc   incax1

        ina                     ; 65C02 version
        bne     @L9
        inx
@L9:    rts

.endproc
