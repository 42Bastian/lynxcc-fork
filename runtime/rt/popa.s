;
; Ullrich von Bassewitz, 25.10.2000
;
; CC65 runtime: Pop a from stack
;

        .export         popa
        .importzp       sp

.proc   popa

        lda     (sp)
        inc     sp              ; (12)
        beq     @L1             ; (14)
        rts                     ; (20)

@L1:    inc     sp+1
        rts

.endproc

