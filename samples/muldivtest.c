/*
** muldivtest.c - On-hardware correctness test for the fused Suzy
** multiply-divide operator 'a !* b !/ c' (see LYNX_CODEGEN_DESIGN.md 2.6.1).
**
** For every triple drawn from a corner-value table (skipping c == 0) it
** compares the fused hardware result against an independent reference
** computed with the software 32-bit long routines:
**
**     got = a !* b !/ c;                       (Suzy, fused, 32-bit product)
**     exp = (int)(((long)a * b) / c);          (software long math)
**
** Both a signed sweep (tossuzymuldivax) and an unsigned sweep
** (tossuzyumuldivax) are run. The screen shows pass/total for each and the
** first failing triple, if any. "ALL PASS" means every case matched.
**
** The reference deliberately uses long math so it does NOT share any code
** with the Suzy path under test.
**
** Build:  cl65 -t lynx -O -o muldivtest.lnx muldivtest.c
** Press A to re-run.
*/

#include <lynx.h>
#include <tgi.h>
#include <joystick.h>
#include <stdio.h>
#include <6502.h>

static char buf[21];

/* Corner values: 0, +/-1, +/-2, byte edges, +/-1000, +/-30000 and the
** 16-bit signed extremes. The same magnitudes serve the unsigned sweep. */
static const int  sval[] = {
    0, 1, -1, 2, -2, 127, -128, 255, 256, -256,
    1000, -1000, 30000, -30000, 32767, -32768
};
#define NV (sizeof (sval) / sizeof (sval[0]))

int main (void)
{
    unsigned       stotal = 0, spass = 0, utotal = 0, upass = 0;
    int            sfa = 0, sfb = 0, sfc = 0, sfg = 0, sfe = 0;
    int            ufa = 0, ufb = 0, ufc = 0, ufg = 0, ufe = 0;
    unsigned char  sfail = 0, ufail = 0;
    unsigned       i, j, k;

    tgi_init ();
    CLI ();
    while (tgi_busy ()) {}
    tgi_setpalette (tgi_getdefpalette ());

    for (;;) {

        tgi_setcolor (COLOR_BLACK);
        tgi_clear ();
        tgi_setcolor (COLOR_WHITE);
        tgi_outtextxy (4, 4, "MULDIV TEST");
        tgi_setcolor (COLOR_YELLOW);
        tgi_outtextxy (4, 18, "RUNNING...");
        tgi_updatedisplay ();
        while (tgi_busy ()) {}

        stotal = spass = utotal = upass = 0;
        sfail = ufail = 0;

        /* -------- signed sweep -------- */
        for (i = 0; i < NV; ++i) {
            for (j = 0; j < NV; ++j) {
                for (k = 0; k < NV; ++k) {
                    int a = sval[i], b = sval[j], c = sval[k];
                    int got, exp;
                    if (c == 0) {
                        continue;
                    }
                    got = a !* b !/ c;                       /* fused Suzy */
                    exp = (int) (((long) a * (long) b) / (long) c);
                    ++stotal;
                    if (got == exp) {
                        ++spass;
                    } else if (!sfail) {
                        sfail = 1;
                        sfa = a; sfb = b; sfc = c; sfg = got; sfe = exp;
                    }
                }
            }
        }

        /* -------- unsigned sweep (same magnitudes, treated as unsigned) -------- */
        for (i = 0; i < NV; ++i) {
            for (j = 0; j < NV; ++j) {
                for (k = 0; k < NV; ++k) {
                    unsigned a = (unsigned) sval[i];
                    unsigned b = (unsigned) sval[j];
                    unsigned c = (unsigned) sval[k];
                    unsigned got, exp;
                    if (c == 0) {
                        continue;
                    }
                    got = a !* b !/ c;                       /* fused Suzy */
                    exp = (unsigned) (((unsigned long) a * (unsigned long) b)
                                      / (unsigned long) c);
                    ++utotal;
                    if (got == exp) {
                        ++upass;
                    } else if (!ufail) {
                        ufail = 1;
                        ufa = (int) a; ufb = (int) b; ufc = (int) c;
                        ufg = (int) got; ufe = (int) exp;
                    }
                }
            }
        }

        /* -------- results -------- */
        tgi_setcolor (COLOR_BLACK);
        tgi_clear ();
        tgi_setcolor (COLOR_WHITE);
        tgi_outtextxy (4, 2, "MULDIV TEST");

        tgi_setcolor (sfail ? COLOR_RED : COLOR_GREEN);
        sprintf (buf, "S %u/%u", spass, stotal);
        tgi_outtextxy (4, 16, buf);
        if (sfail) {
            sprintf (buf, "%d*%d/%d", sfa, sfb, sfc);
            tgi_outtextxy (4, 26, buf);
            sprintf (buf, "g%d e%d", sfg, sfe);
            tgi_outtextxy (4, 36, buf);
        }

        tgi_setcolor (ufail ? COLOR_RED : COLOR_GREEN);
        sprintf (buf, "U %u/%u", upass, utotal);
        tgi_outtextxy (4, 50, buf);
        if (ufail) {
            sprintf (buf, "%u*%u/%u",
                     (unsigned) ufa, (unsigned) ufb, (unsigned) ufc);
            tgi_outtextxy (4, 60, buf);
            sprintf (buf, "g%u e%u", (unsigned) ufg, (unsigned) ufe);
            tgi_outtextxy (4, 70, buf);
        }

        tgi_setcolor ((!sfail && !ufail) ? COLOR_GREEN : COLOR_RED);
        tgi_outtextxy (4, 86, (!sfail && !ufail) ? "ALL PASS" : "FAIL");
        tgi_setcolor (COLOR_YELLOW);
        tgi_outtextxy (4, 96, "A = AGAIN");
        tgi_updatedisplay ();
        while (tgi_busy ()) {}

        while (!(joy_read (JOY_1) & JOY_BTN_1_MASK)) {}
        while (joy_read (JOY_1) & JOY_BTN_1_MASK) {}
    }
    return 0;
}
