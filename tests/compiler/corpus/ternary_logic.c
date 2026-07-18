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
** Ternaries, short-circuit && and ||, and comparison chains — the
** branch-heavy shapes the peepholes rewrite into flag tricks. Side
** effects inside the short-circuit arms verify the arm that must not
** run really does not.
*/

#include "audit.h"

static u8 effects;

static u8 bump (u8 v)
{
    effects = (u8) (effects + 1u);
    return v;
}

u32 t_ternary_logic (void)
{
    u32 crc = 0;
    u8 i;
    i16 s;
    u16 a, b, r;

    effects = 0;

    for (i = 0; i < 12; ++i) {
        a = (u16) ((u16) i * 0x1357u);
        b = (u16) (0x8000u - (u16) i);
        s = (i16) a;

        /* nested ternaries, both signednesses */
        r = (u16) (a < b ? (a == 0 ? 1u : 2u) : (a > b ? 3u : 4u));
        crc = crcstep (crc, (u32) r);
        r = (u16) (s < 0 ? (u16) (i16) -s : (u16) (i16) s);
        crc = crcstep (crc, (u32) r);

        /* ternary as an operand and as an lvalue-free subexpression */
        r = (u16) ((u16) (a > 0x4000u ? a : b) + (u16) (i & 1u ? 1u : 0u));
        crc = crcstep (crc, (u32) r);

        /* comparison results as arithmetic values */
        r = (u16) ((u16) (a < b) + (u16) (a <= b) + (u16) (a == b)
                   + (u16) (a != b) + (u16) (a >= b) + (u16) (a > b));
        crc = crcstep (crc, (u32) r);            /* always 3 */
        crc = crcstep (crc, (u32) (u16) ((i16) s < 0));
    }

    /* short circuit: right arm must not evaluate */
    effects = 0;
    if (effects == 0 || bump (1)) { crc = crcstep (crc, 0x11u); }
    crc = crcstep (crc, (u32) effects);          /* still 0 */
    if (effects != 0 && bump (1)) { crc = crcstep (crc, 0x22u); }
    crc = crcstep (crc, (u32) effects);          /* still 0 */

    /* short circuit: right arm MUST evaluate */
    if (effects == 0 && bump (7) != 0) { crc = crcstep (crc, 0x33u); }
    crc = crcstep (crc, (u32) effects);          /* 1 */
    if (effects == 0 || bump (0) != 0) { crc = crcstep (crc, 0x44u); }
    crc = crcstep (crc, (u32) effects);          /* 2, and 0x44 not folded */

    /* && / || results stored as values, mixed with ternary */
    for (i = 0; i < 6; ++i) {
        a = (u16) (i & 3u);
        b = (u16) (i >> 1);
        r = (u16) ((a != 0 && b != 0) ? 0xA0u : (a != 0 || b != 0) ? 0xB0u : 0xC0u);
        crc = crcstep (crc, (u32) r);
        crc = crcstep (crc, (u32) (u16) (a != 0 && b == 2u));
        crc = crcstep (crc, (u32) (u16) (a == 1u || b == 1u));
    }
    return crc;
}
