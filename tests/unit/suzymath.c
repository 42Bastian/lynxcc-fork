/*
** SPDX-License-Identifier: MPL-2.0
**
** This Source Code Form is subject to the terms of the Mozilla Public License,
** v. 2.0. If a copy of the MPL was not distributed with this file, You can
** obtain one at https://mozilla.org/MPL/2.0/.
**
** Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
*/

/*
** Host-built unit checks for the Suzy hardware-math contracts.
**
** The Suzy math routines under libraries/math/ rely on a handful of arithmetic
** invariants documented in design/LYNX_SUZY_*_DESIGN.md. Those routines are
** 6502 + Suzy-register code and cannot run on the host, but the *algebra* they
** depend on can: this program reimplements each routine's algorithm shape in C
** (the `model_*` functions below) and sweeps it against a trusted reference
** (plain C / 64-bit math). A mismatch means an invariant the asm leans on has
** been broken — exactly the regression the asm review would otherwise have to
** catch by hand.
**
** Contracts checked:
**   1. udiv/umod   — software remainder recompute  r = n - (n/d)*d  == n % d.
**   2. divnorm     — shifting dividend AND divisor <<8 (the d<256 fast path,
**                    design/LYNX_SUZY_DIVNORM) leaves the quotient unchanged and
**                    scales the remainder by 256.
**   3. muldiv      — a !* b !/ c with a 32-bit intermediate product equals
**                    floor(a*b/c); a naive 16-bit product would overflow.
**   4. signed      — magnitude-divide + sign-fixup reproduces C truncated
**                    division (quotient sign = operands differ; remainder sign
**                    follows the dividend).
**
** Build/run:  make    (see Makefile); exits nonzero on the first failing sweep.
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

static long failures = 0;
static long checks   = 0;

static void fail (const char *what, long a, long b, long c,
                  long got, long want)
{
    if (failures < 10)
        fprintf (stderr,
                 "FAIL %s: a=%ld b=%ld c=%ld -> got %ld, want %ld\n",
                 what, a, b, c, got, want);
    ++failures;
}

/* ---- 1. unsigned divide + software-mod recompute ---------------------- */
/* The hardware remainder registers are buggy and unused; modulo is computed
** as n - (n/d)*d (design async-math sec. 4). */
static void check_udiv_umod (void)
{
    uint32_t n, d;
    for (d = 1; d <= 0xFFFFu; d += (d < 300 ? 1 : 257)) {
        for (n = 0; n <= 0xFFFFu; n += (n < 600 ? 1 : 521)) {
            uint16_t q     = (uint16_t)(n / d);
            uint16_t r_sw  = (uint16_t)(n - (uint32_t)q * d);   /* runtime mod */
            uint16_t r_ref = (uint16_t)(n % d);
            ++checks;
            if (r_sw != r_ref) fail ("umod", n, d, 0, r_sw, r_ref);
            if ((uint32_t)q * d + r_sw != n) fail ("udiv-id", n, d, 0, 0, 0);
        }
    }
}

/* ---- 2. divisor-normalisation fast path (d < 256) --------------------- */
/* When the divisor is < 256 the routines shift the (up to 16-bit) dividend and
** the divisor each left by 8, divide, and recover the remainder by >>8. The
** shifted divisor must still fit 16 bits (=> d < 256) and the shifted dividend
** must fit the 32-bit dividend register (n is 16-bit, so n<<8 is <= 24-bit).
** The quotient is invariant and the remainder scales by 256. */
static void check_divnorm (void)
{
    uint32_t n, d;
    for (d = 1; d < 256; ++d) {
        for (n = 0; n <= 0xFFFFu; n += (n < 600 ? 1 : 519)) {
            uint32_t nn = n << 8, dd = d << 8;           /* normalised operands */
            uint16_t q_norm = (uint16_t)(nn / dd);
            uint16_t r_norm = (uint16_t)((nn % dd) >> 8);
            uint16_t q_ref  = (uint16_t)(n / d);
            uint16_t r_ref  = (uint16_t)(n % d);
            ++checks;
            if (dd > 0xFFFFu)            fail ("divnorm-width", n, d, 0, dd, 0);
            if (q_norm != q_ref)         fail ("divnorm-q", n, d, 0, q_norm, q_ref);
            if (r_norm != r_ref)         fail ("divnorm-r", n, d, 0, r_norm, r_ref);
        }
    }
}

/* ---- 3. fused muldiv: 32-bit intermediate product --------------------- */
/* a !* b !/ c chains the 16x16->32 multiply into the 32/16->16 divide so the
** full product is the dividend; this is what stops !* from overflowing 16 bits
** (qbertroot / muldiv design). The 16-bit result equals floor(a*b/c). We also
** assert that at least one swept triple WOULD differ under a naive 16-bit
** product, proving the wide intermediate is load-bearing. */
static void check_muldiv (void)
{
    uint32_t a, b, c;
    long overflow_cases = 0;
    for (a = 0; a <= 0xFFFFu; a += 1103) {
        for (b = 0; b <= 0xFFFFu; b += 1097) {
            uint32_t prod = a * b;                       /* 32-bit, exact */
            for (c = 1; c <= 0xFFFFu; c += 2069) {
                uint16_t q_model = (uint16_t)(prod / c);          /* runtime */
                uint64_t q_ref64 = (uint64_t)a * b / c;          /* trusted */
                ++checks;
                /* Within the 16-bit result domain the model must match. */
                if ((uint16_t)q_ref64 != q_model)
                    fail ("muldiv", a, b, c, q_model, (long)(uint16_t)q_ref64);
                /* Would a naive 16-bit product have diverged here? */
                if (prod > 0xFFFFu) {
                    uint16_t q_naive = (uint16_t)((uint16_t)prod / c);
                    if (q_naive != q_model) ++overflow_cases;
                }
            }
        }
    }
    if (overflow_cases == 0)
        fail ("muldiv-overflow-proof", 0, 0, 0, 0, 1);   /* sweep too weak */
}

/* ---- 4. signed divide / mod sign-fixup -------------------------------- */
/* Signed routines divide magnitudes then fix the sign: quotient negative when
** the operand signs differ, remainder sign follows the dividend. That must
** reproduce C's truncate-toward-zero semantics, which cc65 emits. */
static long imagdiv (long n, long d)         /* magnitude divide + sign fixup */
{
    unsigned long un = n < 0 ? (unsigned long)(-n) : (unsigned long)n;
    unsigned long ud = d < 0 ? (unsigned long)(-d) : (unsigned long)d;
    unsigned long q  = un / ud;
    return ((n < 0) != (d < 0)) ? -(long)q : (long)q;
}
static long imagmod (long n, long d)
{
    unsigned long un = n < 0 ? (unsigned long)(-n) : (unsigned long)n;
    unsigned long ud = d < 0 ? (unsigned long)(-d) : (unsigned long)d;
    unsigned long r  = un % ud;
    return (n < 0) ? -(long)r : (long)r;
}
static void check_signed (void)
{
    long n, d;
    for (n = -32768; n <= 32767; n += 137) {
        for (d = -32768; d <= 32767; d += 139) {
            if (d == 0) continue;
            ++checks;
            if (imagdiv (n, d) != n / d) fail ("sdiv", n, d, 0, imagdiv (n, d), n / d);
            if (imagmod (n, d) != n % d) fail ("smod", n, d, 0, imagmod (n, d), n % d);
        }
    }
}

int main (void)
{
    check_udiv_umod ();
    check_divnorm ();
    check_muldiv ();
    check_signed ();

    printf ("suzymath: %ld checks, %ld failures\n", checks, failures);
    if (failures) {
        fprintf (stderr, "suzymath: FAILED\n");
        return 1;
    }
    printf ("suzymath: OK\n");
    return 0;
}
