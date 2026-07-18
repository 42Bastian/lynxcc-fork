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
** char promotion in shifts and comparisons. cc65 loves to keep chars in
** 8 bits when it thinks it can; every value here is folded through an
** explicit cast so both oracles agree, and the cases target exactly the
** places where an over-eager 8-bit narrowing changes the answer: shifts
** whose result needs 16 bits, sign extension of negative chars in
** comparisons, and mixed signed/unsigned compares.
*/

#include "audit.h"

static u8 uvals[6] = { 0x00, 0x01, 0x7F, 0x80, 0xAA, 0xFF };

u32 t_char_promote (void)
{
    u32 crc = 0;
    u8 i, k;
    i8 s;
    u8 u;

    /* left shift of a char must widen: (u16)(uc << k) keeps high bits */
    for (i = 0; i < 6; ++i) {
        u = uvals[i];
        for (k = 0; k < 8; ++k) {
            crc = crcstep (crc, (u32) (u16) ((u16) u << (k & 7)));
            crc = crcstep (crc, (u32) (u8) ((u16) ((u16) u << (k & 7))));
            crc = crcstep (crc, (u32) (u16) ((u16) u >> (k & 7)));
        }
    }

    /* signed char right shift: do it in the signed 16-bit domain */
    for (i = 0; i < 6; ++i) {
        s = (i8) uvals[i];
        for (k = 0; k < 8; ++k)
            crc = crcstep (crc, (u32) (u16) (i16) ((i16) s >> (k & 7)));
    }

    /* negative char vs int comparison: sign extension must happen */
    for (i = 0; i < 6; ++i) {
        s = (i8) uvals[i];
        crc = crcstep (crc, (u32) (u16) ((i16) s < 0));
        crc = crcstep (crc, (u32) (u16) ((i16) s > 100));
        crc = crcstep (crc, (u32) (u16) ((i16) s == -86));   /* 0xAA */
        crc = crcstep (crc, (u32) (u16) ((i16) s >= -1));
    }

    /* unsigned char vs signed char, both explicitly widened */
    for (i = 0; i < 6; ++i) {
        u = uvals[i];
        s = (i8) uvals[5 - i];
        crc = crcstep (crc, (u32) (u16) ((u16) u > (u16) (i16) s));
        crc = crcstep (crc, (u32) (u16) ((i16) (u8) u < (i16) s));
    }

    /* char arithmetic that overflows 8 bits then is narrowed back */
    for (i = 0; i < 6; ++i) {
        u = uvals[i];
        crc = crcstep (crc, (u32) (u8) ((u16) ((u16) u + 0x90u)));
        crc = crcstep (crc, (u32) (u8) ((u16) ((u16) u * 3u)));
        crc = crcstep (crc, (u32) (u16) ((u16) u + 0x90u));
    }
    return crc;
}
