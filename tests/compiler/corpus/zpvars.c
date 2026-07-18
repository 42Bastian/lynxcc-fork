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
** __zeropage globals (design/LYNX_ZEROPAGE_VARS_DESIGN.md if present;
** include/zeropage.h): a fork-introduced storage class that flows
** through codegen and every peephole that pattern-matches zp addressing
** modes. Mixed widths, pointers IN zp pointing at absolute data,
** aliasing between a zp var and a pointer to it, and arithmetic that
** the zp-specific peepholes (INC zp, direct zp ops) love to rewrite.
** On the host the ZP macro is empty, so the same code runs as plain
** globals (audit.h).
*/

#include "audit.h"

#ifndef AUDIT_HOST
#include <zeropage.h>
#endif

static u8  zb  ZP;
static u16 zw  ZP;
static u32 zl  ZP;
static u8 *zp1 ZP;

static u8 mem[8];

u32 t_zpvars (void)
{
    u32 crc = 0;
    u8 i;
    u8 *alias;

    /* plain arithmetic on zp vars of every width */
    zb = 0x11;
    zb = (u8) (zb + 0x22u);
    zb = (u8) (zb << 1);
    crc = crcstep (crc, (u32) zb);

    zw = 0x0100;
    for (i = 0; i < 5; ++i)
        zw = (u16) (zw + (u16) ((u16) i * 0x101u));
    crc = crcstep (crc, (u32) zw);

    zl = 0xDEADBEEFUL;
    zl = (u32) (zl >> 4);
    zl = (u32) (zl + zw);
    crc = crcstep (crc, (u32) zl);

    /* inc/dec shapes that map onto INC/DEC zp */
    zb = 0xFE;
    ++zb; ++zb;                 /* wraps to 0 */
    crc = crcstep (crc, (u32) zb);
    --zw;
    crc = crcstep (crc, (u32) zw);

    /* zp pointer into absolute memory, walked and indexed */
    for (i = 0; i < 8; ++i)
        mem[i] = (u8) (0x30 + i);
    zp1 = mem;
    *zp1++ = 0xE0;
    *zp1++ = 0xE1;
    crc = crcstep (crc, (u32) zp1[0]);          /* mem[2] */
    zp1[2] = 0xE4;                              /* mem[4] */
    crc = crcstep (crc, (u32) (u16) (zp1 - mem));

    /* aliasing: write through a pointer AT the zp var, read the var */
    alias = (u8 *) &zb;
    zb = 0x5A;
    *alias = (u8) (*alias ^ 0xFFu);
    crc = crcstep (crc, (u32) zb);

    /* zp var as loop counter and as shift amount */
    zb = 3;
    crc = crcstep (crc, (u32) (u16) (0x0021u << (zb & 7u)));
    for (zb = 0; zb < 6; ++zb)
        crc = crcstep (crc, (u32) zb);

    for (i = 0; i < 8; ++i)
        crc = crcstep (crc, (u32) mem[i]);
    return crc;
}
