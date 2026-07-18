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
** 32-bit (long) arithmetic through the runtime helpers: mul, div, mod,
** shifts, compares, widening and narrowing. On the target every one of
** these is a runtime call plus sreg/ptr1 bookkeeping that the optimizer
** loves to rearrange; on the host it's native. Values are chosen to
** straddle the 16-bit boundary and the sign bit.
*/

#include "audit.h"

static u32 uv[7] = {
    0x00000000UL, 0x00000001UL, 0x0000FFFFUL, 0x00010000UL,
    0x7FFFFFFFUL, 0x80000000UL, 0xFFFFFFFFUL
};

u32 t_long32 (void)
{
    u32 crc = 0;
    u8 i, j, k;
    u32 a, b;
    i32 sa, sb;

    for (i = 0; i < 7; ++i) {
        for (j = 0; j < 7; ++j) {
            a = uv[i];
            b = uv[j];

            crc = crcstep (crc, (u32) (a + b));
            crc = crcstep (crc, (u32) (a - b));
            crc = crcstep (crc, (u32) (a * b));
            if (b != 0) {
                crc = crcstep (crc, (u32) (a / b));
                crc = crcstep (crc, (u32) (a % b));
            }
            crc = crcstep (crc, (u32) (a & b));
            crc = crcstep (crc, (u32) (a | b));
            crc = crcstep (crc, (u32) (a ^ b));
            crc = crcstep (crc, (u32) (u16) (a > b));
            crc = crcstep (crc, (u32) (u16) (a == b));

            /* signed compares on the same bit patterns */
            sa = (i32) a;
            sb = (i32) b;
            crc = crcstep (crc, (u32) (u16) (sa < sb));
            crc = crcstep (crc, (u32) (u16) (sa >= sb));

            /* signed div/mod, guarded (min/-1 is UB) */
            if (sb != 0 && !(sa == (i32) 0x80000000UL && sb == -1)) {
                crc = crcstep (crc, (u32) (sa / sb));
                crc = crcstep (crc, (u32) (sa % sb));
            }
        }
    }

    /* shifts by every interesting count, both directions */
    for (i = 0; i < 7; ++i) {
        a = uv[i];
        for (k = 0; k < 32; k = (u8) (k + 5)) {
            crc = crcstep (crc, (u32) (a << (k & 31)));
            crc = crcstep (crc, (u32) (a >> (k & 31)));
            sa = (i32) a;
            crc = crcstep (crc, (u32) (i32) (sa >> (k & 31)));
        }
    }

    /* widening u16 -> u32 and narrowing back through arithmetic */
    for (i = 0; i < 7; ++i) {
        u16 w = (u16) uv[i];
        crc = crcstep (crc, (u32) ((u32) w * 0x10001UL));
        crc = crcstep (crc, (u32) (u16) (uv[i] >> 8));
        crc = crcstep (crc, (u32) (u8) uv[i]);
        crc = crcstep (crc, (u32) (i32) (i16) (u16) uv[i]);  /* sign extend */
    }
    return crc;
}
