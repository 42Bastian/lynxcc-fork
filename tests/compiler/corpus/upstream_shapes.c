/* SPDX-License-Identifier: MPL-2.0
**
** This Source Code Form is subject to the terms of the Mozilla Public License,
** v. 2.0. If a copy of the MPL was not distributed with this file, You can
** obtain one at https://mozilla.org/MPL/2.0/.
**
** Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
*/

/* Compiler-audit corpus (design/LYNX_COMPILER_AUDIT_DESIGN.md sec. T6).
**
** Directed repros for post-2.19 upstream cc65 wrong-code fixes whose
** buggy form this fork may still carry (mined by
** tests/compiler/upstream/mine.py; classification in
** tests/compiler/upstream/FIXES.md). Each section names the upstream
** commit it probes. Unlike the rest of the corpus these deliberately
** use the natural, promotion-reliant spellings — the cast-free shapes
** the bugs lived in. The shapes stay UB-free in ISO C, so the host twin
** remains a valid oracle.
*/

#include "audit.h"

/* NB: member offsets are layout-agnostic here — the host twin pads
** structs differently from cc65, so cross-member byte tricks must land
** on whole members ((&lp->w)[1] is the u16 after w on ANY layout). */
typedef struct { u8 a; u16 w; u16 x; } LS;

static u16 gw;
static u8  gc;

u32 t_upstream_shapes (void)
{
    u32 crc = 0;
    u8 i;

    /* 29154646 "Fixed cc65's generation of char-type bit-shift code":
    ** uncast char shifts, counts 0..7, both directions, both signs */
    for (i = 0; i < 8; ++i) {
        u8 u = (u8) (0xB7 - i);
        i8 s = (i8) (u ^ 0x80u);
        crc = crcstep (crc, (u32) (u16) (u << i));     /* promoted shift  */
        crc = crcstep (crc, (u32) (u8) (u << i));      /* narrowed back   */
        crc = crcstep (crc, (u32) (u16) (u >> i));
        crc = crcstep (crc, (u32) (i16) (s >> i));     /* impl-def: arith */
    }

    /* d628772cd1 signed char vs unsigned constant comparison: CONFIRMED
    ** still broken here — moved to corpus/known/scharcmp_uconst.c */

    /* b2c1a77bb3 "optimizes 16-bit compares when the high bytes are
    ** known to be equal" */
    for (i = 0; i < 4; ++i) {
        u16 x = (u16) (0x1200u + i);
        u16 y = 0x1202;
        crc = crcstep (crc, (u32) (u16) (x < y));
        crc = crcstep (crc, (u32) (u16) (x == y));
        crc = crcstep (crc, (u32) (u16) (x > 0x1201u));
        gw = (u16) (0x3400u | (u16) i);
        crc = crcstep (crc, (u32) (u16) (gw >= 0x3402u));
    }

    /* c8956ce19b signed long vs smaller unsigned comparison: CONFIRMED
    ** still broken here — moved to corpus/known/slongcmp_mixed.c */

    /* bd8eae67f1 "local struct field access via the address of the
    ** struct" — local (stack) sibling of the member_addr shapes */
    {
        LS loc;
        LS *lp = &loc;
        loc.a = 0x21; loc.w = 0x4342; loc.x = 0x8765;
        ((u8 *) &lp->w)[1] = 0x99;      /* stays inside w on any layout */
        crc = crcstep (crc, (u32) loc.w);
        (&lp->w)[1] = 0xBEEF;           /* the u16 after w == x           */
        crc = crcstep (crc, (u32) loc.x);
        crc = crcstep (crc, (u32) ((u8 *) &lp->w)[0]);
        crc = crcstep (crc, (u32) loc.a);
    }

    /* eadaf2fef8 / 1450f146a5 "operators following a postfix inc/dec"
    ** and sizeof-context deferral */
    {
        static u16 arr[4] = { 10, 20, 30, 40 };
        u16 *p = arr;
        u16 v;
        gc = 0;
        v = (*p++);
        crc = crcstep (crc, (u32) v);
        /* p++[0] (subscript after postfix ++) was REJECTED by this
        ** fork's parser until 2026-07-19, when upstream 1450f146a5
        ** ("'[]', '()' '.' and '->' operators following a postfix
        ** increment/decrement") was ported: postfix ++/-- now live in
        ** hie11's postfix-operator loop. p++[0] reads the element BEFORE
        ** the increment (the ++ yields the old pointer value). */
        v = p++[0];
        crc = crcstep (crc, (u32) v);               /* arr[1] == 20 */
        v = *(p++);
        crc = crcstep (crc, (u32) v);
        crc = crcstep (crc, (u32) (u16) (p - arr));       /* 3 */
        v = (u16) sizeof (gc++);        /* gc must NOT change */
        crc = crcstep (crc, (u32) v);
        crc = crcstep (crc, (u32) gc);
    }

    /* 6e61093e79 "pointer subtraction in certain very rare cases" +
    ** f1c715c455 signedness of pointer differences */
    {
        static u16 arr[6];
        u16 *hi = arr + 5, *lo = arr + 1;
        crc = crcstep (crc, (u32) (i16) (hi - lo));       /*  4 */
        crc = crcstep (crc, (u32) (i16) (lo - hi));       /* -4 */
        crc = crcstep (crc, (u32) (u16) ((hi - lo) > 0));
    }
    return crc;
}
