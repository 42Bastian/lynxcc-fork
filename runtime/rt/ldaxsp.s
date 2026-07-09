;
; Ullrich von Bassewitz, 31.08.1998
;
; CC65 runtime: Load ax from offset in stack
;

        .export         ldax0sp, ldaxysp
        .importzp       sp

; Beware: The optimizer knows about the value in Y after return! (The
; compiler's function info table marks Y as changed/unknown for both
; entry points, and all inline-replacement passes match the ldaxysp call
; itself, so the differing exit value of Y below is safe.)

; 65SC02: load the low byte through (sp), saving the DEY. Exits with
; Y = 1 instead of 0.
ldax0sp:
        ldy     #1
        lda     (sp),y          ; get high byte
        tax                     ; and save it
        lda     (sp)            ; load low byte
        rts
ldaxysp:
        lda     (sp),y          ; get high byte
        tax                     ; and save it
        dey                     ; point to lo byte
        lda     (sp),y          ; load low byte
        rts

