/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in this directory.
*/

/*
** muldivtest.c - On-hardware correctness test for this fork's Suzy hardware
** math operators (see design/LYNX_CODEGEN_DESIGN.md 2.6).
**
** Six sweeps over a corner-value table, each comparing a Suzy hardware result
** against an independent software reference:
**
**   MDV  a !* b !/ c   vs  (int)(((long)a*b)/c)     (fused, 32-bit product)
**   DIV  a !/ b        vs  a / b                     (suzy?div.s, normalized)
**   MOD  a !% b        vs  a % b                     (suzy?mod.s,  normalized)
**
** each in a signed (S) and unsigned (U) variant. The MDV reference uses long
** math so it shares no code with the Suzy path; the DIV/MOD references use the
** stock 16-bit software '/' and '%' routines, so a mismatch flags a divergence
** between the normalized Suzy divide (design doc 2.6.3) and known-good software.
**
** The table includes divisors 1, 127, 255 (narrow, <256 -> normalized path),
** 256 (boundary) and -1->$FFFF / 30000 / 32767 (wide path), exercising both
** sides of the small-divisor shift.
**
** The screen shows pass/total per sweep and the first failing case, if any.
** "ALL PASS" means every case matched. Press A to re-run.
**
** Build:  cl65 -O -o muldivtest.lnx muldivtest.c
*/

#include <lynx/lynx.h>
#include <lynx/gfx.h>
#include <lynx/joystick.h>
#include <stdio.h>
#include <6502.h>

static char buf[21];

/* Corner values: 0, +/-1, +/-2, byte edges, +/-1000, +/-30000 and the
** 16-bit signed extremes. The same magnitudes serve the unsigned sweeps. */
static const int sval[] = {
    0, 1, -1, 2, -2, 127, -128, 255, 256, -256,
    1000, -1000, 30000, -30000, 32767, -32768
};
#define NV (sizeof (sval) / sizeof (sval[0]))

/* First-failure capture, shared across all sweeps. */
static unsigned char anyfail;
static char fbuf1[21], fbuf2[21];

static void cap (const char* tag, long a, long b, long c, long got, long exp)
{
    if (anyfail) {
        return;
    }
    anyfail = 1;
    sprintf (fbuf1, "%s %ld %ld %ld", tag, a, b, c);
    sprintf (fbuf2, "g%ld e%ld", got, exp);
}

static void row (unsigned char y, const char* tag,
                 unsigned fail, unsigned pass, unsigned total)
{
    gfx_setcolor (fail ? COLOR_RED : COLOR_GREEN);
    sprintf (buf, "%s %u/%u", tag, pass, total);
    gfx_outtextxy (4, y, buf);
}

int main (void)
{
    unsigned i, j, k;
    /* per-sweep counters */
    unsigned mds_t, mds_p, mdu_t, mdu_p;
    unsigned dvs_t, dvs_p, dvu_t, dvu_p;
    unsigned mos_t, mos_p, mou_t, mou_p;
    unsigned char mds_f, mdu_f, dvs_f, dvu_f, mos_f, mou_f;

    gfx_init ();
    CLI ();
    while (gfx_busy ()) {}
    gfx_setpalette (gfx_getdefpalette ());

    for (;;) {

        gfx_setcolor (COLOR_BLACK);
        gfx_clear ();
        gfx_setcolor (COLOR_YELLOW);
        gfx_outtextxy (4, 4, "RUNNING...");
        gfx_updatedisplay ();
        while (gfx_busy ()) {}

        anyfail = 0;
        mds_t = mds_p = mdu_t = mdu_p = 0;
        dvs_t = dvs_p = dvu_t = dvu_p = 0;
        mos_t = mos_p = mou_t = mou_p = 0;
        mds_f = mdu_f = dvs_f = dvu_f = mos_f = mou_f = 0;

        /* -------- fused multiply-divide: triple loops -------- */
        for (i = 0; i < NV; ++i) {
            for (j = 0; j < NV; ++j) {
                for (k = 0; k < NV; ++k) {
                    int a = sval[i], b = sval[j], c = sval[k];
                    if (c == 0) {
                        continue;
                    }
                    {
                        int sg = a !* b !/ c;
                        int se = (int) (((long) a * (long) b) / (long) c);
                        ++mds_t;
                        if (sg == se) { ++mds_p; }
                        else { mds_f = 1; cap ("MDS", a, b, c, sg, se); }
                    }
                    {
                        unsigned ua = (unsigned) a, ub = (unsigned) b,
                                 uc = (unsigned) c;
                        unsigned ug = ua !* ub !/ uc;
                        unsigned ue = (unsigned)
                            (((unsigned long) ua * (unsigned long) ub)
                             / (unsigned long) uc);
                        ++mdu_t;
                        if (ug == ue) { ++mdu_p; }
                        else { mdu_f = 1; cap ("MDU", ua, ub, uc, ug, ue); }
                    }
                }
            }
        }

        /* -------- plain divide and modulo: double loops -------- */
        for (i = 0; i < NV; ++i) {
            for (j = 0; j < NV; ++j) {
                int a = sval[i], b = sval[j];
                unsigned ua = (unsigned) a, ub = (unsigned) b;
                if (b == 0) {
                    continue;
                }
                {
                    int g = a !/ b, e = a / b;       /* Suzy vs software */
                    ++dvs_t;
                    if (g == e) { ++dvs_p; }
                    else { dvs_f = 1; cap ("DVS", a, b, 0, g, e); }
                }
                {
                    unsigned g = ua !/ ub, e = ua / ub;
                    ++dvu_t;
                    if (g == e) { ++dvu_p; }
                    else { dvu_f = 1; cap ("DVU", ua, ub, 0, g, e); }
                }
                {
                    int g = a !% b, e = a % b;
                    ++mos_t;
                    if (g == e) { ++mos_p; }
                    else { mos_f = 1; cap ("MOS", a, b, 0, g, e); }
                }
                {
                    unsigned g = ua !% ub, e = ua % ub;
                    ++mou_t;
                    if (g == e) { ++mou_p; }
                    else { mou_f = 1; cap ("MOU", ua, ub, 0, g, e); }
                }
            }
        }

        /* -------- results -------- */
        gfx_setcolor (COLOR_BLACK);
        gfx_clear ();
        gfx_setcolor (COLOR_WHITE);
        gfx_outtextxy (4, 2, "SUZY MATH TEST");

        row (14, "MDS", mds_f, mds_p, mds_t);
        row (23, "MDU", mdu_f, mdu_p, mdu_t);
        row (32, "DVS", dvs_f, dvs_p, dvs_t);
        row (41, "DVU", dvu_f, dvu_p, dvu_t);
        row (50, "MOS", mos_f, mos_p, mos_t);
        row (59, "MOU", mou_f, mou_p, mou_t);

        gfx_setcolor (anyfail ? COLOR_RED : COLOR_GREEN);
        gfx_outtextxy (4, 72, anyfail ? "FAIL:" : "ALL PASS");
        if (anyfail) {
            gfx_setcolor (COLOR_WHITE);
            gfx_outtextxy (4, 82, fbuf1);
            gfx_outtextxy (4, 91, fbuf2);
        } else {
            gfx_setcolor (COLOR_YELLOW);
            gfx_outtextxy (4, 91, "A = AGAIN");
        }
        gfx_updatedisplay ();
        while (gfx_busy ()) {}

        while (!(joy_read () & JOY_BTN_1_MASK)) {}
        while (joy_read () & JOY_BTN_1_MASK) {}
    }
    return 0;
}
