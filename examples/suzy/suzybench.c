/*
** suzybench.c - Suzy hardware math accuracy + speed test for the Atari Lynx.
**
** This fork adds the operators  !*  !/  !%  which route int multiply, divide
** and modulo through Suzy's hardware math unit (see design/LYNX_CODEGEN_DESIGN.md
** section 2.6). This cart answers two questions about them:
**
**   1. Are they CORRECT?  Each Suzy result is compared against the stock
**      software operator (*, /, %), which is the trusted oracle. Coverage is
**      a fixed corner-case cross-product plus a deterministic pseudo-random
**      sweep, for BOTH unsigned and signed operands, so all five runtime
**      routines are exercised (suzymul, suzyudiv, suzyumod, suzydiv, suzymod).
**
**   2. Are they FAST?  Each operation is timed with a free-running Mikey
**      timer (timer 1 at 1 us linked into timers 3 and 5 -> 24-bit) at
**      microsecond resolution, software loop versus Suzy loop, identical
**      operand sequences so loop overhead cancels. Both 8-bit (char) and
**      16-bit (int) operand widths are timed: char operands promote to int,
**      so the Suzy operators still call the 16-bit routines, while the stock
**      software path uses cc65's byte-optimized variants for unsigned char.
**      Microseconds per operation are shown for software and Suzy side by
**      side.
**
** Real Lynx hardware is the authoritative oracle; an emulator is a quick
** pass only and is trustworthy for speed numbers only if it models Suzy
** math timing.
**
** Screens (press A to advance): accuracy, then first-failures if any, then
** 8-bit speed, 16-bit speed, then a constant-operand screen comparing cc65's
** compile-time strength reduction (powers of two -> shifts, modulo -> AND
** mask, small constants -> mulaxN helpers) against the Suzy hardware doing
** the same arithmetic, then loop. Accuracy failures show the first failing
** (a, b, expected, got) triple per operator, in hex.
**
** Build:  cl65 -Osri -o suzybench.lnx suzybench.c
*/

#include <lynx.h>
#include <tgi.h>
#include <joystick.h>
#include <stdio.h>
#include <6502.h>

/* ------------------------------------------------------------------ */
/* Operand corner cases.                                               */
/* ------------------------------------------------------------------ */

static const unsigned ucorner[] = {
    0u, 1u, 2u, 3u, 7u, 0x00FFu, 0x0100u, 0x7FFFu, 0x8000u, 0xFFFFu
};
static const int scorner[] = {
    0, 1, -1, 2, -2, 100, -100, 0x4000, -0x4000, 0x7FFF, (int)0x8000
};
#define NUC (sizeof ucorner / sizeof ucorner[0])
#define NSC (sizeof scorner / sizeof scorner[0])

/* Number of pseudo-random operand pairs per (operator, signedness). */
#define RAND_PAIRS 400U

/* Deterministic 16-bit xorshift; identical sequence every run. */
static unsigned rng;
static void rng_seed (void) { rng = 0xACE1u; }
static unsigned xrnd (void)
{
    rng ^= (unsigned)(rng << 7);
    rng ^= (unsigned)(rng >> 9);
    rng ^= (unsigned)(rng << 8);
    return rng;
}

/* ------------------------------------------------------------------ */
/* Per-operator accuracy bookkeeping.                                  */
/* Index: 0 UMUL 1 UDIV 2 UMOD 3 SMUL 4 SDIV 5 SMOD.                    */
/* ------------------------------------------------------------------ */

#define OP_UMUL 0
#define OP_UDIV 1
#define OP_UMOD 2
#define OP_SMUL 3
#define OP_SDIV 4
#define OP_SMOD 5
#define NOPS    6

static const char* const opname[NOPS] = {
    "UMUL", "UDIV", "UMOD", "SMUL", "SDIV", "SMOD"
};

static unsigned pass_cnt[NOPS];
static unsigned fail_cnt[NOPS];

static unsigned char fail_have[NOPS];
static unsigned fail_a[NOPS];
static unsigned fail_b[NOPS];
static unsigned fail_exp[NOPS];
static unsigned fail_got[NOPS];

static void record (unsigned char op, unsigned a, unsigned b,
                    unsigned expected, unsigned got)
{
    if (expected == got) {
        ++pass_cnt[op];
    } else {
        ++fail_cnt[op];
        if (!fail_have[op]) {
            fail_have[op] = 1;
            fail_a[op]   = a;
            fail_b[op]   = b;
            fail_exp[op] = expected;
            fail_got[op] = got;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Accuracy checks. One function per (operator, signedness): the !op   */
/* token compiles to a specific Suzy routine, so it cannot be passed   */
/* as a parameter. Operands come from runtime values, so the compiler  */
/* cannot constant-fold or strength-reduce them - the Suzy helper is   */
/* always called.                                                      */
/* ------------------------------------------------------------------ */

static void check_umul (unsigned a, unsigned b)
{
    record (OP_UMUL, a, b, (unsigned)(a * b), (unsigned)(a !* b));
}
static void check_udiv (unsigned a, unsigned b)
{
    if (b == 0) return;                 /* div-by-zero handled separately */
    record (OP_UDIV, a, b, (unsigned)(a / b), (unsigned)(a !/ b));
}
static void check_umod (unsigned a, unsigned b)
{
    if (b == 0) return;
    record (OP_UMOD, a, b, (unsigned)(a % b), (unsigned)(a !% b));
}
static void check_smul (int a, int b)
{
    record (OP_SMUL, (unsigned)a, (unsigned)b,
            (unsigned)(a * b), (unsigned)(a !* b));
}
static void check_sdiv (int a, int b)
{
    if (b == 0 || (a == (int)0x8000 && b == -1)) return;   /* skip undefined */
    record (OP_SDIV, (unsigned)a, (unsigned)b,
            (unsigned)(a / b), (unsigned)(a !/ b));
}
static void check_smod (int a, int b)
{
    if (b == 0 || (a == (int)0x8000 && b == -1)) return;   /* skip undefined */
    record (OP_SMOD, (unsigned)a, (unsigned)b,
            (unsigned)(a % b), (unsigned)(a !% b));
}

static void run_accuracy (void)
{
    unsigned char i, j;
    unsigned n;
    unsigned char op;

    for (op = 0; op < NOPS; ++op) {
        pass_cnt[op] = fail_cnt[op] = 0;
        fail_have[op] = 0;
    }

    /* Corner-case cross-products. */
    for (i = 0; i < NUC; ++i) {
        for (j = 0; j < NUC; ++j) {
            check_umul (ucorner[i], ucorner[j]);
            check_udiv (ucorner[i], ucorner[j]);
            check_umod (ucorner[i], ucorner[j]);
        }
    }
    for (i = 0; i < NSC; ++i) {
        for (j = 0; j < NSC; ++j) {
            check_smul (scorner[i], scorner[j]);
            check_sdiv (scorner[i], scorner[j]);
            check_smod (scorner[i], scorner[j]);
        }
    }

    /* Deterministic random sweep. */
    rng_seed ();
    for (n = 0; n < RAND_PAIRS; ++n) {
        unsigned a = xrnd ();
        unsigned b = xrnd ();
        check_umul (a, b);
        check_udiv (a, b);
        check_umod (a, b);
        check_smul ((int)a, (int)b);
        check_sdiv ((int)a, (int)b);
        check_smod ((int)a, (int)b);
    }
}

/* Divide-by-zero is undefined in C, so it is reported informationally
** (not pass/fail). These read the documented Suzy contract directly. */
static unsigned div0_udiv, div0_umod;
static void run_div0 (void)
{
    unsigned a = 0x1234u, b = 0u;
    div0_udiv = (unsigned)(a !/ b);
    div0_umod = (unsigned)(a !% b);
}

/* ------------------------------------------------------------------ */
/* Mikey timer: timer 1 (1 us) linked into timer 3 linked into timer 5 */
/* -> 24-bit, 1 us, free-running down-counter (range ~16.7 s). A 16-bit */
/* counter wraps at 65536 us, which the slowest loops here exceed, so   */
/* the third stage is required to avoid rollover. Timers 0/2 (video)    */
/* and 7 (sound) are avoided; 1, 3 and 5 are free timers in link group  */
/* B (1 -> 3 -> 5 -> 7).                                                 */
/* ------------------------------------------------------------------ */

#define TIM1_BKUP (*(volatile unsigned char*)0xFD04)
#define TIM1_CTLA (*(volatile unsigned char*)0xFD05)
#define TIM1_CNT  (*(volatile unsigned char*)0xFD06)
#define TIM3_BKUP (*(volatile unsigned char*)0xFD0C)
#define TIM3_CTLA (*(volatile unsigned char*)0xFD0D)
#define TIM3_CNT  (*(volatile unsigned char*)0xFD0E)
#define TIM5_BKUP (*(volatile unsigned char*)0xFD14)
#define TIM5_CTLA (*(volatile unsigned char*)0xFD15)
#define TIM5_CNT  (*(volatile unsigned char*)0xFD16)

#define T_ENABLE_COUNT  0x08
#define T_ENABLE_RELOAD 0x10
#define T_CLK_1US       0x00
#define T_CLK_LINK      0x07

static void timer_init (void)
{
    TIM5_BKUP = 0xFF;
    TIM3_BKUP = 0xFF;
    TIM1_BKUP = 0xFF;
    TIM5_CTLA = T_ENABLE_COUNT | T_ENABLE_RELOAD | T_CLK_LINK;
    TIM3_CTLA = T_ENABLE_COUNT | T_ENABLE_RELOAD | T_CLK_LINK;
    TIM1_CTLA = T_ENABLE_COUNT | T_ENABLE_RELOAD | T_CLK_1US;
}

/* Read the three cascaded down-counters consistently: the high and mid
** bytes are re-read after the low byte, and the read is retried if either
** changed (i.e. a borrow rippled up) during the window. */
static unsigned long timer_read (void)
{
    unsigned char hi, mid, lo, mid2, hi2;
    do {
        hi   = TIM5_CNT;
        mid  = TIM3_CNT;
        lo   = TIM1_CNT;
        mid2 = TIM3_CNT;
        hi2  = TIM5_CNT;
    } while (hi != hi2 || mid != mid2);
    return ((unsigned long)hi << 16) | ((unsigned)mid << 8) | lo;
}

/* The timer counts down, so elapsed = (start - end) & 0xFFFFFF. */
#define ELAPSED(start, end) (((start) - (end)) & 0xFFFFFFUL)


#define BITER 1000U

static unsigned sink;

/* Operand generation for the timed loops.
**
** Both operands are swept pseudo-uniformly across the full 16-bit range by
** stepping with large odd strides (an odd stride is coprime to 65536, so it
** visits every value with full period). This matters for division timing,
** which is divisor-dependent: Suzy's divide takes 176 + 14*N ticks where N
** is the number of leading zeros in the divisor, and the software udiv16
** uses a faster 16x8 sub-path when the divisor is < 256. Neither divide is
** sensitive to the dividend (both signed and unsigned software divide call
** the same constant-time udiv16; there is no dividend<divisor early-exit),
** so the dividend is simply swept widely as well.
**
** The earlier a+=3/b+=7 ramp under-sampled the divisor (monotonic and
** correlated with the dividend), so the divide/modulo numbers were not
** representative. Operands stay runtime-variable and the divisor is never a
** compile-time constant, so no multiply/divide is strength-reduced to a
** shift. The stepping is two adds and a zero-guard, identical on the
** software and Suzy side, so it cancels in the ratio. For the signed loops
** the operands wrap through negative values, exercising the sign-fixup
** paths; the divisor is guarded to never be zero. */

#define INITA 0x1234U
#define INITB 0x0ABCU
#define STEPU(a, b) do { (a) += 0x2B7DU; (b) += 0x1D4FU; if ((b) == 0U) (b) = 1U; } while (0)
#define STEPS(a, b) do { (a) = (int)((unsigned)(a) + 0x2B7DU); (b) = (int)((unsigned)(b) + 0x1D4FU); if ((b) == 0) (b) = 1; } while (0)

static unsigned long t_sw_umul (void)
{
    unsigned a = INITA, b = INITB, i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += a * b;
        STEPU (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_hw_umul (void)
{
    unsigned a = INITA, b = INITB, i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += a !* b;
        STEPU (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_sw_smul (void)
{
    int a = INITA, b = INITB; unsigned i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += (unsigned)(a * b);
        STEPS (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_hw_smul (void)
{
    int a = INITA, b = INITB; unsigned i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += (unsigned)(a !* b);
        STEPS (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_sw_udiv (void)
{
    unsigned a = INITA, b = INITB, i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += a / b;
        STEPU (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_hw_udiv (void)
{
    unsigned a = INITA, b = INITB, i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += a !/ b;
        STEPU (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_sw_umod (void)
{
    unsigned a = INITA, b = INITB, i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += a % b;
        STEPU (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_hw_umod (void)
{
    unsigned a = INITA, b = INITB, i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += a !% b;
        STEPU (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_sw_sdiv (void)
{
    int a = INITA, b = INITB; unsigned i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += (unsigned)(a / b);
        STEPS (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_hw_sdiv (void)
{
    int a = INITA, b = INITB; unsigned i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += (unsigned)(a !/ b);
        STEPS (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_sw_smod (void)
{
    int a = INITA, b = INITB; unsigned i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += (unsigned)(a % b);
        STEPS (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_hw_smod (void)
{
    int a = INITA, b = INITB; unsigned i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += (unsigned)(a !% b);
        STEPS (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}

/* 8-bit (char) variants. char operands promote to int for arithmetic, so the
** Suzy operators still call the 16-bit tossuzy* routines (with a pusha0/pushax
** promotion). The stock software path, by contrast, uses cc65's byte-optimized
** runtime variants for UNSIGNED char (tosumula0/tosudiva0/tosumoda0); signed
** char uses the same full 16-bit routines as int on both sides. The result is
** cast back to char each iteration, matching real 8-bit code. */

#define INIT8A 0x12U
#define INIT8B 0x57U
#define STEP8U(a, b) do { (a) = (unsigned char)((a) + 0x53U); (b) = (unsigned char)((b) + 0x2DU); if ((b) == 0U) (b) = 1U; } while (0)
#define STEP8S(a, b) do { (a) = (signed char)((a) + 0x53);   (b) = (signed char)((b) + 0x2D);   if ((b) == 0)  (b) = 1;  } while (0)

static unsigned long t_sw_umul8 (void)
{
    unsigned char a = INIT8A, b = INIT8B; unsigned i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += (unsigned char)(a * b);
        STEP8U (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_hw_umul8 (void)
{
    unsigned char a = INIT8A, b = INIT8B; unsigned i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += (unsigned char)(a !* b);
        STEP8U (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_sw_smul8 (void)
{
    signed char a = INIT8A, b = INIT8B; unsigned i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += (unsigned)(signed char)(a * b);
        STEP8S (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_hw_smul8 (void)
{
    signed char a = INIT8A, b = INIT8B; unsigned i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += (unsigned)(signed char)(a !* b);
        STEP8S (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_sw_udiv8 (void)
{
    unsigned char a = INIT8A, b = INIT8B; unsigned i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += (unsigned char)(a / b);
        STEP8U (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_hw_udiv8 (void)
{
    unsigned char a = INIT8A, b = INIT8B; unsigned i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += (unsigned char)(a !/ b);
        STEP8U (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_sw_sdiv8 (void)
{
    signed char a = INIT8A, b = INIT8B; unsigned i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += (unsigned)(signed char)(a / b);
        STEP8S (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_hw_sdiv8 (void)
{
    signed char a = INIT8A, b = INIT8B; unsigned i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += (unsigned)(signed char)(a !/ b);
        STEP8S (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_sw_umod8 (void)
{
    unsigned char a = INIT8A, b = INIT8B; unsigned i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += (unsigned char)(a % b);
        STEP8U (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_hw_umod8 (void)
{
    unsigned char a = INIT8A, b = INIT8B; unsigned i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += (unsigned char)(a !% b);
        STEP8U (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_sw_smod8 (void)
{
    signed char a = INIT8A, b = INIT8B; unsigned i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += (unsigned)(signed char)(a % b);
        STEP8S (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_hw_smod8 (void)
{
    signed char a = INIT8A, b = INIT8B; unsigned i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += (unsigned)(signed char)(a !% b);
        STEP8S (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}

/* ------------------------------------------------------------------ */
/* Constant-operand optimizations vs Suzy.                             */
/*                                                                     */
/* When the second operand is a compile-time constant, cc65 replaces   */
/* the multiply/divide/modulo with cheaper code: powers of two become  */
/* shifts (shlaxN / shraxN) or, for modulo, an AND mask; several small  */
/* non-power-of-two multipliers use special shift-add helpers          */
/* (mulax3 / mulax5 / mulax10). The Suzy operators apply the SAME       */
/* strength reduction, so a constant !* 8 also becomes a shift and      */
/* never reaches the hardware. To time the hardware path against these  */
/* optimized sequences, the Suzy side multiplies/divides by a runtime   */
/* global (kN below) holding the same value, which cc65 cannot fold     */
/* into a shift. The SOFT column is therefore the optimized constant    */
/* code and the SUZY column is the hardware doing the same arithmetic.  */
/* ------------------------------------------------------------------ */

static unsigned k2 = 2, k8 = 8, k3 = 3, k10 = 10;

#define STEPK(a) do { (a) += 0x2B7DU; } while (0)

static unsigned long t_sw_x2 (void)
{
    unsigned a = INITA, i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += a * 2u;
        STEPK (a);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_hw_x2 (void)
{
    unsigned a = INITA, i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += a !* k2;
        STEPK (a);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_sw_x8 (void)
{
    unsigned a = INITA, i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += a * 8u;
        STEPK (a);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_hw_x8 (void)
{
    unsigned a = INITA, i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += a !* k8;
        STEPK (a);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_sw_d8 (void)
{
    unsigned a = INITA, i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += a / 8u;
        STEPK (a);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_hw_d8 (void)
{
    unsigned a = INITA, i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += a !/ k8;
        STEPK (a);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_sw_m8 (void)
{
    unsigned a = INITA, i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += a % 8u;
        STEPK (a);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_hw_m8 (void)
{
    unsigned a = INITA, i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += a !% k8;
        STEPK (a);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_sw_x3 (void)
{
    unsigned a = INITA, i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += a * 3u;
        STEPK (a);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_hw_x3 (void)
{
    unsigned a = INITA, i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += a !* k3;
        STEPK (a);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_sw_x10 (void)
{
    unsigned a = INITA, i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += a * 10u;
        STEPK (a);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long t_hw_x10 (void)
{
    unsigned a = INITA, i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += a !* k10;
        STEPK (a);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}

#define NSPEED 6
#define NWIDTH 2                /* 0 = 8-bit, 1 = 16-bit */
static const char* const speedname[NSPEED] = {
    "UMUL", "SMUL", "UDIV", "SDIV", "UMOD", "SMOD"
};
static unsigned long sw_us[NWIDTH][NSPEED];
static unsigned long hw_us[NWIDTH][NSPEED];

#define NOPT 6
static const char* const optname[NOPT] = {
    "x2", "x8", "/8", "%8", "x3", "x10"
};
static unsigned long sw_opt[NOPT];
static unsigned long hw_opt[NOPT];

static void run_speed (void)
{
    /* Interrupts off during each timed loop removes IRQ jitter; the
    ** Mikey timer runs in hardware regardless of the CPU mask. */
    SEI ();
    sw_us[0][0] = t_sw_umul8 (); hw_us[0][0] = t_hw_umul8 ();
    sw_us[0][1] = t_sw_smul8 (); hw_us[0][1] = t_hw_smul8 ();
    sw_us[0][2] = t_sw_udiv8 (); hw_us[0][2] = t_hw_udiv8 ();
    sw_us[0][3] = t_sw_sdiv8 (); hw_us[0][3] = t_hw_sdiv8 ();
    sw_us[0][4] = t_sw_umod8 (); hw_us[0][4] = t_hw_umod8 ();
    sw_us[0][5] = t_sw_smod8 (); hw_us[0][5] = t_hw_smod8 ();
    sw_us[1][0] = t_sw_umul ();  hw_us[1][0] = t_hw_umul ();
    sw_us[1][1] = t_sw_smul ();  hw_us[1][1] = t_hw_smul ();
    sw_us[1][2] = t_sw_udiv ();  hw_us[1][2] = t_hw_udiv ();
    sw_us[1][3] = t_sw_sdiv ();  hw_us[1][3] = t_hw_sdiv ();
    sw_us[1][4] = t_sw_umod ();  hw_us[1][4] = t_hw_umod ();
    sw_us[1][5] = t_sw_smod ();  hw_us[1][5] = t_hw_smod ();
    sw_opt[0] = t_sw_x2 ();   hw_opt[0] = t_hw_x2 ();
    sw_opt[1] = t_sw_x8 ();   hw_opt[1] = t_hw_x8 ();
    sw_opt[2] = t_sw_d8 ();   hw_opt[2] = t_hw_d8 ();
    sw_opt[3] = t_sw_m8 ();   hw_opt[3] = t_hw_m8 ();
    sw_opt[4] = t_sw_x3 ();   hw_opt[4] = t_hw_x3 ();
    sw_opt[5] = t_sw_x10 ();  hw_opt[5] = t_hw_x10 ();
    CLI ();
}

/* ------------------------------------------------------------------ */
/* Display.                                                            */
/* ------------------------------------------------------------------ */

static char buf[28];

static void wait_a (void)
{
    while (!(joy_read (JOY_1) & JOY_BTN_1_MASK)) {}
    while (joy_read (JOY_1) & JOY_BTN_1_MASK) {}
}

static void show_accuracy (void)
{
    unsigned char op;
    unsigned char y;
    unsigned char anyfail = 0;

    tgi_setcolor (COLOR_BLACK);
    tgi_clear ();
    tgi_setcolor (COLOR_WHITE);
    tgi_outtextxy (2, 2, "SUZY MATH ACCURACY");
    tgi_setcolor (COLOR_GREEN);
    tgi_outtextxy (2, 14, "       PASS   FAIL");
    tgi_setcolor (COLOR_WHITE);

    y = 24;
    for (op = 0; op < NOPS; ++op) {
        if (fail_cnt[op]) {
            tgi_setcolor (COLOR_RED);
            anyfail = 1;
        } else {
            tgi_setcolor (COLOR_WHITE);
        }
        sprintf (buf, "%-4s %6u %6u", opname[op], pass_cnt[op], fail_cnt[op]);
        tgi_outtextxy (2, y, buf);
        y += 9;
    }

    tgi_setcolor (COLOR_GREY);
    sprintf (buf, "DIV0 !/=%04X !%%=%04X", div0_udiv, div0_umod);
    tgi_outtextxy (2, y + 2, buf);

    tgi_setcolor (anyfail ? COLOR_RED : COLOR_YELLOW);
    tgi_outtextxy (2, 92, anyfail ? "A = FAILURES" : "A = SPEED");

    tgi_updatedisplay ();
    while (tgi_busy ()) {}
    wait_a ();
}

static void show_failures (void)
{
    unsigned char op;
    unsigned char y;

    tgi_setcolor (COLOR_BLACK);
    tgi_clear ();
    tgi_setcolor (COLOR_RED);
    tgi_outtextxy (2, 2, "FIRST FAILURES");
    tgi_setcolor (COLOR_WHITE);

    y = 16;
    for (op = 0; op < NOPS; ++op) {
        if (!fail_have[op]) continue;
        sprintf (buf, "%s a=%04X b=%04X",
                 opname[op], fail_a[op], fail_b[op]);
        tgi_outtextxy (2, y, buf);
        y += 9;
        sprintf (buf, "  exp=%04X got=%04X", fail_exp[op], fail_got[op]);
        tgi_outtextxy (2, y, buf);
        y += 11;
    }

    tgi_setcolor (COLOR_YELLOW);
    tgi_outtextxy (2, 92, "A = SPEED");
    tgi_updatedisplay ();
    while (tgi_busy ()) {}
    wait_a ();
}

/* Format tenths-of-a-unit (e.g. 306) as "30.6" into d. */
static void fmt10 (char* d, unsigned tenths)
{
    sprintf (d, "%u.%u", tenths / 10u, tenths % 10u);
}

static char cs[8], ch[8];

/* SOFT and SUZY are microseconds per operation (one decimal). w selects the
** operand width set: 0 = 8-bit (char), 1 = 16-bit (int). The next-screen
** prompt depends on whether another width follows. */
static void show_speed (unsigned char w, const char* title, const char* prompt)
{
    unsigned char i;
    unsigned char y;

    tgi_setcolor (COLOR_BLACK);
    tgi_clear ();
    tgi_setcolor (COLOR_WHITE);
    tgi_outtextxy (2, 2, title);
    tgi_setcolor (COLOR_GREEN);
    tgi_outtextxy (2, 14, "        SOFT   SUZY");
    tgi_setcolor (COLOR_WHITE);

    y = 24;
    for (i = 0; i < NSPEED; ++i) {
        unsigned s10 = (unsigned)(sw_us[w][i] * 10u / BITER);
        unsigned h10 = (unsigned)(hw_us[w][i] * 10u / BITER);
        fmt10 (cs, s10);
        fmt10 (ch, h10);
        sprintf (buf, "%-4s %7s %6s", speedname[i], cs, ch);
        tgi_outtextxy (2, y, buf);
        y += 9;
    }

    tgi_setcolor (COLOR_YELLOW);
    tgi_outtextxy (2, 92, prompt);
    tgi_updatedisplay ();
    while (tgi_busy ()) {}
    wait_a ();
}

/* Constant-operand optimizations (shift / mask / small-const helper) vs the
** Suzy hardware doing the same arithmetic. SOFT is the strength-reduced
** constant code; SUZY uses a runtime operand so the hardware path runs. */
static void show_opt (const char* prompt)
{
    unsigned char i;
    unsigned char y;

    tgi_setcolor (COLOR_BLACK);
    tgi_clear ();
    tgi_setcolor (COLOR_WHITE);
    tgi_outtextxy (2, 2, "CONST-OPT us/op");
    tgi_setcolor (COLOR_GREEN);
    tgi_outtextxy (2, 14, "        SOFT   SUZY");
    tgi_setcolor (COLOR_WHITE);

    y = 24;
    for (i = 0; i < NOPT; ++i) {
        unsigned s10 = (unsigned)(sw_opt[i] * 10u / BITER);
        unsigned h10 = (unsigned)(hw_opt[i] * 10u / BITER);
        fmt10 (cs, s10);
        fmt10 (ch, h10);
        sprintf (buf, "%-4s %7s %6s", optname[i], cs, ch);
        tgi_outtextxy (2, y, buf);
        y += 9;
    }

    tgi_setcolor (COLOR_GREY);
    tgi_outtextxy (2, 78, "SOFT = SHIFT/MASK");
    tgi_setcolor (COLOR_YELLOW);
    tgi_outtextxy (2, 92, prompt);
    tgi_updatedisplay ();
    while (tgi_busy ()) {}
    wait_a ();
}

static unsigned char any_failures (void)
{
    unsigned char op;
    for (op = 0; op < NOPS; ++op)
        if (fail_cnt[op]) return 1;
    return 0;
}

static void show_progress (const char* msg)
{
    tgi_setcolor (COLOR_BLACK);
    tgi_clear ();
    tgi_setcolor (COLOR_WHITE);
    tgi_outtextxy (2, 10, "SUZY MATH BENCH");
    tgi_setcolor (COLOR_YELLOW);
    tgi_outtextxy (2, 40, msg);
    tgi_updatedisplay ();
    while (tgi_busy ()) {}
}

void main (void)
{
    tgi_init ();
    CLI ();
    while (tgi_busy ()) {}
    tgi_setpalette (tgi_getdefpalette ());

    timer_init ();

    for (;;) {
        show_progress ("CHECKING...");
        run_accuracy ();
        run_div0 ();

        show_progress ("TIMING...");
        run_speed ();

        show_accuracy ();
        if (any_failures ())
            show_failures ();
        show_speed (0, "8-BIT SPEED us/op", "A = 16BIT");
        show_speed (1, "16-BIT SPEED us/op", "A = CONST-OPT");
        show_opt ("A = RUN AGAIN");
    }
}
