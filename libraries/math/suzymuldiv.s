; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.

;
; cc65 Lynx fork: fused (a*b)/c via the Suzy hardware math unit.
;
; Reached only through the fork-specific fused '!*'+'!/' parser path
; (a !* b !/ c); the separate '!*' and '!/' operators keep their own
; routines, and the standard '*'/'/'  keep the software routines.
;
; The point of fusing: Suzy's multiply leaves its full 32-bit product in
; the MATHE..MATHH register group, which is exactly the divide's dividend
; register. So the product is divided IN PLACE - the intermediate is never
; truncated to 16 bits and never read back to the CPU and reloaded. This
; both removes a 32-bit readback + zero-extended reload AND fixes the silent
; 16-bit overflow that 'a !* b' (taken alone) suffers when a*b > 65535.
;
; This is the legitimate, polled form of the multiply->divide register
; chaining that the "QbertRoot" hardware joke (hardware docs 12.4) abuses by
; NOT waiting for the multiply to finish. Here MULTSTAT is polled between the
; two operations, so the dividend is the settled product, not a race.
;
; The divide is started WITHOUT disturbing the product: writing MATHE starts
; a divide, and MATHE already holds the product's most-significant byte, so
; it is simply rewritten with its own value. (Writing MATHE forces nothing to
; zero - only writes to B,D,F,H,K,M force their pair partner to 0 - so
; MATHE..MATHH stay intact.)
;
; Entry:  divisor c in A/X (primary); the two factors on the C stack,
;         'a' pushed first, 'b' pushed second (b = TOS):
;             (sp)+0,1 = b low,high      (sp)+2,3 = a low,high
; Exit:   quotient low word in A/X; both stacked factors dropped (incsp4).
;
; Contracts (see suzymul.s / design/LYNX_CODEGEN_DESIGN.md 2.6): the Suzy sprite
; engine must be idle (it shares the math unit); NOT interrupt-safe (the math
; registers are global hardware state with no safe save/restore); the SPRSYS
; unsafe-access bit may be set spuriously by any math operation (hardware bug).
;
; Divide-by-zero: the hardware quotient reads $FFFF. C behavior is undefined
; here, so that is acceptable (matches the other Suzy divide routines).
;
; tossuzyumuldivax: unsigned.  Uses A, X, Y; no zero-page temporaries.
; tossuzymuldivax:  signed.    Uses A, X, Y, tmp1.
;

        .export         tossuzymuldivax, tossuzyumuldivax
        .import         incsp4, negax
        .importzp       sp, tmp1

        .include        "lynx/extzp.inc"
        .include "lynx/lynx.inc"

; ---------------------------------------------------------------------------
; Unsigned (a*b)/c
; ---------------------------------------------------------------------------

tossuzyumuldivax:
        sta     MATHP           ; divisor low
        stx     MATHN           ; divisor high
        lda     __sprsys        ; sign-math off, accumulate off for the
        and     #$3F            ; multiply; keep the sprite control bits from
        sta     SPRSYS          ; the shadow byte (also resets unsafe-access)

        lda     (sp)            ; b low
        sta     MATHD           ; (also forces MATHC = 0)
        ldy     #1
        lda     (sp),y          ; b high
        sta     MATHC
        ldy     #2
        lda     (sp),y          ; a low
        sta     MATHB           ; (also forces MATHA = 0)
        ldy     #3
        lda     (sp),y          ; a high
        sta     MATHA           ; start multiply: MATHE..MATHH = a*b (32-bit)
@L0:    lda     SPRSYS
        bmi     @L0             ; wait for the multiply to complete

        lda     MATHE           ; product MSB (bits 24..31), already in place
        sta     MATHE           ; rewrite same value -> starts the divide of
                                ; the 32-bit product by MATHN/MATHP
@L1:    lda     SPRSYS
        bmi     @L1             ; wait for the divide to complete
        lda     MATHD           ; quotient bits 0..7
        ldx     MATHC           ; quotient bits 8..15
        jmp     incsp4          ; drop a and b, return result in A/X

; ---------------------------------------------------------------------------
; Signed (a*b)/c
;
; Suzy's divide is unsigned and its signed multiply is buggy, so all three
; operands are reduced to magnitudes, the unsigned (|a|*|b|)/|c| is computed,
; and the result is negated iff an odd number of operands were negative.
; |a|,|b| <= 32768, so |a|*|b| <= 2^30 fits the 32-bit product. A $8000
; operand negates to itself and is read as the unsigned magnitude 32768,
; which is correct (matches tossuzydivax).
; ---------------------------------------------------------------------------

tossuzymuldivax:
        stx     tmp1            ; seed sign accumulator with c's high byte
        cpx     #$80            ; c negative?
        bcc     @cpos
        jsr     negax           ; c = |c|
@cpos:  sta     MATHP           ; |c| low
        stx     MATHN           ; |c| high
        lda     __sprsys        ; sign-math off, accumulate off for the multiply
        and     #$3F
        sta     SPRSYS

; b: fold its sign into tmp1, then make it positive in place.

        ldy     #1
        lda     (sp),y          ; b high
        eor     tmp1
        sta     tmp1
        lda     (sp),y          ; b high again
        bpl     @bpos
        lda     #0
        sec
        sbc     (sp)            ; 0 - b low
        sta     (sp)
        lda     #0
        ldy     #1
        sbc     (sp),y          ; 0 - b high - borrow
        sta     (sp),y
@bpos:

; a: fold its sign into tmp1, then make it positive in place.

        ldy     #3
        lda     (sp),y          ; a high
        eor     tmp1
        sta     tmp1
        lda     (sp),y          ; a high again
        bpl     @apos
        ldy     #2
        lda     #0
        sec
        sbc     (sp),y          ; 0 - a low
        sta     (sp),y
        ldy     #3
        lda     #0
        sbc     (sp),y          ; 0 - a high - borrow
        sta     (sp),y
@apos:

; magnitudes are in place; multiply then divide as in the unsigned path.

        lda     (sp)            ; |b| low
        sta     MATHD           ; (also forces MATHC = 0)
        ldy     #1
        lda     (sp),y          ; |b| high
        sta     MATHC
        ldy     #2
        lda     (sp),y          ; |a| low
        sta     MATHB           ; (also forces MATHA = 0)
        ldy     #3
        lda     (sp),y          ; |a| high
        sta     MATHA           ; start multiply
@L2:    lda     SPRSYS
        bmi     @L2
        lda     MATHE
        sta     MATHE           ; start divide of the 32-bit product
@L3:    lda     SPRSYS
        bmi     @L3
        lda     MATHD           ; |quotient| bits 0..7
        ldx     MATHC           ; |quotient| bits 8..15

; Sign fixup: negative iff an odd number of operands were negative.

        bit     tmp1            ; N <- bit 7 of the sign accumulator
        bpl     @done
        jsr     negax           ; result = -result
@done:  jmp     incsp4          ; drop a and b, return result in A/X
