;
; Randum number generator
;
; Written and donated by Sidney Cadot - sidney@ch.twi.tudelft.nl
; 2016-11-07, modified by Brad Smith
; 2019-10-07, modified by Lewis "LRFLEW" Fox
;
; May be distributed with the cc65 runtime using the same license.
;
;
; int rand (void);
; void srand (unsigned seed);
;
;  Uses 4-byte state.
;  Multiplier must be 1 (mod 4)
;  Added value must be 1 (mod 2)
;  This guarantees max. period (2**32)
;  The lowest bits have poor entropy and
;  exhibit easily detectable patterns, so
;  only the upper bits 16-22 and 24-31 of the
;  4-byte state are returned.
;
;  The best 8 bits, 24-31 are returned in the
;  low byte A to provide the best entropy in the
;  most commonly used part of the return value.
;
;  Uses the following LCG values for ax + c (mod m)
;  a = $01010101
;  c = $B3B3B3B3
;  m = $100000000 (32-bit truncation)
;
;  The multiplier was carefully chosen such that it can
;  be computed with 3 adc instructions, and the increment
;  was chosen to have the same value in each byte to allow
;  the addition to be performed in conjunction with the
;  multiplication, adding only 1 additional adc instruction.
;

        .export         _rand, _srand, _randmask
        .importzp       tmp1

.data

; The seed. When srand() is not called, the C standard says that that rand()
; should behave as if srand() was called with an argument of 1 before.
rand:   .dword   1

.code

_rand:  clc
        lda     rand+0
        adc     #$B3
        sta     rand+0
        adc     rand+1
        sta     rand+1
        adc     rand+2
        sta     rand+2
        and     #$7f            ; Suppress sign bit (make it positive)
        tax
        lda     rand+2
        adc     rand+3
        sta     rand+3
        rts                     ; return bit (16-22,24-31) in (X,A)

; unsigned char __fastcall__ randmask (unsigned char mask);
;
; Advance the generator and return the best output byte (bits 24-31)
; ANDed with the caller-supplied mask, in A (X = 0). Convenient for
; power-of-two ranges, e.g. randmask(0x07) -> 0..7. Because rand()
; already returns the high-entropy bits in its low byte, masking here
; keeps full quality (unlike masking the low bits of a raw LCG state).
_randmask:
        sta     tmp1            ; Save the mask (fastcall arg arrives in A)
        jsr     _rand           ; Advance LCG; A = bits 24-31, X = bits 16-22
        and     tmp1            ; Keep only the requested bits
        ldx     #0              ; High byte of the (char) result is zero
        rts

_srand: sta     rand+0          ; Store the seed
        stx     rand+1
        lda     #0
        sta     rand+2          ; Set MSW to zero
        sta     rand+3
        rts


