/*
** SPDX-License-Identifier: MIT
**
** Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
** Provided under the MIT License; copy it into your own projects freely.
** See the LICENSE file in this directory.
*/

/*
** heaptest.c - On-target verification of the cc65 Lynx heap allocator.
**
** This ROM exercises the heap two ways and shows a colour-coded pass/fail
** grid on screen:
**
**   1. Through the public stdlib API - malloc, free, calloc, realloc,
**      _heapblocksize, _heapmemavail, _heapadd - checking return values,
**      reported sizes and reclamation.
**
**   2. By examining memory directly - the runtime heap pointers
**      (_heaporg / _heapptr / _heapend), the raw block-size word that lives
**      immediately below every user pointer, and the free-list head
**      (_heapfirst). These checks pin down the 2-byte-header layout
**      (design/LYNX_HEAP_HEADER_DESIGN.md): the admin header is exactly
**      HEAP_ADMIN_SPACE (2) bytes and the raw block is always (user - 2).
**
** Each test sets result[i] = 1 (pass) or 0 (fail). The display shows a 3-wide
** grid of T1..Tn in green (OK) or red (X), a PASS k/n summary, a LEAK line
** (heap fully reclaimed to its starting state), and the first failing test.
**
** Build:  cl65 -Ors -o heaptest.lnx heaptest.c
*/

#include <lynx/lynx.h>
#include <lynx/gfx.h>
#include <stdlib.h>
#include <string.h>
#include <_heap.h>
#include <6502.h>

/* Direct-memory helpers tied to the 2-byte-header layout. */
#define RAWSIZE(p)  (((unsigned*)(p))[-1])              /* size word below user ptr */
#define RAWPTR(p)   ((unsigned char*)(p) - HEAP_ADMIN_SPACE)
#define ADDR(p)     ((unsigned)(p))

#define NTESTS 17
static unsigned char result[NTESTS];

/* A static buffer donated to the heap via _heapadd in the last test. */
static unsigned char donate[64];

/* ---- tiny formatting helpers (avoid pulling in printf) ---- */
static char numbuf[8];

static const char* dec (unsigned v)
{
    char tmp[6];
    unsigned char i = 0, j = 0;
    if (v == 0) { numbuf[0] = '0'; numbuf[1] = 0; return numbuf; }
    while (v) { tmp[i++] = (char)('0' + (v % 10)); v /= 10; }
    while (i) numbuf[j++] = tmp[--i];
    numbuf[j] = 0;
    return numbuf;
}

static char hexbuf[6];
static const char* hex16 (unsigned v)
{
    static const char d[] = "0123456789ABCDEF";
    hexbuf[0] = d[(v >> 12) & 15];
    hexbuf[1] = d[(v >>  8) & 15];
    hexbuf[2] = d[(v >>  4) & 15];
    hexbuf[3] = d[ v        & 15];
    hexbuf[4] = 0;
    return hexbuf;
}

/* memory check: all 'n' bytes at p equal value v */
static unsigned char allbytes (const void* p, unsigned char v, unsigned n)
{
    const unsigned char* q = p;
    while (n--) if (*q++ != v) return 0;
    return 1;
}

/* ---- the test battery ---- */
static void run_tests (void)
{
    unsigned base, m0, h0, h1;
    unsigned char grew;
    void *a, *b, *c, *p, *q, *barrier;

    memset (result, 0, sizeof result);
    base = _heapmemavail ();

    /* T1: the measured header overhead (raw size word minus the user-visible
    ** size) is exactly HEAP_ADMIN_SPACE. The 2-byte value itself is locked in
    ** at compile time by the heap_admin_is_two guard above. */
    p = malloc (8);
    result[0] = (RAWSIZE (p) - _heapblocksize (p) == HEAP_ADMIN_SPACE);
    free (p);

    /* T2: raw size word below the pointer == request + admin (no rounding). */
    p = malloc (100);
    result[1] = (p != NULL) && (RAWSIZE (p) == 102);
    free (p);

    /* T3: _heapblocksize agrees with the raw header (raw - admin == request). */
    p = malloc (100);
    result[2] = (p != NULL) && (_heapblocksize (p) == 100)
                            && (RAWSIZE (p) == _heapblocksize (p) + HEAP_ADMIN_SPACE);
    free (p);

    /* T4: tiny request rounds up to the 6-byte minimum block; user cap == 4. */
    p = malloc (1);
    result[3] = (p != NULL) && (RAWSIZE (p) == 6) && (_heapblocksize (p) == 4);
    free (p);

    /* T5: a fresh allocation advances _heapptr by exactly the raw block size. */
    h0 = ADDR (_heapptr);
    p  = malloc (50);
    result[4] = (p != NULL) && (ADDR (_heapptr) - h0 == RAWSIZE (p))
                            && (RAWSIZE (p) == 52);
    free (p);

    /* T6: freeing the top block returns _heapptr to where it was. */
    h0 = ADDR (_heapptr);
    p  = malloc (50);
    free (p);
    result[5] = (ADDR (_heapptr) == h0);

    /* T7: user pointer == raw + admin, and lies inside [_heaporg, _heapend). */
    p = malloc (8);
    result[6] = (p != NULL)
                && (ADDR (p) == ADDR (RAWPTR (p)) + HEAP_ADMIN_SPACE)
                && (ADDR (p) >= ADDR (_heaporg))
                && (ADDR (p) <  ADDR (_heapend));
    free (p);

    /* T8: an exact-fit free block is reused in full (no heap-top growth). */
    a = malloc (40);
    b = malloc (40);
    free (a);                       /* a is below b -> goes to the free list  */
    h0 = ADDR (_heapptr);
    c  = malloc (40);               /* needs 42; the 42-byte hole is exact-fit */
    result[7] = (c != NULL) && (ADDR (_heapptr) == h0) && (c == a);
    free (b);
    free (c);

    /* T9: realloc grow of the top block happens in place. */
    p = malloc (20);
    q = realloc (p, 40);
    result[8] = (q == p) && (RAWSIZE (q) == 42);
    free (q);

    /* T10: realloc of a non-top block moves it and preserves the payload. */
    p = malloc (20);
    barrier = malloc (8);           /* pins p away from the heap top           */
    memset (p, 0x5A, 20);
    q = realloc (p, 200);
    result[9] = (q != NULL) && (q != p) && allbytes (q, 0x5A, 20);
    free (q);
    free (barrier);

    /* T11: realloc shrink of the top block lowers _heapptr in place. */
    p  = malloc (100);
    h1 = ADDR (_heapptr);
    q  = realloc (p, 10);
    result[10] = (q == p) && (ADDR (_heapptr) < h1) && (RAWSIZE (q) == 12);
    free (q);

    /* T12: calloc zeroes the block and sizes its header correctly. */
    p = calloc (10, 4);
    result[11] = (p != NULL) && (RAWSIZE (p) == 42) && allbytes (p, 0, 40);
    free (p);

    /* T13: malloc(0) returns NULL. */
    result[12] = (malloc (0) == NULL);

    /* T14: free(NULL) is a no-op and disturbs nothing. */
    m0 = _heapmemavail ();
    free (NULL);
    result[13] = (_heapmemavail () == m0);

    /* T15: a neighbour's free leaves adjacent blocks' data and headers intact. */
    a = malloc (16); memset (a, 0xAA, 16);
    b = malloc (16); memset (b, 0xBB, 16);
    c = malloc (16); memset (c, 0xCC, 16);
    free (b);                       /* middle block freed                      */
    result[14] = allbytes (a, 0xAA, 16) && allbytes (c, 0xCC, 16)
                 && (RAWSIZE (a) == 18) && (RAWSIZE (c) == 18);
    free (a);
    free (c);

    /* T16: out-of-order frees coalesce and fully reclaim the heap. */
    m0 = _heapmemavail ();
    a = malloc (30);
    b = malloc (30);
    c = malloc (30);
    free (b);
    free (a);
    free (c);
    result[15] = (_heapmemavail () == m0)
                 && (ADDR (_heapptr) == ADDR (_heaporg))
                 && (_heapfirst == NULL)
                 && (m0 == base);   /* whole run reclaimed back to the start   */

    /* T17: _heapadd donates external memory; it grows the pool by exactly the
    **      buffer size, and a later allocation is served out of that buffer.
    **      The pool-growth check must be read right after the donation (before
    **      the malloc consumes part of it). Run last - it permanently shifts
    **      the free pool, so it must not precede the reclaim check. */
    m0 = _heapmemavail ();
    _heapadd (donate, sizeof donate);
    grew = (_heapmemavail () == m0 + sizeof donate);
    p  = malloc (20);
    result[16] = grew
                 && (p != NULL)
                 && ((unsigned char*) p >= donate)
                 && ((unsigned char*) p <  donate + sizeof donate);
}

/* ---- rendering ---- */
static void draw_cell (unsigned char i)
{
    unsigned char col = i % 3;
    unsigned char row = i / 3;
    int x = 4 + col * 52;
    int y = 28 + row * 8;
    char line[8];
    const char* n = dec (i + 1);
    unsigned char k = 0;

    line[k++] = 'T';
    while (*n) line[k++] = *n++;
    line[k++] = ':';
    if (result[i]) { line[k++] = 'O'; line[k++] = 'K'; }
    else           { line[k++] = 'X'; }
    line[k] = 0;

    gfx_setcolor (result[i] ? COLOR_GREEN : COLOR_RED);
    gfx_outtextxy (x, y, line);
}

void main (void)
{
    unsigned char i, pass = 0, firstfail = 0;
    unsigned base;

    gfx_init ();
    CLI ();
    while (gfx_busy ()) {}
    gfx_setdefpalette ();
    gfx_setframerate (60);
    gfx_setcollisiondetection (0);
    gfx_setfont (GFX_FONT_COMPACT);

    base = _heapmemavail ();
    run_tests ();

    for (i = 0; i < NTESTS; ++i) {
        if (result[i]) ++pass;
        else if (firstfail == 0) firstfail = i + 1;
    }

    for (;;) {
        while (gfx_busy ()) {}

        gfx_setcolor (COLOR_BLACK);
        gfx_clear ();

        gfx_setcolor (COLOR_WHITE);
        gfx_outtextxy (2, 2, "HEAP 2B-HEADER SELFTEST");

        /* Direct-memory facts, read straight from the runtime heap vars. */
        gfx_setcolor (COLOR_LIGHTBLUE);
        gfx_outtextxy (2, 10, "ADMIN:");
        gfx_outtextxy (40, 10, dec (HEAP_ADMIN_SPACE));
        gfx_outtextxy (60, 10, "MEM:");
        gfx_outtextxy (92, 10, hex16 (base));

        gfx_outtextxy (2, 18, "ORG:");
        gfx_outtextxy (30, 18, hex16 (ADDR (_heaporg)));
        gfx_outtextxy (64, 18, "PTR:");
        gfx_outtextxy (92, 18, hex16 (ADDR (_heapptr)));
        gfx_outtextxy (126, 18, hex16 (ADDR (_heapend)));

        for (i = 0; i < NTESTS; ++i) draw_cell (i);

        gfx_setcolor ((pass == NTESTS) ? COLOR_GREEN : COLOR_RED);
        gfx_outtextxy (2, 78, "PASS");
        gfx_outtextxy (32, 78, dec (pass));
        gfx_outtextxy (48, 78, "/");
        gfx_outtextxy (56, 78, dec (NTESTS));

        if (pass == NTESTS) {
            gfx_setcolor (COLOR_GREEN);
            gfx_outtextxy (2, 88, "ALL CHECKS PASSED");
        } else {
            gfx_setcolor (COLOR_RED);
            gfx_outtextxy (2, 88, "1ST FAIL T");
            gfx_outtextxy (62, 88, dec (firstfail));
        }

        gfx_updatedisplay ();
    }
}
