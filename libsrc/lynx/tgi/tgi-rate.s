;
; Lynx static TGI: display frame rate.
;
; unsigned char __fastcall__ tgi_setframerate (unsigned char rate);
;
; rate is 50, 60 or 75 (Hz). Returns 0 on success, nonzero for an
; invalid rate (the only fallible call left in the library - the error
; model is gone, see LYNX_TGI_DESIGN.md sec. 2.5).
;
; Only the timer backup registers are written, which is the safe subset
; of timer handling (spec ch. 3.3).
;

        .include        "lynx.inc"

        .export         _tgi_setframerate

.code

_tgi_setframerate:
        cmp     #75
        beq     @r75
        cmp     #60
        beq     @r60
        cmp     #50
        beq     @r50
        lda     #1              ; Invalid rate
        bra     @exit

@r50:   lda     #$bd            ; 50 Hz
        ldx     #$31
        bra     @set
@r60:   lda     #$9e            ; 60 Hz
        ldx     #$29
        bra     @set
@r75:   lda     #$7e            ; 75 Hz
        ldx     #$20
@set:   sta     HTIMBKUP
        stx     PBKUP
        lda     #0              ; Success
@exit:
        ldx     #0
        rts
