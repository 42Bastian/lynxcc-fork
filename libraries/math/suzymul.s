;
; cc65 Lynx fork: 16x16 multiplication via the Suzy hardware math unit.
;
; Reached only through the fork-specific '!*' operator (design doc 2.6);
; the standard '*' operator keeps the software tosmulax/tosumulax routines.
;
; One unsigned hardware multiply serves both entries: cc65's int multiply
; returns only the low 16 bits of the product, which are identical for
; signed and unsigned operands.
;
; Contracts (see design/LYNX_CODEGEN_DESIGN.md 2.6):
;   - The Suzy sprite engine must be idle (it shares the math unit during
;     scaled sprite rendering). TGI draws synchronously, so this holds by
;     construction unless custom asynchronous sprite code is used.
;   - NOT interrupt-safe: the math registers are global hardware state and
;     there is no safe save/restore protocol (rewriting the inputs starts a
;     new operation). Do not perform '!*'/'!/'/'!%' math in IRQ handlers.
;   - The SPRSYS unsafe-access bit may be set spuriously by any math
;     operation (hardware bug). Sprite-debug code using that bit must reset
;     it after hardware math.
;
; Exit: result low word in A/X. Uses A, X, Y; no zero-page temporaries.
;

        .export         tossuzymulax, tossuzyumulax
        .import         incsp2
        .importzp       sp

        .include        "lynx/extzp.inc"
        .include "lynx/lynx.inc"

tossuzymulax:
tossuzyumulax:
        sta     MATHD           ; rhs low  (also forces MATHC = 0)
        stx     MATHC           ; rhs high
        lda     __sprsys        ; sign-math off, accumulate off; keeps the
        and     #$3F            ; sprite control bits from the shadow byte
        sta     SPRSYS          ; (CLR_UNSAFE in the shadow also resets the
                                ; unsafe-access bit from a previous op)
        lda     (sp)            ; lhs low
        sta     MATHB           ; (also forces MATHA = 0)
        ldy     #1
        lda     (sp),y          ; lhs high
        sta     MATHA           ; writing MATHA starts the multiply (44 ticks)
@L0:    lda     SPRSYS
        bmi     @L0             ; wait for the math-working bit to clear
        lda     __sprsys
        sta     SPRSYS          ; restore SPRSYS      

        lda     MATHH           ; product bits 0..7
        ldx     MATHG           ; product bits 8..15
        jmp     incsp2          ; drop lhs, return result in A/X
