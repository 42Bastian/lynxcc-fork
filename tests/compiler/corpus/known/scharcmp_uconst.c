/* SPDX-License-Identifier: MPL-2.0
**
** This Source Code Form is subject to the terms of the Mozilla Public License,
** v. 2.0. If a copy of the MPL was not distributed with this file, You can
** obtain one at https://mozilla.org/MPL/2.0/.
**
** Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
*/

/* known-bug: schar-cmp-uconst (tests/compiler/upstream/FIXES.md)
**
** Compiler-audit corpus, KNOWN-BUG regression
** (design/LYNX_COMPILER_AUDIT_DESIGN.md sec. 5 — fails before fix).
**
** signed char compared against an unsigned numeric constant, natural
** spelling (no casts). Under ISO C the char promotes to int and, with
** cc65's 16-bit int, the usual arithmetic conversions make these
** compare as unsigned 16-bit — same observable answers as a 32-bit
** host for these operand ranges. The fork (== cc65 2.19) gets some of
** them wrong; upstream fixed the comparison handling in d628772cd1
** ("Fixed signed char type comparison with unsigned numeric
** constants", 2021-02-22). Found live by the audit's host-twin oracle,
** 2026-07-19.
*/

#include "audit.h"

u32 t_scharcmp_uconst (void)
{
    u32 crc = 0;
    u8 i;

    for (i = 0; i < 4; ++i) {
        i8 s = (i8) (0x7Du + i);        /* 125, 126, 127, -128 */
        crc = crcstep (crc, (u32) (u16) (s > 100u));
        crc = crcstep (crc, (u32) (u16) (s < 200u));
        crc = crcstep (crc, (u32) (u16) (s == 0x80u));
        crc = crcstep (crc, (u32) (u16) (s != 126u));
    }
    return crc;
}
