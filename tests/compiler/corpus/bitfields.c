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
** Bitfield insert and extract at widths 1..15, deliberately crossing
** byte boundaries. cc65 supports unsigned int bitfields; the layout is
** ABI-private, so the CRC folds only the VALUES read back through the
** field names (identical for both oracles), never the raw bytes.
*/

#include "audit.h"

typedef struct {
    unsigned int f1  : 1;
    unsigned int f3  : 3;
    unsigned int f4  : 4;       /* first byte full */
    unsigned int f7  : 7;       /* crosses into byte 2 */
    unsigned int f1b : 1;
} BF16;

typedef struct {
    unsigned int lo  : 5;
    unsigned int mid : 6;       /* straddles the byte boundary */
    unsigned int hi  : 5;
} BF565;

typedef struct {
    unsigned int a : 15;        /* nearly full word */
    unsigned int b : 1;
} BF151;

static BF16  g16;
static BF565 g565;
static BF151 g151;

u32 t_bitfields (void)
{
    u32 crc = 0;
    u8 i;
    u16 v;

    /* walk values through every field, read back after each insert */
    for (i = 0; i < 8; ++i) {
        v = (u16) ((u16) i * 0x2Du);

        g16.f1  = (u16) (v & 0x1u);
        g16.f3  = (u16) (v & 0x7u);
        g16.f4  = (u16) (v & 0xFu);
        g16.f7  = (u16) (v & 0x7Fu);
        g16.f1b = (u16) ((v >> 3) & 0x1u);
        crc = crcstep (crc, (u32) (u16) g16.f1);
        crc = crcstep (crc, (u32) (u16) g16.f3);
        crc = crcstep (crc, (u32) (u16) g16.f4);
        crc = crcstep (crc, (u32) (u16) g16.f7);
        crc = crcstep (crc, (u32) (u16) g16.f1b);

        /* neighbours must survive a single-field update */
        g16.f4 = (u16) (~v & 0xFu);
        crc = crcstep (crc, (u32) (u16) g16.f3);
        crc = crcstep (crc, (u32) (u16) g16.f4);
        crc = crcstep (crc, (u32) (u16) g16.f7);

        g565.lo  = (u16) (v & 0x1Fu);
        g565.mid = (u16) ((v >> 5) & 0x3Fu);
        g565.hi  = (u16) ((v >> 11) & 0x1Fu);
        crc = crcstep (crc, (u32) (u16) g565.lo);
        crc = crcstep (crc, (u32) (u16) g565.mid);
        crc = crcstep (crc, (u32) (u16) g565.hi);

        g151.a = (u16) (v & 0x7FFFu);
        g151.b = (u16) (v >> 15);
        crc = crcstep (crc, (u32) (u16) g151.a);
        crc = crcstep (crc, (u32) (u16) g151.b);
    }

    /* bitfield arithmetic: compound assign and increment on fields */
    g565.mid = 60;
    g565.mid = (u16) ((u16) g565.mid + 5u);      /* wraps within 6 bits */
    crc = crcstep (crc, (u32) (u16) g565.mid);
    g16.f3 = 7;
    g16.f3 = (u16) ((u16) g16.f3 + 1u);          /* 3-bit wrap to 0 */
    crc = crcstep (crc, (u32) (u16) g16.f3);

    /* field in a condition and as a shift amount */
    v = (u16) (g16.f3 == 0 ? 0x55AAu : 0xAA55u);
    crc = crcstep (crc, (u32) v);
    g16.f3 = 3;
    crc = crcstep (crc, (u32) (u16) (0x0101u << ((u16) g16.f3 & 7u)));
    return crc;
}
