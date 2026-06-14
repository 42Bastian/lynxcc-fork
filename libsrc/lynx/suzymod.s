;
; cc65 Lynx fork: signed 16/16 modulo via the Suzy hardware math unit.
;
; Reached only through the fork-specific '!%' operator (design doc 2.6);
; the standard '%' operator keeps the software tosmodax routine.
;
; Combines the signed fixup of suzydiv.s with the n - (n/d)*d remainder
; computation of suzyumod.s (the hardware remainder registers are buggy,
; see suzyumod.s): negate both operands to positive, compute |n| mod |d|
; with one hardware divide and one hardware multiply, then negate the
; result if the dividend was negative (C truncation semantics: the
; remainder takes the sign of the dividend).
;
; Divide-by-zero: |n| % 0 yields |n| (see suzyumod.s), sign-adjusted to
; n. C behavior is undefined here, so that is acceptable.
;
; Contracts (see suzymul.s / LYNX_CODEGEN_DESIGN.md 2.6): sprite engine
; must be idle; NOT interrupt-safe; the SPRSYS unsafe-access bit may be set
; spuriously by any math operation (hardware bug).
;
; Exit: remainder in A/X. Uses A, X, Y, ptr1, tmp1, tmp2, tmp3.
;

        .export         tossuzymodax
        .import         popptr1, negax
        .importzp       ptr1, tmp1, tmp2, tmp3

        .include        "extzp.inc"
        .include        "lynx.inc"

tossuzymodax:
        cpx     #$80            ; rhs negative?
        bcc     @L0
        jsr     negax           ; make rhs positive
@L0:    sta     MATHP           ; |divisor| low
        sta     tmp1            ; save |divisor| for the multiply below
        stx     MATHN           ; |divisor| high
        stx     tmp2
        lda     __sprsys        ; sign-math off, accumulate off for the
        and     #$3F            ; multiply below; keeps the sprite control
        sta     SPRSYS          ; bits from the shadow byte
        jsr     popptr1         ; lhs -> ptr1 (A/Y clobbered, X untouched)
        lda     ptr1+1
        sta     tmp3            ; sign of the dividend
        bpl     @L1             ; dividend already positive?
        lda     #0              ; negate the dividend in ptr1 (it is needed
        sec                     ; again for the final subtraction)
        sbc     ptr1
        sta     ptr1
        lda     #0
        sbc     ptr1+1
        sta     ptr1+1
; Small-divisor normalization (design doc 2.6.3): for |divisor| < 256, divide
; |dividend|<<8 by |divisor|<<8 for the same quotient ~112 ticks faster. Only
; the divide is shifted; tmp1/tmp2 keep the original |divisor| for the
; (|n|/|d|)*|d| multiply below, and MATHD/MATHC still hold the quotient.

@L1:    lda     tmp2            ; |divisor| high byte
        bne     @wide           ; >= 256 -> stock placement
        stz     MATHP           ; (|d|<<8) low  = 0
        lda     tmp1
        sta     MATHN           ; (|d|<<8) high = |divisor| low byte
        stz     MATHH           ; dividend bits 0..7  = 0 (forces MATHG = 0)
        lda     ptr1
        sta     MATHG           ; |n| low  -> bits 8..15
        lda     ptr1+1
        sta     MATHF           ; |n| high -> bits 16..23 (forces MATHE = 0)
        stz     MATHE           ; writing MATHE starts the divide
        bra     @div
@wide:  lda     ptr1
        sta     MATHH           ; |dividend| low (also forces MATHG = 0)
        lda     ptr1+1
        sta     MATHG           ; |dividend| high
        stz     MATHF           ; zero-extend (also forces MATHE = 0)
        stz     MATHE           ; writing MATHE starts the divide
@div:   lda     SPRSYS
        bmi     @div            ; wait for the divide to complete

; The quotient low word is now in MATHD/MATHC, which is the multiply's
; C/D operand. Write |divisor| into B/A to compute (|n|/|d|)*|d|.

        lda     tmp1
        sta     MATHB           ; |divisor| low (also forces MATHA = 0)
        lda     tmp2
        sta     MATHA           ; writing MATHA starts the multiply (44 ticks)
@L3:    lda     SPRSYS
        bmi     @L3             ; wait for the multiply to complete

        lda     __sprsys
        sta     SPRSYS          ; restore SPRSYS      
; |remainder| = |n| - (|n|/|d|)*|d|, then apply the dividend's sign.

        sec
        lda     ptr1
        sbc     MATHH           ; low byte of (|n|/|d|)*|d|
        sta     tmp1
        lda     ptr1+1
        sbc     MATHG           ; high byte
        tax
        lda     tmp1
        ldy     tmp3            ; was the dividend negative?
        bmi     @L4
        rts                     ; lhs already popped by popptr1
@L4:    jmp     negax
