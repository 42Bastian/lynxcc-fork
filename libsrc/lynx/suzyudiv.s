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
; Contracts (see suzymul.s / design/LYNX_CODEGEN_DESIGN.md 2.6): sprite engine
; must be idle; NOT interrupt-safe; the SPRSYS unsafe-access bit may be set
; spuriously by any math operation (hardware bug).
;
; Small-divisor normalization (design doc 2.6.3): the divide costs
; 176 + 14*N ticks, N = leading zeros of the divisor, so a divisor < 256 is
; the slow case. (n<<8)/(d<<8) has the same quotient as n/d but the divisor
; now occupies its high byte, erasing 8 leading zeros and saving ~112 ticks.
; The 16-bit dividend zero-extends to <= 24 bits after the shift, so it never
; overflows the 32-bit MATHE..MATHH dividend. The shift is taken only when the
; divisor high byte is zero; otherwise the stock placement runs unchanged.
;
; Exit: quotient in A/X. Uses A, X, Y; no zero-page temporaries.
; (No SPRSYS setup is needed: sign-math and accumulate affect multiply only.)
;

        .export         tossuzyudivax
        .import         incsp2
        .importzp       sp

        .include        "lynx.inc"

tossuzyudivax:
        cpx     #0              ; divisor high byte zero -> divisor < 256
        bne     @wide

; Narrow divisor: divisor<<8 and dividend<<8, ~112 ticks cheaper (2.6.3).
        stz     MATHP           ; (d<<8) low  = 0
        sta     MATHN           ; (d<<8) high = divisor low byte
        stz     MATHH           ; dividend bits 0..7  = 0 (forces MATHG = 0)
        lda     (sp)            ; n low  -> bits 8..15
        sta     MATHG
        ldy     #1
        lda     (sp),y          ; n high -> bits 16..23
        sta     MATHF           ; (forces MATHE = 0)
        stz     MATHE           ; bits 24..31 = 0; writing MATHE starts divide
        bra     @wait

@wide:  sta     MATHP           ; divisor low
        stx     MATHN           ; divisor high
        lda     (sp)            ; dividend low
        sta     MATHH           ; (also forces MATHG = 0)
        ldy     #1
        lda     (sp),y          ; dividend high
        sta     MATHG
        stz     MATHF           ; zero-extend (also forces MATHE = 0)
        stz     MATHE           ; writing MATHE starts the divide
                                ; (176 + 14*N ticks, N = leading zeros of divisor)
@wait:  lda     SPRSYS
        bmi     @wait           ; wait for the math-working bit to clear
        lda     MATHD           ; quotient bits 0..7
        ldx     MATHC           ; quotient bits 8..15
        jmp     incsp2          ; drop lhs, return result in A/X
