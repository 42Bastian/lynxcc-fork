/* SPDX-License-Identifier: MPL-2.0
**
** This Source Code Form is subject to the terms of the Mozilla Public License,
** v. 2.0. If a copy of the MPL was not distributed with this file, You can
** obtain one at https://mozilla.org/MPL/2.0/.
**
** Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
*/

/* known-bug: slong-cmp-mixed (tests/compiler/upstream/FIXES.md)
**
** Compiler-audit corpus, KNOWN-BUG regression
** (design/LYNX_COMPILER_AUDIT_DESIGN.md sec. 5 — fails before fix).
**
** signed long compared against smaller unsigned types. ISO C converts
** the unsigned int/char operand to (signed) long — 40000 and 200 are
** representable — so -2 < 40000 must be true. The fork (== cc65 2.19)
** miscompares; upstream fixed it in c8956ce19b ("Fixed signed long
** comparisons with smaller unsigned types", 2022-03-03). Found live by
** the audit's host-twin oracle, 2026-07-19.
*/

#include "audit.h"

u32 t_slongcmp_mixed (void)
{
    u32 crc = 0;
    i32 sl;
    u16 uw = 40000u;
    u8  ub = 200;

    sl = -2;
    crc = crcstep (crc, (u32) (u16) (sl < uw));     /* must be 1 */
    crc = crcstep (crc, (u32) (u16) (sl < ub));     /* must be 1 */
    crc = crcstep (crc, (u32) (u16) (sl > uw));     /* must be 0 */
    sl = 50000;
    crc = crcstep (crc, (u32) (u16) (sl > uw));     /* must be 1 */
    crc = crcstep (crc, (u32) (u16) (sl < uw));     /* must be 0 */
    sl = 40000;
    crc = crcstep (crc, (u32) (u16) (sl == uw));    /* must be 1 */
    return crc;
}
