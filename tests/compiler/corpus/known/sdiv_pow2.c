/* SPDX-License-Identifier: MPL-2.0
**
** This Source Code Form is subject to the terms of the Mozilla Public License,
** v. 2.0. If a copy of the MPL was not distributed with this file, You can
** obtain one at https://mozilla.org/MPL/2.0/.
**
** Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
*/

/* known-bug: sdiv-pow2 (tests/compiler/upstream/FIXES.md)
**
** Compiler-audit corpus, KNOWN-BUG regression
** (design/LYNX_COMPILER_AUDIT_DESIGN.md sec. 5 — fails before fix).
**
** Signed division by a power-of-two constant. This fork inherits cc65
** 2.19's g_div (compiler/cc65/codegen.c), which strength-reduces
** n / 2^k to an arithmetic shift (asrax*, or a sign-extended byte move
** for /256). An arithmetic shift is FLOOR division; C requires
** truncation toward zero, so every negative non-multiple is off by one:
** -7/2 yields -4 (must be -3), -300/256 yields -2 (must be -1).
** Upstream fixed this shortly after 2.19 with a conditional-shift
** sequence (see FIXES.md). Found live by the audit's host-twin oracle,
** 2026-07-19.
**
** The harness treats this file as expected-to-diverge until the fix is
** ported; when it starts passing, the fix has landed and the known-bug
** marker must be removed (the harness flags the stale marker).
*/

#include "audit.h"

static i16 nums[8] = { -32768, -32767, -300, -7, -1, 1, 7, 32767 };

u32 t_sdiv_pow2 (void)
{
    u32 crc = 0;
    u8 i;
    i16 n;

    for (i = 0; i < 8; ++i) {
        n = nums[i];
        crc = crcstep (crc, (u32) (u16) (i16) (n / 2));    /* asrax1 */
        crc = crcstep (crc, (u32) (u16) (i16) (n / 8));    /* asrax3 */
        crc = crcstep (crc, (u32) (u16) (i16) (n / 256));  /* byte move */
    }
    /* the canonical one-liner: must be -3, the shift gives -4 */
    {
        i16 m = -7;
        crc = crcstep (crc, (u32) (u16) (i16) (m / 2));
    }
    return crc;
}
