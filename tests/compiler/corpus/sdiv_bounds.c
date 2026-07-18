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
** Signed / and % at the boundaries: rounding toward zero for every sign
** combination, remainder sign following the dividend, and the extremes
** near INT_MIN. The INT_MIN/-1 quotient is UB in C, so that single pair
** is guarded out; INT_MIN divided by everything else is fair game.
** Constant divisors get their own section because cc65 strength-reduces
** those through a different path than variable divisors.
*/

#include "audit.h"

static i16 nums[8] = { -32768, -32767, -7, -1, 0, 1, 7, 32767 };
static i16 dens[6] = { -32768, -3, -1, 1, 3, 32767 };

u32 t_sdiv_bounds (void)
{
    u32 crc = 0;
    u8 i, j;
    i16 n, d;

    /* variable / variable, all sign combinations */
    for (i = 0; i < 8; ++i) {
        for (j = 0; j < 6; ++j) {
            n = nums[i];
            d = dens[j];
            if (d == 0 || (n == -32768 && d == -1))
                continue;
            crc = crcstep (crc, (u32) (u16) (i16) (n / d));
            crc = crcstep (crc, (u32) (u16) (i16) (n % d));
        }
    }

    /* variable / constant: strength-reduced path. Signed division by a
    ** POWER OF TWO constant (n/2, n/8, n/256 ...) is deliberately absent
    ** here: this fork inherits cc65 2.19's g_div, which strength-reduces
    ** it to an arithmetic shift (floor, not C's truncation toward zero).
    ** That known miscompile lives in corpus/known/sdiv_pow2.c until the
    ** upstream fix is ported (tests/compiler/upstream/FIXES.md). */
    for (i = 0; i < 8; ++i) {
        n = nums[i];
        crc = crcstep (crc, (u32) (u16) (i16) (n % 2));
        crc = crcstep (crc, (u32) (u16) (i16) (n % 8));
        crc = crcstep (crc, (u32) (u16) (i16) (n / -2));
        crc = crcstep (crc, (u32) (u16) (i16) (n / 10));
        crc = crcstep (crc, (u32) (u16) (i16) (n % 10));
        crc = crcstep (crc, (u32) (u16) (i16) (n % 256));
    }

    /* unsigned sibling paths at the same magnitudes */
    for (i = 0; i < 8; ++i) {
        u16 un = (u16) nums[i];
        crc = crcstep (crc, (u32) (u16) (un / 2u));
        crc = crcstep (crc, (u32) (u16) (un / 3u));
        crc = crcstep (crc, (u32) (u16) (un % 3u));
        crc = crcstep (crc, (u32) (u16) (un / 256u));
        crc = crcstep (crc, (u32) (u16) (un % 256u));
        crc = crcstep (crc, (u32) (u16) (un / 257u));
        crc = crcstep (crc, (u32) (u16) (un % 257u));
    }

    /* rounding-toward-zero spot checks with known answers */
    crc = crcstep (crc, (u32) (u16) (i16) (7 / -2));    /* -3 */
    crc = crcstep (crc, (u32) (u16) (i16) (-7 / 2));    /* -3 */
    crc = crcstep (crc, (u32) (u16) (i16) (-7 % 2));    /* -1 */
    crc = crcstep (crc, (u32) (u16) (i16) (7 % -2));    /*  1 */
    return crc;
}
