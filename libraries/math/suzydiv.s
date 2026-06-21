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
; Contracts (see suzymul.s / design/LYNX_CODEGEN_DESIGN.md 2.6): sprite engine
; must be idle; NOT interrupt-safe; the SPRSYS unsafe-access bit may be set
; spuriously by any math operation (hardware bug).
;
; Small-divisor normalization (design doc 2.6.3) applies to the unsigned
; magnitudes: when |divisor| < 256, the divide of |dividend|<<8 by |divisor|<<8
; yields the same quotient ~112 ticks faster. |dividend| <= 32768, so the
; <<8 stays within the 32-bit dividend. The dividend magnitude is formed in
; ptr1 first so a single narrow/wide branch places it.
;
; Exit: quotient in A/X. Uses A, X, Y, ptr1, tmp1, tmp2, tmp3.
;

        .export         tossuzydivax
        .import         popptr1, negax
        .importzp       ptr1, tmp1, tmp2, tmp3

        .include "lynx/lynx.inc"

tossuzydivax:
        stx     tmp2            ; sign of rhs (high byte)
        cpx     #$80            ; rhs negative?
        bcc     @rpos
        jsr     negax           ; make rhs positive
@rpos:  sta     tmp3            ; |divisor| low (X = |divisor| high, preserved)
        jsr     popptr1         ; lhs -> ptr1 (A/Y clobbered, X untouched)
        lda     ptr1+1
        sta     tmp1            ; sign of lhs
        bpl     @dpos           ; lhs already positive?
        lda     #0              ; form |dividend| in place in ptr1
        sec
        sbc     ptr1
        sta     ptr1
        lda     #0
        sbc     ptr1+1
        sta     ptr1+1
@dpos:                          ; ptr1 = |dividend|, X = |divisor| high

        cpx     #0              ; |divisor| < 256 -> normalize (2.6.3)
        bne     @wide
        stz     MATHP           ; (|d|<<8) low  = 0
        lda     tmp3
        sta     MATHN           ; (|d|<<8) high = |divisor| low byte
        stz     MATHH           ; dividend bits 0..7  = 0 (forces MATHG = 0)
        lda     ptr1
        sta     MATHG           ; |n| low  -> bits 8..15
        lda     ptr1+1
        sta     MATHF           ; |n| high -> bits 16..23 (forces MATHE = 0)
        stz     MATHE           ; writing MATHE starts the divide
        bra     @wait

@wide:  lda     tmp3
        sta     MATHP           ; |divisor| low
        stx     MATHN           ; |divisor| high
        lda     ptr1
        sta     MATHH           ; |dividend| low (also forces MATHG = 0)
        lda     ptr1+1
        sta     MATHG           ; |dividend| high
        stz     MATHF           ; zero-extend (also forces MATHE = 0)
        stz     MATHE           ; writing MATHE starts the divide
@wait:  lda     SPRSYS
        bmi     @wait           ; wait for the math-working bit to clear

; Fix up the result sign: negative iff operand signs differ.

        lda     tmp1
        eor     tmp2
        bmi     @neg
        lda     MATHD           ; quotient bits 0..7
        ldx     MATHC           ; quotient bits 8..15
        rts                     ; lhs already popped by popptr1
@neg:   lda     MATHD
        ldx     MATHC
        jmp     negax
