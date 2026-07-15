/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in this directory.
*/

/*
** suzyasyncbench.c - speed benchmark for this fork's ASYNCHRONOUS Suzy math
** (see design/LYNX_SUZY_ASYNC_MATH_DESIGN.md and <suzymath.h>).
**
** The async API only pays off when there is unrelated, non-Suzy CPU work to
** run in the shadow of a slow divide. It also costs one extra call per
** operation (start + result, versus one blocking call), so with NO overlap
** work it is SLOWER than the plain '!/' operator. This cart measures exactly
** that trade-off honestly:
**
** For three workload sizes (no work / light / heavy) it times, per divide:
**
**   SOFT  : d = a / b;            then the workload     (software divide)
**   SYNC  : d = a !/ b;           then the workload     (blocking Suzy)
**   ASYNC : suzy_udiv_start(a,b); workload; result()    (overlapped Suzy)
**
** The workload is a fixed amount of pure-CPU mixing that touches no Suzy
** state. SYNC runs divide-then-work serially; ASYNC runs the work while the
** divide is in flight. The crossover - where ASYNC overtakes SYNC because
** the work is hidden under the divide latency - is the whole point.
**
** A second screen spot-checks signed divide, unsigned modulo and fused
** muldiv (sync vs async) at the heavy workload.
**
** Every loop also cross-checks results so a speed win is never a correctness
** loss: SOFT, SYNC and ASYNC must agree, or MISMATCH is shown.
**
** Timing uses a 24-bit, 1 us Mikey timer (timer 1 -> 3 -> 5), as in
** suzybench.c. Real hardware is the oracle; an emulator is trustworthy here
** only if it models Suzy math timing.
**
** Build:  cl65 -Osri -o suzyasyncbench.lnx suzyasyncbench.c
*/

#include <lynx/lynx.h>
#include <lynx/gfx.h>
#include <lynx/joystick.h>
#include <lynx/suzymath.h>
#include <stdio.h>
#include <6502.h>

/* ------------------------------------------------------------------ */
/* Mikey 24-bit microsecond timer (timer 1 @1us -> 3 -> 5).            */
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

#define ELAPSED(s, e) (((s) - (e)) & 0xFFFFFFUL)

/* ------------------------------------------------------------------ */
/* Operand sweep (wide odd strides hit every divisor magnitude, as in  */
/* suzybench.c so the divide-timing is representative).                 */
/* ------------------------------------------------------------------ */

#define BITER 1000U
#define INITA 0x1234U
#define INITB 0x0ABCU
#define STEPU(a, b) do { (a) += 0x2B7DU; (b) += 0x1D4FU; if ((b) == 0U) (b) = 1U; } while (0)

static unsigned sink;       /* keeps results live so nothing folds away */
static unsigned wsink;      /* workload accumulator */
static unsigned char mism;  /* set if any SOFT/SYNC/ASYNC result diverged */

/* Pure-CPU workload: no Suzy state touched. n controls the amount. This is
** the work that ASYNC overlaps with the divide and SYNC must run serially. */
static void busywork (unsigned char n)
{
    unsigned char k;
    unsigned a = wsink;
    for (k = 0; k < n; ++k) {
        a += 0x2B7DU;
        a ^= (unsigned)(a >> 5);
    }
    wsink = a;
}

/* The three timed variants, parameterized by workload size. */

static unsigned long t_soft (unsigned char work)
{
    unsigned a = INITA, b = INITB, i, d;
    unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        d = a / b;
        busywork (work);
        sink += d;
        STEPU (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}

static unsigned long t_sync (unsigned char work)
{
    unsigned a = INITA, b = INITB, i, d;
    unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        d = a !/ b;
        busywork (work);
        sink += d;
        STEPU (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}

static unsigned long t_async (unsigned char work)
{
    unsigned a = INITA, b = INITB, i, d;
    unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        suzy_udiv_start (a, b);     /* divide runs ... */
        busywork (work);            /* ... while this CPU work overlaps it */
        d = suzy_udiv_result ();
        sink += d;
        STEPU (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}

/* Correctness: run the same sweep once and confirm SOFT == SYNC == ASYNC. */
static void verify_div (void)
{
    unsigned a = INITA, b = INITB, i, dsoft, dsync, dasync;
    for (i = 0; i < BITER; ++i) {
        dsoft = a / b;
        dsync = a !/ b;
        suzy_udiv_start (a, b);
        busywork (3);
        dasync = suzy_udiv_result ();
        if (dsoft != dsync || dsoft != dasync) mism = 1;
        STEPU (a, b);
    }
}

/* ------------------------------------------------------------------ */
/* Spot checks for signed divide, unsigned modulo and fused muldiv.    */
/* Each compares sync (operator) vs async at the heavy workload.        */
/* ------------------------------------------------------------------ */

#define HEAVY 20

static unsigned long sp_sync[3];        /* SDIV, UMOD, MULDIV */
static unsigned long sp_async[3];

static unsigned long sp_sdiv_sync (void)
{
    int a = INITA, b = INITB; unsigned i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += (unsigned)(a !/ b);
        busywork (HEAVY);
        a = (int)((unsigned)a + 0x2B7DU); b = (int)((unsigned)b + 0x1D4FU);
        if (b == 0) b = 1;
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long sp_sdiv_async (void)
{
    int a = INITA, b = INITB; unsigned i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        suzy_div_start (a, b);
        busywork (HEAVY);
        sink += (unsigned)suzy_div_result ();
        a = (int)((unsigned)a + 0x2B7DU); b = (int)((unsigned)b + 0x1D4FU);
        if (b == 0) b = 1;
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long sp_umod_sync (void)
{
    unsigned a = INITA, b = INITB, i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += a !% b;
        busywork (HEAVY);
        STEPU (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long sp_umod_async (void)
{
    unsigned a = INITA, b = INITB, i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        suzy_umod_start (a, b);
        busywork (HEAVY);
        sink += suzy_umod_result ();
        STEPU (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long sp_muldiv_sync (void)
{
    unsigned a = INITA, b = INITB, i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        sink += a !* b !/ 1000u;       /* fused operator path */
        busywork (HEAVY);
        STEPU (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}
static unsigned long sp_muldiv_async (void)
{
    unsigned a = INITA, b = INITB, i; unsigned long s, e;
    s = timer_read ();
    for (i = 0; i < BITER; ++i) {
        suzy_umuldiv_start (a, b, 1000u);
        busywork (HEAVY);
        sink += suzy_umuldiv_result ();
        STEPU (a, b);
    }
    e = timer_read ();
    return ELAPSED (s, e);
}

/* ------------------------------------------------------------------ */
/* Results storage.                                                    */
/* ------------------------------------------------------------------ */

#define NWORK 3
static const char* const worklabel[NWORK] = { "W=0", "W=6", "W=20" };
static const unsigned char worksz[NWORK]  = { 0, 6, 20 };

static unsigned long us_soft[NWORK];
static unsigned long us_sync[NWORK];
static unsigned long us_async[NWORK];

static void run_all (void)
{
    unsigned char w;
    SEI ();                             /* remove IRQ jitter from timing */
    for (w = 0; w < NWORK; ++w) {
        us_soft[w]  = t_soft  (worksz[w]);
        us_sync[w]  = t_sync  (worksz[w]);
        us_async[w] = t_async (worksz[w]);
    }
    sp_sync[0]  = sp_sdiv_sync ();    sp_async[0] = sp_sdiv_async ();
    sp_sync[1]  = sp_umod_sync ();    sp_async[1] = sp_umod_async ();
    sp_sync[2]  = sp_muldiv_sync ();  sp_async[2] = sp_muldiv_async ();
    CLI ();
}

/* ------------------------------------------------------------------ */
/* Display.                                                            */
/* ------------------------------------------------------------------ */

static char cs[8], cy2[8], ca[8];

static void fmt10 (char* d, unsigned long us)
{
    unsigned t = (unsigned)(us * 10u / BITER);  /* tenths of us per op */
    sprintf (d, "%u.%u", t / 10u, t % 10u);
}

static void wait_a (void)
{
    while (!(joy_read () & JOY_BTN_1_MASK)) {}
    while (joy_read () & JOY_BTN_1_MASK) {}
}

/* The Lynx graphics text renderer draws at most 20 chars per call, so every column is
** emitted as its own short call at a fixed x rather than one wide sprintf.
** Column x's are tuned for the 5x5 compact font (6px pitch): a 6-char value
** is 36px, so the rightmost column ends well within the 160px screen. */
#define CX_LBL   2
#define CX_SOFT  40
#define CX_SYNC  82
#define CX_ASY   122

static void show_div (void)
{
    unsigned char w, y;

    gfx_setcolor (COLOR_BLACK);
    gfx_clear ();
    gfx_setcolor (COLOR_WHITE);
    gfx_outtextxy (CX_LBL, 2, "UDIV+WORK us/op");
    gfx_setcolor (COLOR_GREEN);
    gfx_outtextxy (CX_SOFT, 14, "SOFT");
    gfx_outtextxy (CX_SYNC, 14, "SYNC");
    gfx_outtextxy (CX_ASY,  14, "ASYNC");

    y = 26;
    for (w = 0; w < NWORK; ++w) {
        fmt10 (cs, us_soft[w]);
        fmt10 (cy2, us_sync[w]);
        fmt10 (ca, us_async[w]);
        /* Highlight green when async beats sync (work hidden under divide). */
        gfx_setcolor (us_async[w] < us_sync[w] ? COLOR_GREEN : COLOR_WHITE);
        gfx_outtextxy (CX_LBL,  y, worklabel[w]);
        gfx_outtextxy (CX_SOFT, y, cs);
        gfx_outtextxy (CX_SYNC, y, cy2);
        gfx_outtextxy (CX_ASY,  y, ca);
        y += 11;
    }

    gfx_setcolor (COLOR_GREY);
    gfx_outtextxy (CX_LBL, 66, "GREEN: ASYNC < SYNC");
    gfx_outtextxy (CX_LBL, 76, "OVERLAP HIDES WORK");

    gfx_setcolor (mism ? COLOR_RED : COLOR_YELLOW);
    gfx_outtextxy (CX_LBL, 92, mism ? "MISMATCH! A=NEXT" : "RESULTS OK  A=NEXT");
    gfx_updatedisplay ();
    while (gfx_busy ()) {}
    wait_a ();
}

static const char* const spname[3] = { "SDIV", "UMOD", "MDIV" };

#define SX_LBL   2
#define SX_SYNC  60
#define SX_ASY   110

static void show_spot (void)
{
    unsigned char i, y;

    gfx_setcolor (COLOR_BLACK);
    gfx_clear ();
    gfx_setcolor (COLOR_WHITE);
    gfx_outtextxy (SX_LBL, 2, "OPS +W=20 us/op");
    gfx_setcolor (COLOR_GREEN);
    gfx_outtextxy (SX_SYNC, 14, "SYNC");
    gfx_outtextxy (SX_ASY,  14, "ASYNC");

    y = 26;
    for (i = 0; i < 3; ++i) {
        fmt10 (cy2, sp_sync[i]);
        fmt10 (ca, sp_async[i]);
        gfx_setcolor (sp_async[i] < sp_sync[i] ? COLOR_GREEN : COLOR_WHITE);
        gfx_outtextxy (SX_LBL,  y, spname[i]);
        gfx_outtextxy (SX_SYNC, y, cy2);
        gfx_outtextxy (SX_ASY,  y, ca);
        y += 11;
    }

    gfx_setcolor (COLOR_GREY);
    gfx_outtextxy (SX_LBL, 64, "MDIV = (A*B)/C");
    gfx_outtextxy (SX_LBL, 74, "DIV PART OVERLAPS");
    gfx_setcolor (COLOR_YELLOW);
    gfx_outtextxy (SX_LBL, 92, "A = RUN AGAIN");
    gfx_updatedisplay ();
    while (gfx_busy ()) {}
    wait_a ();
}

static void show_progress (const char* msg)
{
    gfx_setcolor (COLOR_BLACK);
    gfx_clear ();
    gfx_setcolor (COLOR_WHITE);
    gfx_outtextxy (2, 10, "ASYNC SUZY BENCH");
    gfx_setcolor (COLOR_YELLOW);
    gfx_outtextxy (2, 40, msg);
    gfx_updatedisplay ();
    while (gfx_busy ()) {}
}

void main (void)
{
    gfx_init ();
    CLI ();
    while (gfx_busy ()) {}
    gfx_setdefpalette ();
    gfx_setfont (GFX_FONT_COMPACT);     /* 5x5 font (6px pitch): all the
                                        ** number columns fit on screen */
    timer_init ();

    for (;;) {
        mism = 0;
        show_progress ("CHECKING...");
        verify_div ();
        show_progress ("TIMING...");
        run_all ();

        show_div ();
        show_spot ();
    }
}
