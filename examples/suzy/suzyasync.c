/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in this directory.
*/

/*
** suzyasync.c - realistic use of this fork's ASYNCHRONOUS Suzy math
** (start / poll / harvest; see design/LYNX_SUZY_ASYNC_MATH_DESIGN.md and
** <suzymath.h>).
**
** A perspective starfield. Each star has a world position (x,y) and a depth
** z; its screen position is the perspective divide
**
**       screen_x = CX + (x * FOCAL) / z
**       screen_y = CY + (y * FOCAL) / z
**
** - exactly the kind of repeated, divisor-varying divide the async API is
** built for. The per-star projection is software-pipelined so the slow
** signed divide overlaps real, non-Suzy work:
**
**     suzy_div_start(x*FOCAL, z);     // kick off the screen_x divide
**         ... advance this star's depth (z -= SPEED, recycle if too near) -
**             pure CPU work, touches no Suzy state, runs WHILE Suzy divides
**     screen_x = CX + suzy_div_result();
**     suzy_div_start(y*FOCAL, z);     // kick off the screen_y divide
**         ... clamp/cull screen_x against the screen bounds (more CPU work)
**     screen_y = CY + suzy_div_result();
**
** The math phase (above) touches no Suzy sprite state, so the contract
** "no Suzy work between start and result" holds; ALL drawing happens in a
** separate phase afterwards (tgi_outtextxy uses the sprite engine, which
** shares the math unit, so it must not run inside an async window).
**
** A one-time self-check compares the async results against the stock
** software operators before the animation starts; the HUD shows MATH OK or
** MATH FAIL so a regression is obvious on real hardware.
**
** Build:  cl65 -O -o suzyasync.lnx suzyasync.c
*/

#include <lynx/lynx.h>
#include <lynx/tgi.h>
#include <lynx/joystick.h>
#include <lynx/suzymath.h>
#include <stdio.h>
#include <6502.h>

/* ------------------------------------------------------------------ */
/* Starfield parameters.                                              */
/* ------------------------------------------------------------------ */

#define NSTARS  40
#define CX      80              /* screen centre */
#define CY      51
#define FOCAL   32              /* projection focal length */
#define ZNEAR   6               /* recycle when a star gets this close */
#define ZFAR    255             /* spawn depth */
#define SPEED   4               /* depth units per frame */

static int          wx[NSTARS];         /* world X (-128..127) */
static int          wy[NSTARS];         /* world Y */
static unsigned     wz[NSTARS];         /* depth */
static int          px[NSTARS];         /* projected screen X */
static int          py[NSTARS];         /* projected screen Y */
static unsigned char vis[NSTARS];       /* 1 if on-screen this frame */
static unsigned char depthchar[NSTARS]; /* 0 far .  1 mid +  2 near * */

/* Deterministic xorshift, so every run looks identical. */
static unsigned rng = 0xC0DEu;
static unsigned xrnd (void)
{
    rng ^= (unsigned)(rng << 7);
    rng ^= (unsigned)(rng >> 9);
    rng ^= (unsigned)(rng << 8);
    return rng;
}

static void spawn (unsigned char i, unsigned char atback)
{
    wx[i] = (int)(xrnd () & 0xFFu) - 128;
    wy[i] = (int)(xrnd () & 0xFFu) - 128;
    /* New stars at the back; the initial field is spread through depth. */
    wz[i] = atback ? ZFAR : (ZNEAR + (xrnd () % (ZFAR - ZNEAR)));
}

static void init_stars (void)
{
    unsigned char i;
    for (i = 0; i < NSTARS; ++i) {
        spawn (i, 0);
    }
}

/* ------------------------------------------------------------------ */
/* The async-pipelined projection: the headline of this sample.        */
/* No Suzy sprite work happens here, so each start/result window is     */
/* clean. The depth advance and the X clamp are the real CPU work that  */
/* overlaps the two per-star divides.                                  */
/* ------------------------------------------------------------------ */

static void project_all (void)
{
    unsigned char i;
    for (i = 0; i < NSTARS; ++i) {
        unsigned zc = wz[i];            /* depth used for BOTH divides */
        int sx, sy;
        unsigned nz;

        /* --- screen_x divide; overlap = advance this star's depth --- */
        suzy_div_start (wx[i] * FOCAL, (int)zc);
        nz = zc - SPEED;                /* (runs while Suzy divides) */
        if ((int)nz < ZNEAR) {
            spawn (i, 1);               /* recycle to the back */
            nz = wz[i];
        }
        wz[i] = nz;
        sx = CX + suzy_div_result ();

        /* --- screen_y divide; overlap = classify/clamp screen_x --- */
        suzy_div_start (wy[i] * FOCAL, (int)zc);
        if (zc >= 180u)      depthchar[i] = 0;      /* far  */
        else if (zc >= 90u)  depthchar[i] = 1;      /* mid  */
        else                 depthchar[i] = 2;      /* near */
        px[i] = sx;
        sy = CY + suzy_div_result ();
        py[i] = sy;

        vis[i] = (sx >= 2 && sx <= 155 && sy >= 8 && sy <= 96) ? 1 : 0;
    }
}

/* ------------------------------------------------------------------ */
/* One-time correctness self-check: async API vs the stock operators.  */
/* ------------------------------------------------------------------ */

static const int tnum[] = { 0, 1, -1, 1234, -1234, 30000, -30000, 32000 };
static const int tden[] = { 1, 2, -2, 7, -7, 127, 256, -1000 };
#define NT (sizeof tnum / sizeof tnum[0])

static unsigned char self_check (void)
{
    unsigned char i, j;
    for (i = 0; i < NT; ++i) {
        for (j = 0; j < NT; ++j) {
            int a = tnum[i], b = tden[j];
            int r;

            suzy_div_start (a, b);
            r = suzy_div_result ();
            if (r != a / b) return 0;

            suzy_mod_start (a, b);
            r = suzy_mod_result ();
            if (r != a % b) return 0;

            suzy_umul_start ((unsigned)a, (unsigned)b);
            if ((unsigned)suzy_umul_result () != (unsigned)(a * b)) return 0;

            /* fused (a*b)/c, 32-bit intermediate, vs a long reference */
            suzy_muldiv_start (a, b, 100);
            r = suzy_muldiv_result ();
            if (r != (int)(((long)a * b) / 100)) return 0;
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Drawing phase (sprite engine; kept strictly out of async windows).  */
/* ------------------------------------------------------------------ */

static char buf[24];
static const char* const glyph[3] = { ".", "+", "*" };
static const unsigned char glyphcol[3] = { COLOR_GREY, COLOR_BLUE, COLOR_WHITE };

static void draw_frame (unsigned frame, unsigned char ok)
{
    unsigned char i;

    tgi_setcolor (COLOR_BLACK);
    tgi_clear ();

    for (i = 0; i < NSTARS; ++i) {
        if (!vis[i]) continue;
        tgi_setcolor (glyphcol[depthchar[i]]);
        tgi_outtextxy (px[i], py[i], glyph[depthchar[i]]);
    }

    tgi_setcolor (ok ? COLOR_GREEN : COLOR_RED);
    tgi_outtextxy (2, 2, ok ? "ASYNC SUZY  MATH OK" : "ASYNC SUZY MATH FAIL");
    tgi_setcolor (COLOR_GREY);
    sprintf (buf, "FRAME %u", frame);
    tgi_outtextxy (2, 94, buf);

    tgi_updatedisplay ();
    while (tgi_busy ()) {}
}

void main (void)
{
    unsigned frame = 0;
    unsigned char ok;

    tgi_init ();
    CLI ();
    while (tgi_busy ()) {}
    tgi_setpalette (tgi_getdefpalette ());

    ok = self_check ();
    init_stars ();

    for (;;) {
        project_all ();             /* async-pipelined perspective divides */
        draw_frame (frame, ok);     /* sprite drawing, outside any window  */
        ++frame;
    }
}
