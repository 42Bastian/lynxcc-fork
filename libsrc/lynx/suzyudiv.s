;
; cc65 Lynx fork: unsigned 16/16 division via the Suzy hardware math unit.
;
; Reached only through the fork-specific '!/' operator (design doc 2.6);
; the standard '/' operator keeps the software tosudivax routine.
;
; The 16-bit dividend is zero-extended into the 32-bit MATHE..MATHH group;
; Suzy's divide is unsigned only.
;
; Divide-by-zero: the hardware returns $FFFFFFFF, so the quotient reads as
; $FFFF. C behavior is undefined here, so that is acceptable.
;
; Contracts (see suzymul.s / LYNX_CODEGEN_DESIGN.md 2.6): sprite engine
; must be idle; NOT interrupt-safe; the SPRSYS unsafe-access bit may be set
; spuriously by any math operation (hardware bug).
;
; Exit: quotient in A/X. Uses A, X, Y; no zero-page temporaries.
; (No SPRSYS setup is needed: sign-math and accumulate affect multiply only.)
;

        .export         tossuzyudivax
        .import         incsp2
        .importzp       sp

        .include        "lynx.inc"

tossuzyudivax:
        sta     MATHP           ; divisor low
        stx     MATHN           ; divisor high
        lda     (sp)            ; dividend low
        sta     MATHH           ; (also forces MATHG = 0)
        ldy     #1
        lda     (sp),y          ; dividend high
        sta     MATHG
        stz     MATHF           ; zero-extend (also forces MATHE = 0)
        stz     MATHE           ; writing MATHE starts the divide
                                ; (176 + 14*N ticks, N = leading zeros of divisor)
@L0:    lda     SPRSYS
        bmi     @L0             ; wait for the math-working bit to clear
        lda     MATHD           ; quotient bits 0..7
        ldx     MATHC           ; quotient bits 8..15
        jmp     incsp2          ; drop lhs, return result in A/X
