;
; cc65 Lynx fork: signed 16/16 division via the Suzy hardware math unit.
;
; Reached only through the fork-specific '!/' operator (design doc 2.6);
; the standard '/' operator keeps the software tosdivax routine.
;
; Suzy's divide is unsigned only (its sign-math mode applies solely to
; multiply, and is buggy there), so the software fixup is mandatory:
; negate the operands to positive, divide unsigned in hardware, then fix
; up the result sign (C truncation semantics: quotient is negative iff
; the operand signs differ).
;
; As with the software tosdivax, the case of a $8000 operand is benign:
; negation leaves $8000 in place, which the unsigned divide reads as
; 32768 - the correct magnitude.
;
; Divide-by-zero: the hardware quotient reads $FFFF (possibly negated).
; C behavior is undefined here, so that is acceptable.
;
; Contracts (see suzymul.s / LYNX_CODEGEN_DESIGN.md 2.6): sprite engine
; must be idle; NOT interrupt-safe; the SPRSYS unsafe-access bit may be set
; spuriously by any math operation (hardware bug).
;
; Exit: quotient in A/X. Uses A, X, Y, ptr1, tmp1, tmp2.
;

        .export         tossuzydivax
        .import         popptr1, negax
        .importzp       ptr1, tmp1, tmp2

        .include        "lynx.inc"

tossuzydivax:
        stx     tmp2            ; sign of rhs (high byte)
        cpx     #$80            ; rhs negative?
        bcc     @L0
        jsr     negax           ; make rhs positive
@L0:    sta     MATHP           ; |divisor| low
        stx     MATHN           ; |divisor| high
        jsr     popptr1         ; lhs -> ptr1 (A/Y clobbered, X untouched)
        lda     ptr1+1
        sta     tmp1            ; sign of lhs
        bpl     @L1             ; lhs already positive?
        lda     #0              ; negate the dividend while writing it
        sec
        sbc     ptr1
        sta     MATHH           ; |dividend| low (also forces MATHG = 0)
        lda     #0
        sbc     ptr1+1
        sta     MATHG           ; |dividend| high
        bra     @L2
@L1:    lda     ptr1
        sta     MATHH           ; dividend low (also forces MATHG = 0)
        lda     ptr1+1
        sta     MATHG           ; dividend high
@L2:    stz     MATHF           ; zero-extend (also forces MATHE = 0)
        stz     MATHE           ; writing MATHE starts the divide
@L3:    lda     SPRSYS
        bmi     @L3             ; wait for the math-working bit to clear

; Fix up the result sign: negative iff operand signs differ.

        lda     tmp1
        eor     tmp2
        bmi     @L4
        lda     MATHD           ; quotient bits 0..7
        ldx     MATHC           ; quotient bits 8..15
        rts                     ; lhs already popped by popptr1
@L4:    lda     MATHD
        ldx     MATHC
        jmp     negax
