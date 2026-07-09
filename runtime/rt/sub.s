;
; Ullrich von Bassewitz, 05.08.1998
;
; CC65 runtime: sub ints
;

        .export         tossuba0, tossubax
        .import         addysp1
        .importzp       sp

; AX = TOS - AX

tossuba0:
        ldx     #0
tossubax:
        sec
        eor     #$FF
        adc     (sp)
        ldy     #1
        pha                     ; Save high byte
        txa
        eor     #$FF
        adc     (sp),y          ; Subtract high byte
        tax                     ; High byte into X
        pla                     ; Restore low byte
        jmp     addysp1         ; drop TOS

