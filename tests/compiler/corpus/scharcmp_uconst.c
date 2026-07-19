/* SPDX-License-Identifier: MPL-2.0
**
** This Source Code Form is subject to the terms of the Mozilla Public License,
** v. 2.0. If a copy of the MPL was not distributed with this file, You can
** obtain one at https://mozilla.org/MPL/2.0/.
**
** Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
*/

/* Compiler-audit corpus regression (design/LYNX_COMPILER_AUDIT_DESIGN.md
** sec. 5): signed char compared against an unsigned numeric constant,
** natural spelling (no casts).
**
** Under ISO C the char promotes to int and, with cc65's 16-bit int, the
** usual arithmetic conversions make these compare as unsigned 16-bit —
** same observable answers as a 32-bit host for these operand ranges.
** The fork (== cc65 2.19) got some of them wrong: hie_compare's
** char-vs-constant fast path clamped the unsigned constant into char
** range and compared as a (possibly signed) char op. Found live by the
** audit's host-twin oracle 2026-07-19 and FIXED 2026-07-19 in
** compiler/cc65/expr.c by porting upstream d628772cd1 ("Fixed signed
** char type comparison with unsigned numeric constants", 2021-02-22):
** the char fast path now requires a signed constant, and mixed
** signedness falls through to the full-width unsigned compare. History
** in doc/compilerbugs.html; upstream reference in
** tests/compiler/upstream/FIXES.md.
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
