;
; Ullrich von Bassewitz, 26.10.2000
;
; CC65 runtime: Push value in a onto the stack
;

        .export         pusha0sp, pushaysp, pusha
        .importzp       sp

        .macpack        cpu

; Beware: The optimizer knows about this function!

pusha0sp:
        ldy     #$00
pushaysp:
        lda     (sp),y
.if (.cpu .bitand ::CPU_ISET_65SC02)
; 65SC02 version. Uses STA (sp) and avoids reloading Y. Beware: Exit value
; of Y is the low byte of sp here, not 0 - the function info table in the
; compiler (codeinfo.c) marks Y as changed/unknown, so this is safe.
pusha:  ldy     sp              ; (3)
        beq     @L1             ; (6)
        dec     sp              ; (11)
        sta     (sp)            ; (16)
        rts                     ; (22)

@L1:    dec     sp+1            ; (11)
        dec     sp              ; (16)
        sta     (sp)            ; (21)
        rts                     ; (27)
.else
pusha:  ldy     sp              ; (3)
        beq     @L1             ; (6)
        dec     sp              ; (11)
        ldy     #0              ; (13)
        sta     (sp),y          ; (19)
        rts                     ; (25)

@L1:    dec     sp+1            ; (11)
        dec     sp              ; (16)
        sta     (sp),y          ; (22)
        rts                     ; (28)
.endif

