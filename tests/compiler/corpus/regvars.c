/* SPDX-License-Identifier: MPL-2.0
**
** This Source Code Form is subject to the terms of the Mozilla Public License,
** v. 2.0. If a copy of the MPL was not distributed with this file, You can
** obtain one at https://mozilla.org/MPL/2.0/.
**
** Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
*/

/* Compiler-audit corpus (design/LYNX_COMPILER_AUDIT_DESIGN.md sec. T7).
**
** register variables reused across loop bodies. -Ors puts these in the
** zero-page register bank with save/restore sequences around the
** function; the audit runs the same code at -O (no regvars) so any
** divergence in save/restore, aliasing with the pointer walks, or
** reuse of a dead register slot shows up as a CRC mismatch. Nested
** functions force spills at call boundaries.
*/

#include "audit.h"

static u8 tab[12];

static u16 helper (u16 x)
{
    register u16 acc = x;
    register u8  n;
    for (n = 0; n < 4; ++n)
        acc = (u16) (acc + (u16) ((u16) n * 3u));
    return acc;
}

u32 t_regvars (void)
{
    u32 crc = 0;
    register u8 *p;
    register u16 sum;
    register i16 sacc;
    u8 i;

    for (i = 0; i < 12; ++i)
        tab[i] = (u8) (0x60 + i);

    /* register pointer walking a table, register accumulator */
    sum = 0;
    for (p = tab; p < tab + 12; ++p)
        sum = (u16) (sum + (u16) *p);
    crc = crcstep (crc, (u32) sum);

    /* same registers reused in a second loop with different roles */
    sacc = 0;
    for (p = tab + 11; p >= tab; --p) {
        sacc = (i16) (sacc - (i16) (u16) *p);
        if (p == tab)
            break;              /* no --p below tab (UB) */
    }
    crc = crcstep (crc, (u32) (u16) sacc);

    /* register values must survive a call that itself uses regvars */
    sum = 0x1234;
    sum = (u16) (sum + helper ((u16) (sum & 0x00FFu)));
    crc = crcstep (crc, (u32) sum);

    /* register loop var with inner call each iteration */
    for (i = 0; i < 4; ++i) {
        register u16 v = (u16) ((u16) i << 4);
        v = (u16) (v ^ helper ((u16) i));
        crc = crcstep (crc, (u32) v);
    }

    /* interleaved register char pair (the 2-slot bank edge) */
    {
        register u8 x = 3, y = 200;
        for (i = 0; i < 5; ++i) {
            x = (u8) (x + y);
            y = (u8) (y ^ x);
        }
        crc = crcstep (crc, (u32) ((u32) x | ((u32) y << 8)));
    }
    return crc;
}
