;
; Ullrich von Bassewitz, 05.08.1998
; Christian Krueger, 11-Mar-2017, spend two bytes for one cycle, improved 65SC02 optimization
;
; CC65 runtime: add ints
;

; Make this as fast as possible, even if it needs more space since it's
; called a lot!

        .export         tosadda0, tosaddax
        .importzp       sp, tmp1

tosadda0:
        ldx     #0
tosaddax:
        clc                     ; (2)

        adc     (sp)            ; 7
        tay                     ; 9
        txa                     ; 11
        inc     sp              ; 16
        beq     hiadd1          ; 18 / 19
        adc     (sp)            ; 23
        tax                     ; 25
        tya                     ; 27
        inc     sp              ; 32
        beq     hiadd2          ; 34 / 35

        rts

hiadd2:
        inc     sp+1            ; 40
        rts

hiadd1:
        inc     sp+1            ; 40
        adc     (sp)            ; 45
        tax                     ; 47
        tya                     ; 49
        inc     sp              ; 54   no check needed!

        rts                     ; (6502: 45 cycles, 26 bytes <-> 65SC02: best case 34cycles
