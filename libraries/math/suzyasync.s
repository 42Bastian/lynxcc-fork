;
; cc65 Lynx fork: ASYNCHRONOUS (non-blocking) Suzy hardware math.
;
; See design/LYNX_SUZY_ASYNC_MATH_DESIGN.md. These are the start/poll/harvest
; counterparts of the synchronous '!*'/'!/'/'!%' operator routines
; (suzymul.s / suzy?div.s / suzy?mod.s / suzymuldiv.s). A *_start routine
; writes the operand registers and the trigger register and returns WITHOUT
; polling, so the caller can run unrelated (non-Suzy) CPU code while Suzy
; computes; the matching *_result routine harvests the result, polling
; defensively so an early harvest degrades to synchronous rather than
; returning garbage.
;
; These are reached as ordinary C functions (declared in <suzymath.h>), NOT
; as operators - the compiler is unchanged. cc65's default (__fastcall__)
; convention puts the rightmost argument in A/X and the rest on the C stack,
; which lines up exactly with how the operator routines already receive
; operands (rhs in A/X, lhs on the stack).
;
; CONTRACTS (design/LYNX_CODEGEN_DESIGN.md 2.6, plus one new rule):
;   - Exactly ONE async operation may be in flight at a time (single math
;     unit, single shared state block below).
;   - Between a *_start and its *_result the overlapped code must touch NO
;     Suzy state: no TGI draw, no SPRGO, no other Suzy math, no SPRSYS write.
;     The sprite engine shares these registers.
;   - Not interrupt-safe; sprite engine must be idle; the SPRSYS unsafe bit
;     may be set spuriously by any math op; divide-by-zero returns $FFFF.
;
; Carried state (survives the gap between start and result) lives in this
; module's BSS, NOT in the tmp*/ptr1 runtime scratch, because the overlapped
; C code is free to clobber that scratch. One shared block suffices because
; only one op is ever outstanding. Unsigned divide and multiply carry no
; state at all.
;
; Single module by design: referencing any one async entry links the whole
; file (a few hundred bytes); programs that never call one link none of it.
;

        .export         _suzy_udiv_start, _suzy_udiv_result
        .export         _suzy_div_start,  _suzy_div_result
        .export         _suzy_umod_start, _suzy_umod_result
        .export         _suzy_mod_start,  _suzy_mod_result
        .export         _suzy_umul_start, _suzy_umul_result
        .export         _suzy_umuldiv_start, _suzy_umuldiv_result
        .export         _suzy_muldiv_start,  _suzy_muldiv_result

        .import         incsp2, incsp4, negax, popptr1
        .importzp       sp, ptr1, tmp1, tmp2, tmp3

        .include        "lynx/extzp.inc"
        .include "lynx/lynx.inc"

; ===========================================================================
; Carried state (module BSS). One op in flight, so a single block is enough.
;   __sa_flag : bit7 set => negate the result at harvest (signed div/mod/
;               muldiv).
;   __sa_n    : modulo only - the (magnitude of the) dividend, needed for the
;               final n-(n/d)*d subtraction.
;   __sa_d    : modulo only - the (magnitude of the) divisor, needed to start
;               the (n/d)*d multiply at harvest.
; ===========================================================================

        .bss
__sa_flag:      .res    1
__sa_n:         .res    2
__sa_d:         .res    2

        .code

; ===========================================================================
; Unsigned divide.  q = suzy_udiv_start(n,d) ... suzy_udiv_result()
;   entry: d in A/X, n at (sp). No carried state.
; Mirrors tossuzyudivax (incl. small-divisor normalization, design 2.6.3)
; but returns the instant the divide is started.
; ===========================================================================

_suzy_udiv_start:
        cpx     #0              ; divisor high zero -> divisor < 256 (narrow)
        bne     @wide
        stz     MATHP           ; (d<<8) low  = 0
        sta     MATHN           ; (d<<8) high = divisor low byte
        stz     MATHH           ; dividend bits 0..7  = 0 (forces MATHG = 0)
        lda     (sp)            ; n low  -> bits 8..15
        sta     MATHG
        ldy     #1
        lda     (sp),y          ; n high -> bits 16..23
        sta     MATHF           ; (forces MATHE = 0)
        stz     MATHE           ; writing MATHE starts the divide
        jmp     incsp2          ; drop n, return (divide runs in background)
@wide:  sta     MATHP           ; divisor low
        stx     MATHN           ; divisor high
        lda     (sp)            ; dividend low
        sta     MATHH           ; (also forces MATHG = 0)
        ldy     #1
        lda     (sp),y          ; dividend high
        sta     MATHG
        stz     MATHF           ; zero-extend (also forces MATHE = 0)
        stz     MATHE           ; writing MATHE starts the divide
        jmp     incsp2

_suzy_udiv_result:
@w:     lda     SPRSYS
        bmi     @w              ; defensive poll: degrade to synchronous
        lda     MATHD           ; quotient bits 0..7
        ldx     MATHC           ; quotient bits 8..15
        rts

; ===========================================================================
; Signed divide.  q = suzy_div_start(n,d) ... suzy_div_result()
;   entry: d in A/X, n at (sp).
; Mirrors tossuzydivax: reduce to magnitudes, divide unsigned, and record
; "negate result" (bit7 of __sa_flag) = signs of n and d differ.
; ===========================================================================

_suzy_div_start:
        stx     tmp2            ; sign of divisor (high byte)
        cpx     #$80
        bcc     @rpos
        jsr     negax           ; |divisor|
@rpos:  sta     tmp3            ; |divisor| low  (X = |divisor| high)
        jsr     popptr1         ; dividend -> ptr1 (stack cleaned; X kept)
        lda     ptr1+1
        eor     tmp2            ; sign(n) ^ sign(d) -> result sign
        sta     __sa_flag       ; bit7 = negate at harvest
        lda     ptr1+1
        bpl     @dpos           ; dividend already positive?
        lda     #0              ; |dividend| in place in ptr1
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
        stz     MATHE           ; start the divide
        rts                     ; stack already cleaned by popptr1
@wide:  lda     tmp3
        sta     MATHP           ; |divisor| low
        stx     MATHN           ; |divisor| high
        lda     ptr1
        sta     MATHH           ; |dividend| low (also forces MATHG = 0)
        lda     ptr1+1
        sta     MATHG           ; |dividend| high
        stz     MATHF           ; zero-extend (also forces MATHE = 0)
        stz     MATHE           ; start the divide
        rts

_suzy_div_result:
@w:     lda     SPRSYS
        bmi     @w
        lda     __sa_flag
        bmi     @neg
        lda     MATHD           ; quotient bits 0..7
        ldx     MATHC           ; quotient bits 8..15
        rts
@neg:   lda     MATHD
        ldx     MATHC
        jmp     negax           ; result = -result

; ===========================================================================
; Unsigned modulo.  r = suzy_umod_start(n,d) ... suzy_umod_result()
;   entry: d in A/X, n at (sp).
; Two-phase: the DIVIDE overlaps the caller's work; the (n/d)*d multiply and
; the n-(n/d)*d subtraction run at harvest. n and d are saved across the gap.
; Mirrors tossuzyumodax.
; ===========================================================================

_suzy_umod_start:
        sta     __sa_d          ; divisor low  (saved for the multiply)
        sta     tmp1
        stx     __sa_d+1        ; divisor high (saved)
        stx     tmp2
        lda     (sp)            ; n low  (saved for the subtraction)
        sta     __sa_n
        ldy     #1
        lda     (sp),y          ; n high (saved)
        sta     __sa_n+1
        lda     __sprsys        ; sign-math/accumulate off for the later
        and     #$3F            ; multiply; keep sprite control bits. Set now;
        sta     SPRSYS          ; the overlap contract forbids SPRSYS writes
                                ; between start and result, so it persists.
        lda     tmp2            ; divisor high zero -> narrow (2.6.3)
        bne     @wide
        stz     MATHP           ; (d<<8) low  = 0
        lda     tmp1
        sta     MATHN           ; (d<<8) high = divisor low byte
        stz     MATHH           ; dividend bits 0..7 = 0 (forces MATHG = 0)
        lda     (sp)            ; n low  -> bits 8..15
        sta     MATHG
        ldy     #1
        lda     (sp),y          ; n high -> bits 16..23
        sta     MATHF           ; (forces MATHE = 0)
        stz     MATHE           ; start the divide
        jmp     incsp2
@wide:  lda     tmp1
        sta     MATHP           ; divisor low
        lda     tmp2
        sta     MATHN           ; divisor high
        lda     (sp)            ; dividend low
        sta     MATHH           ; (also forces MATHG = 0)
        ldy     #1
        lda     (sp),y          ; dividend high
        sta     MATHG
        stz     MATHF           ; zero-extend (also forces MATHE = 0)
        stz     MATHE           ; start the divide
        jmp     incsp2

_suzy_umod_result:
@wd:    lda     SPRSYS
        bmi     @wd             ; wait for the divide
        lda     __sa_d          ; quotient is in MATHD/MATHC = the multiply's
        sta     MATHB           ; C/D operand; write divisor to B/A to start
        lda     __sa_d+1        ; (n/d)*d  (also forces MATHA = 0)
        sta     MATHA           ; start the multiply
@wm:    lda     SPRSYS
        bmi     @wm             ; wait for the multiply (~44 ticks)
        sec
        lda     __sa_n          ; remainder = n - (n/d)*d
        sbc     MATHH           ; low byte of (n/d)*d
        sta     tmp1
        lda     __sa_n+1
        sbc     MATHG           ; high byte
        tax
        lda     tmp1
        rts

; ===========================================================================
; Signed modulo.  r = suzy_mod_start(n,d) ... suzy_mod_result()
;   entry: d in A/X, n at (sp).
; Magnitudes of n and d are saved; the remainder takes the sign of n.
; Mirrors tossuzymodax.
; ===========================================================================

_suzy_mod_start:
        cpx     #$80
        bcc     @dpos
        jsr     negax           ; |divisor|
@dpos:  sta     __sa_d          ; |divisor| low  (for the multiply)
        sta     tmp1
        stx     __sa_d+1        ; |divisor| high
        stx     tmp2
        lda     __sprsys        ; accumulate/sign off for the multiply
        and     #$3F
        sta     SPRSYS
        jsr     popptr1         ; dividend -> ptr1 (stack cleaned)
        lda     ptr1+1
        sta     __sa_flag       ; sign of dividend (bit7) -> remainder sign
        bpl     @npos
        lda     #0              ; |dividend| in ptr1
        sec
        sbc     ptr1
        sta     ptr1
        lda     #0
        sbc     ptr1+1
        sta     ptr1+1
@npos:  lda     ptr1
        sta     __sa_n          ; save |dividend| for the subtraction
        lda     ptr1+1
        sta     __sa_n+1
        lda     tmp2            ; |divisor| high zero -> narrow (2.6.3)
        bne     @wide
        stz     MATHP           ; (|d|<<8) low  = 0
        lda     tmp1
        sta     MATHN           ; (|d|<<8) high = |divisor| low byte
        stz     MATHH           ; (forces MATHG = 0)
        lda     ptr1
        sta     MATHG           ; |n| low  -> bits 8..15
        lda     ptr1+1
        sta     MATHF           ; |n| high -> bits 16..23 (forces MATHE = 0)
        stz     MATHE           ; start the divide
        rts
@wide:  lda     tmp1
        sta     MATHP           ; |divisor| low
        lda     tmp2
        sta     MATHN           ; |divisor| high
        lda     ptr1
        sta     MATHH           ; |dividend| low (also forces MATHG = 0)
        lda     ptr1+1
        sta     MATHG           ; |dividend| high
        stz     MATHF           ; zero-extend (also forces MATHE = 0)
        stz     MATHE           ; start the divide
        rts

_suzy_mod_result:
@wd:    lda     SPRSYS
        bmi     @wd             ; wait for the divide
        lda     __sa_d          ; write |divisor| to B/A to start
        sta     MATHB           ; (|n|/|d|)*|d|  (also forces MATHA = 0)
        lda     __sa_d+1
        sta     MATHA           ; start the multiply
@wm:    lda     SPRSYS
        bmi     @wm             ; wait for the multiply
        lda     __sprsys
        sta     SPRSYS          ; restore SPRSYS (mirrors signed suzymod.s)
        sec
        lda     __sa_n          ; |remainder| = |n| - (|n|/|d|)*|d|
        sbc     MATHH
        sta     tmp1
        lda     __sa_n+1
        sbc     MATHG
        tax
        lda     tmp1
        bit     __sa_flag       ; N <- sign of the dividend
        bmi     @neg
        rts
@neg:   jmp     negax           ; remainder takes the dividend's sign

; ===========================================================================
; Multiply (signed == unsigned, low 16 bits).
;   p = suzy_umul_start(a,b) ... suzy_umul_result()
;   entry: b in A/X (rhs), a at (sp) (lhs). No carried state.
; Provided for symmetry; the multiply is so short (~44 ticks) that little
; overlap is possible. Mirrors tossuzymulax.
; ===========================================================================

_suzy_umul_start:
        sta     MATHD           ; rhs low  (also forces MATHC = 0)
        stx     MATHC           ; rhs high
        lda     __sprsys        ; sign-math/accumulate off, keep sprite bits
        and     #$3F
        sta     SPRSYS
        lda     (sp)            ; lhs low
        sta     MATHB           ; (also forces MATHA = 0)
        ldy     #1
        lda     (sp),y          ; lhs high
        sta     MATHA           ; writing MATHA starts the multiply
        jmp     incsp2

_suzy_umul_result:
@w:     lda     SPRSYS
        bmi     @w
        lda     __sprsys
        sta     SPRSYS          ; restore SPRSYS (mirrors suzymul.s)
        lda     MATHH           ; product bits 0..7
        ldx     MATHG           ; product bits 8..15
        rts

; ===========================================================================
; Fused (a*b)/c.  q = suzy_umuldiv_start(a,b,c) ... suzy_umuldiv_result()
;   entry: c in A/X (divisor), b at (sp)+0/1, a at (sp)+2/3.
; The multiply+inter-op poll happen in _start (the multiply gates the divide
; and cannot be overlapped); the DIVIDE runs in the background. All three
; operands are taken at start so the divisor is in place before MATHE is
; rewritten to trigger the divide. Mirrors tossuzyumuldivax.
; ===========================================================================

_suzy_umuldiv_start:
        sta     MATHP           ; divisor low
        stx     MATHN           ; divisor high
        lda     __sprsys        ; sign-math/accumulate off, keep sprite bits
        and     #$3F
        sta     SPRSYS
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
        sta     MATHA           ; start multiply: MATHE..MATHH = a*b
@wm:    lda     SPRSYS
        bmi     @wm             ; wait for the multiply (gates the divide)
        lda     MATHE           ; product MSB, already in place
        sta     MATHE           ; rewrite same value -> starts the divide
        jmp     incsp4          ; drop a,b, return (divide runs in background)

_suzy_umuldiv_result:
@w:     lda     SPRSYS
        bmi     @w
        lda     MATHD           ; quotient bits 0..7
        ldx     MATHC           ; quotient bits 8..15
        rts

; ===========================================================================
; Signed fused (a*b)/c.
;   q = suzy_muldiv_start(a,b,c) ... suzy_muldiv_result()
;   entry: c in A/X, b at (sp)+0/1, a at (sp)+2/3.
; Reduce all three to magnitudes, run the unsigned core, record "negate iff
; an odd number of operands were negative". Mirrors tossuzymuldivax.
; ===========================================================================

_suzy_muldiv_start:
        stx     tmp1            ; seed sign accumulator with c's high byte
        cpx     #$80
        bcc     @cpos
        jsr     negax           ; |c|
@cpos:  sta     MATHP           ; |c| low
        stx     MATHN           ; |c| high
        lda     __sprsys
        and     #$3F
        sta     SPRSYS

        ldy     #1              ; b: fold sign, make positive in place
        lda     (sp),y          ; b high
        eor     tmp1
        sta     tmp1
        lda     (sp),y
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
        ldy     #3              ; a: fold sign, make positive in place
        lda     (sp),y          ; a high
        eor     tmp1
        sta     tmp1
        lda     (sp),y
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
        lda     tmp1
        sta     __sa_flag       ; carry the parity bit to harvest

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
@wm:    lda     SPRSYS
        bmi     @wm             ; wait for the multiply
        lda     MATHE
        sta     MATHE           ; start divide of the 32-bit product
        jmp     incsp4          ; drop a,b, return (divide runs in background)

_suzy_muldiv_result:
@w:     lda     SPRSYS
        bmi     @w
        lda     MATHD           ; |quotient| bits 0..7
        ldx     MATHC           ; |quotient| bits 8..15
        bit     __sa_flag       ; N <- parity of negative operands
        bpl     @done
        jmp     negax           ; result = -result
@done:  rts
