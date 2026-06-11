;
; cc65 Lynx fork: unsigned 16/16 modulo via the Suzy hardware math unit.
;
; Reached only through the fork-specific '!%' operator (design doc 2.6);
; the standard '%' operator keeps the software tosumodax routine.
;
; The hardware remainder registers (MATHL/MATHM) are NOT used: the Lynx
; hardware documentation (12.1.4, "Bugs in MathLand") states the remainder
; has two value-dependent errors - "just don't use it." The remainder is
; computed instead as  n - (n/d)*d : one hardware divide, one hardware
; multiply, one 16-bit subtract.
;
; The divide leaves its quotient in MATHA..MATHD, and MATHD/MATHC are
; exactly the multiply's C/D operand - so only the divisor needs to be
; written into MATHB/MATHA to start the (n/d)*d multiply.
;
; Divide-by-zero: the hardware quotient reads $FFFF, $FFFF*0 = 0, so
; n % 0 returns n. C behavior is undefined here, so that is acceptable.
;
; Contracts (see suzymul.s / LYNX_CODEGEN_DESIGN.md 2.6): sprite engine
; must be idle; NOT interrupt-safe; the SPRSYS unsafe-access bit may be set
; spuriously by any math operation (hardware bug).
;
; Exit: remainder in A/X. Uses A, X, Y, tmp1, tmp2.
;

        .export         tossuzyumodax
        .import         incsp2
        .importzp       sp, tmp1, tmp2

        .include        "extzp.inc"
        .include        "lynx.inc"

tossuzyumodax:
        sta     MATHP           ; divisor low
        sta     tmp1            ; save divisor for the multiply below
        stx     MATHN           ; divisor high
        stx     tmp2
        lda     __sprsys        ; sign-math off, accumulate off for the
        and     #$3F            ; multiply below; keeps the sprite control
        sta     SPRSYS          ; bits from the shadow byte
        lda     (sp)            ; dividend low
        sta     MATHH           ; (also forces MATHG = 0)
        ldy     #1
        lda     (sp),y          ; dividend high
        sta     MATHG
        stz     MATHF           ; zero-extend (also forces MATHE = 0)
        stz     MATHE           ; writing MATHE starts the divide
@L0:    lda     SPRSYS
        bmi     @L0             ; wait for the divide to complete

; The quotient low word is now in MATHD/MATHC, which is the multiply's
; C/D operand. Write the divisor into B/A to compute (n/d)*d.

        lda     tmp1
        sta     MATHB           ; divisor low (also forces MATHA = 0)
        lda     tmp2
        sta     MATHA           ; writing MATHA starts the multiply (44 ticks)
@L1:    lda     SPRSYS
        bmi     @L1             ; wait for the multiply to complete

; Remainder = n - (n/d)*d

        sec
        lda     (sp)
        sbc     MATHH           ; low byte of (n/d)*d
        sta     tmp1
        lda     (sp),y          ; Y is still 1
        sbc     MATHG           ; high byte of (n/d)*d
        tax
        lda     tmp1
        jmp     incsp2          ; drop lhs, return result in A/X
