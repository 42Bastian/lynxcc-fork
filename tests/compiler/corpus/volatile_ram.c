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
** volatile accesses must survive every optimizer pass untouched. These
** are volatile RAM globals (not hardware registers, so the cart stays
** deterministic and the host twin agrees): repeated stores that a
** non-volatile pass would coalesce, repeated loads it would CSE, and
** read-modify-write chains it would fold. The CRC can only prove value
** correctness, not access COUNT — the count is what the asm scan and
** eyeball review of the kept .s files are for — but a pass that folds a
** volatile RMW chain usually breaks the value too.
*/

#include "audit.h"

static volatile u8  vb;
static volatile u16 vw;
static volatile u32 vl;

u32 t_volatile_ram (void)
{
    u32 crc = 0;
    u8 i;
    u16 t;

    /* stores that must all happen; only the last survives in value */
    vb = 1; vb = 2; vb = 3;
    crc = crcstep (crc, (u32) vb);

    /* loads that must all happen; a CSE'd pair would still sum right,
    ** but reordering across the interleaved store would not */
    vw = 0x1100;
    t = (u16) (vw + 1u);
    vw = (u16) (t + vw);
    crc = crcstep (crc, (u32) vw);

    /* volatile RMW chains at every width */
    vb = 0x0F;
    vb = (u8) (vb | 0x30u);
    vb = (u8) (vb ^ 0xFFu);
    crc = crcstep (crc, (u32) vb);

    vw = 0x00FF;
    vw = (u16) (vw << 4);
    vw = (u16) (vw + 0x21u);
    crc = crcstep (crc, (u32) vw);

    vl = 0x12345678UL;
    vl = (u32) (vl >> 8);
    vl = (u32) (vl ^ 0x00FF00FFUL);
    crc = crcstep (crc, (u32) vl);

    /* volatile in loop condition and body */
    vb = 0;
    for (i = 0; i < 8; ++i) {
        vb = (u8) (vb + i);
        if (vb > 100)
            vb = (u8) (vb - 100u);
    }
    crc = crcstep (crc, (u32) vb);

    /* volatile read used twice in one expression: two loads, and the
    ** value is stable because nothing writes between them */
    vw = 0x0808;
    crc = crcstep (crc, (u32) (u16) ((u16) vw + (u16) vw));
    return crc;
}
