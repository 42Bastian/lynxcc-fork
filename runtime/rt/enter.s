;
; Ullrich von Bassewitz, 06.08.1998
;
; CC65 runtime: function prologue
;

        .export         enter
        .importzp       sp

; 65SC02: store through (sp), no Y reload needed. The compiler has no
; register info entry for this function, so all registers are assumed
; changed - the different exit value of Y is safe.
enter:  tya                     ; get arg size
        ldy     sp
        bne     L1
        dec     sp+1
L1:     dec     sp
        sta     (sp)            ; Store the arg count
        rts

