/* SPDX-License-Identifier: MPL-2.0
**
** This Source Code Form is subject to the terms of the Mozilla Public License,
** v. 2.0. If a copy of the MPL was not distributed with this file, You can
** obtain one at https://mozilla.org/MPL/2.0/.
**
** Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
*/

/* Compiler-audit corpus regression (design/LYNX_COMPILER_AUDIT_DESIGN.md
** sec. 5): signed division by a power-of-two constant.
**
** cc65 2.19's g_div strength-reduced signed n / 2^k to a bare arithmetic
** shift (asrax*, or a sign-extended byte move for /256). An arithmetic
** shift FLOORS; C requires truncation toward zero, so every negative
** non-multiple was off by one: -7/2 gave -4 (must be -3), -300/256 gave
** -2 (must be -1). Found live by the audit's host-twin oracle
** 2026-07-19 and FIXED the same day in compiler/cc65/codegen.c g_div:
** negative 8/16-bit dividends get (2^k)-1 added before the shift;
** signed 32-bit dividends use the runtime divide. History in
** doc/compilerbugs.html; upstream reference in
** tests/compiler/upstream/FIXES.md.
*/

#include "audit.h"

static i16 nums[8] = { -32768, -32767, -300, -7, -1, 1, 7, 32767 };
static i32 lnums[6] = {
    (i32) 0x80000000UL, -300000L, -300, -1, 1, 0x7FFFFFFFL
};

u32 t_sdiv_pow2 (void)
{
    u32 crc = 0;
    u8 i;
    i16 n;
    i8 c;

    /* 16-bit: inline adjust + shift */
    for (i = 0; i < 8; ++i) {
        n = nums[i];
        crc = crcstep (crc, (u32) (u16) (i16) (n / 2));      /* asrax1  */
        crc = crcstep (crc, (u32) (u16) (i16) (n / 8));      /* asrax3  */
        crc = crcstep (crc, (u32) (u16) (i16) (n / 256));    /* byte    */
        crc = crcstep (crc, (u32) (u16) (i16) (n / 32768));  /* p2 = 15 */
    }

    /* 8-bit char path */
    for (i = 0; i < 8; ++i) {
        c = (i8) nums[i];
        crc = crcstep (crc, (u32) (u16) (i16) (i8) ((i8) c / 2));
        crc = crcstep (crc, (u32) (u16) (i16) (i8) ((i8) c / 64));
    }

    /* 32-bit: signed constant power-of-two goes through the runtime */
    for (i = 0; i < 6; ++i) {
        crc = crcstep (crc, (u32) (lnums[i] / 2));
        crc = crcstep (crc, (u32) (lnums[i] / 8));
        crc = crcstep (crc, (u32) (lnums[i] / 65536));
    }

    /* the canonical one-liner: -7/2 must be -3 */
    {
        i16 m = -7;
        crc = crcstep (crc, (u32) (u16) (i16) (m / 2));
    }
    return crc;
}
