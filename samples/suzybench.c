/*
** suzybench.c - Software vs Suzy hardware math benchmark for the Atari Lynx.
**
** Times unsigned 16-bit multiply, divide and modulo using the standard C
** operators (stock software runtime) against this fork's Suzy hardware
** operators !* !/ !% (see LYNX_CODEGEN_DESIGN.md section 2.6).
**
** Each test runs ITER iterations of an identical loop with identical
** operand sequences, so the six reported numbers (clock() ticks, one tick
** per video frame) are directly comparable. Loop overhead is included on
** both sides.
**
** After the results are shown, press button A to run another round.
**
** Build:  cl65 -t lynx -Ors -o suzybench.lnx suzybench.c
*/

#include <lynx.h>
#include <tgi.h>
#include <joystick.h>
#include <time.h>
#include <stdio.h>
#include <6502.h>

#define ITER 4000U

/* Accumulator keeps the results live so loops are not dead code. */
static unsigned sink;

static char buf[21];

/* ------------------------------------------------------------------ */
/* The six timed loops. Operands step through the same sequences in    */
/* every test; b starts at 1 and never reaches 0 within ITER steps.    */
/* ------------------------------------------------------------------ */

static clock_t bench_sw_mul (void)
{
    unsigned a = 3, b = 1, i;
    clock_t t = clock ();
    for (i = 0; i < ITER; ++i) {
        sink += a * b;
        a += 3; b += 7;
    }
    return clock () - t;
}

static clock_t bench_hw_mul (void)
{
    unsigned a = 3, b = 1, i;
    clock_t t = clock ();
    for (i = 0; i < ITER; ++i) {
        sink += a !* b;
        a += 3; b += 7;
    }
    return clock () - t;
}

static clock_t bench_sw_div (void)
{
    unsigned a = 3, b = 1, i;
    clock_t t = clock ();
    for (i = 0; i < ITER; ++i) {
        sink += a / b;
        a += 3; b += 7;
    }
    return clock () - t;
}

static clock_t bench_hw_div (void)
{
    unsigned a = 3, b = 1, i;
    clock_t t = clock ();
    for (i = 0; i < ITER; ++i) {
        sink += a !/ b;
        a += 3; b += 7;
    }
    return clock () - t;
}

static clock_t bench_sw_mod (void)
{
    unsigned a = 3, b = 1, i;
    clock_t t = clock ();
    for (i = 0; i < ITER; ++i) {
        sink += a % b;
        a += 3; b += 7;
    }
    return clock () - t;
}

static clock_t bench_hw_mod (void)
{
    unsigned a = 3, b = 1, i;
    clock_t t = clock ();
    for (i = 0; i < ITER; ++i) {
        sink += a !% b;
        a += 3; b += 7;
    }
    return clock () - t;
}

/* ------------------------------------------------------------------ */

static void show_row (unsigned char y, const char* name,
                      unsigned long sw, unsigned long hw)
{
    sprintf (buf, "%s %6lu %6lu", name, sw, hw);
    tgi_outtextxy (4, y, buf);
}

void main (void)
{
    clock_t swmul, hwmul, swdiv, hwdiv, swmod, hwmod;

    tgi_install (tgi_static_stddrv);
    joy_install (joy_static_stddrv);
    tgi_init ();
    CLI ();

    while (tgi_busy ()) {}
    tgi_setpalette (tgi_getdefpalette ());

    for (;;) {

        /* Progress message */
        tgi_clear ();
        tgi_setcolor (COLOR_WHITE);
        tgi_outtextxy (4, 10, "SUZY MATH BENCH");
        tgi_setcolor (COLOR_YELLOW);
        tgi_outtextxy (4, 30, "BENCHMARK");
        tgi_outtextxy (4, 40, "IN PROGRESS...");
        tgi_updatedisplay ();
        while (tgi_busy ()) {}

        /* Run the six tests */
        swmul = bench_sw_mul ();
        hwmul = bench_hw_mul ();
        swdiv = bench_sw_div ();
        hwdiv = bench_hw_div ();
        swmod = bench_sw_mod ();
        hwmod = bench_hw_mod ();

        /* Results */
        tgi_clear ();
        tgi_setcolor (COLOR_WHITE);
        tgi_outtextxy (4,  2, "SUZY MATH BENCH");
        sprintf (buf, "TICKS/%u OPS", ITER);
        tgi_outtextxy (4, 12, buf);
        tgi_setcolor (COLOR_GREEN);
        tgi_outtextxy (4, 26, "      SOFT   SUZY");
        tgi_setcolor (COLOR_WHITE);
        show_row (36, "MUL", swmul, hwmul);
        show_row (46, "DIV", swdiv, hwdiv);
        show_row (56, "MOD", swmod, hwmod);
        tgi_setcolor (COLOR_YELLOW);
        tgi_outtextxy (4, 80, "A = RUN AGAIN");
        tgi_updatedisplay ();
        while (tgi_busy ()) {}

        /* Wait for button A (press, then release) */
        while (!(joy_read (JOY_1) & JOY_BTN_1_MASK)) {}
        while (joy_read (JOY_1) & JOY_BTN_1_MASK) {}
    }
}
