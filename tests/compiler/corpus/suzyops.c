/* SPDX-License-Identifier: MPL-2.0
**
** This Source Code Form is subject to the terms of the Mozilla Public License,
** v. 2.0. If a copy of the MPL was not distributed with this file, You can
** obtain one at https://mozilla.org/MPL/2.0/.
**
** Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
*/

/* no-host: Suzy operators are lynxcc-only syntax; the software-C
** reference computation runs on the cart right next to them, so this
** file carries its own oracle across the O levels instead.
**
** Compiler-audit corpus (design/LYNX_COMPILER_AUDIT_DESIGN.md sec. T7 /
** threat model A): the fork's !* !/ !% operators and the fused
** a !* b !/ c muldiv (design/LYNX_CODEGEN_DESIGN.md). Each Suzy result
** is folded into the CRC alongside the equivalent software-C result, so
** any divergence between optimization levels OR between Suzy and
** software arithmetic shows up. Operand evaluation order and side
** effects around the fused chain get their own section — novel
** expression-tree shapes are exactly where sequencing bugs live.
*/

#include "audit.h"

static u8 calls;

static i16 nxt (i16 v)
{
    calls = (u8) (calls + 1u);
    return (i16) (v + (i16) calls);
}

u32 t_suzyops (void)
{
    u32 crc = 0;
    static i16 sv[6] = { -300, -7, -1, 1, 123, 3000 };
    static u16 uv[6] = { 0u, 1u, 7u, 255u, 4097u, 65535u };
    u8 i, j, k;

    /* plain Suzy divide / modulo vs software, signed and unsigned */
    for (i = 0; i < 6; ++i) {
        for (j = 0; j < 6; ++j) {
            i16 a = sv[i], b = sv[j];
            u16 ua = uv[i], ub = uv[j];
            if (b != 0 && !(a == -32768 && b == -1)) {
                crc = crcstep (crc, (u32) (u16) (i16) (a !/ b));
                crc = crcstep (crc, (u32) (u16) (i16) (a / b));
                crc = crcstep (crc, (u32) (u16) (i16) (a !% b));
                crc = crcstep (crc, (u32) (u16) (i16) (a % b));
            }
            if (ub != 0) {
                crc = crcstep (crc, (u32) (u16) (ua !/ ub));
                crc = crcstep (crc, (u32) (u16) (ua / ub));
                crc = crcstep (crc, (u32) (u16) (ua !% ub));
                crc = crcstep (crc, (u32) (u16) (ua % ub));
            }
        }
    }

    /* lone !* : 16-bit result, low word must equal software multiply */
    for (i = 0; i < 6; ++i) {
        for (j = 0; j < 6; ++j) {
            u16 ua = uv[i], ub = uv[j];
            i16 a = sv[i], b = sv[j];
            crc = crcstep (crc, (u32) (u16) (ua !* ub));
            crc = crcstep (crc, (u32) (u16) (ua * ub));
            crc = crcstep (crc, (u32) (u16) (i16) (a !* b));
            crc = crcstep (crc, (u32) (u16) (i16) (a * b));
        }
    }

    /* fused muldiv: 32-bit product, then divide — vs long software math */
    for (i = 0; i < 6; ++i) {
        for (j = 0; j < 6; ++j) {
            for (k = 0; k < 6; ++k) {
                i16 a = sv[i], b = sv[j], c = sv[k];
                u16 ua = uv[i], ub = uv[j], uc = uv[k];
                if (c != 0) {
                    crc = crcstep (crc, (u32) (u16) (i16) (a !* b !/ c));
                    crc = crcstep (crc, (u32) (u16) (i16)
                                   (((i32) a * (i32) b) / (i32) c));
                }
                if (uc != 0) {
                    crc = crcstep (crc, (u32) (u16) (ua !* ub !/ uc));
                    crc = crcstep (crc, (u32) (u16)
                                   (((u32) ua * (u32) ub) / (u32) uc));
                }
            }
        }
    }

    /* operand evaluation order and side effects around the fused chain:
    ** one call per operand slot, sequenced by the statement boundary */
    calls = 0;
    {
        i16 a = nxt (10);       /* 11, calls=1 */
        i16 b = nxt (20);       /* 22, calls=2 */
        i16 c = nxt (1);        /*  4, calls=3 */
        crc = crcstep (crc, (u32) (u16) (i16) (a !* b !/ c));
        crc = crcstep (crc, (u32) calls);
        /* side effect INSIDE an operand of the fused chain */
        crc = crcstep (crc, (u32) (u16) (i16) (nxt (5) !* b !/ c));
        crc = crcstep (crc, (u32) calls);
        /* Suzy op result feeding another Suzy op through a temp */
        a = (i16) (b !* c);
        crc = crcstep (crc, (u32) (u16) (i16) (a !/ c));
        crc = crcstep (crc, (u32) (u16) (i16) (a / c));
    }
    return crc;
}
