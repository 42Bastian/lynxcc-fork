/* SPDX-License-Identifier: MPL-2.0
**
** This Source Code Form is subject to the terms of the Mozilla Public License,
** v. 2.0. If a copy of the MPL was not distributed with this file, You can
** obtain one at https://mozilla.org/MPL/2.0/.
**
** Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
*/

/* Compiler-audit corpus regression (design/LYNX_COMPILER_AUDIT_DESIGN.md
** sec. 5): signed long compared against smaller unsigned types.
**
** ISO C converts the unsigned int/char operand to (signed) long — 40000
** and 200 are representable — so -2 < 40000 must be true. The fork
** (== cc65 2.19) compared unsigned whenever either operand was
** unsigned. Found live by the audit's host-twin oracle 2026-07-19 and
** FIXED 2026-07-19 (fork equivalent of upstream c8956ce19b, "Fixed
** signed long comparisons with smaller unsigned types", 2022-03-03):
** g_typeadjust in compiler/cc65/codegen.c now applies the usual
** arithmetic conversions — a smaller unsigned operand converts to plain
** (signed) long and only an unsigned long operand makes the operation
** unsigned — and hie_compare's constant fold and unsigned-compare
** strength reduction follow the converted common type instead of the
** raw operand signedness. History in doc/compilerbugs.html; upstream
** reference in tests/compiler/upstream/FIXES.md.
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

    /* Constant rhs: the unsigned-compare strength reduction (< 1 ==> == 0)
    ** must NOT fire — the compare is signed, so -2 < 1 is 1, not (-2 == 0).
    */
    sl = -2;
    crc = crcstep (crc, (u32) (u16) (sl < (u16) 1));    /* must be 1 */
    crc = crcstep (crc, (u32) (u16) (sl > (u16) 0));    /* must be 0 */

    /* The same usual-arithmetic-conversion rule governs arithmetic:
    ** signed long / smaller unsigned is a SIGNED divide.
    */
    sl = -6;
    crc = crcstep (crc, (u32) (sl / ub));           /* -6/200 == 0  */
    crc = crcstep (crc, (u32) (sl % ub));           /* must be -6   */
    sl = -80000L;
    crc = crcstep (crc, (u32) (sl / uw));           /* -80000/40000 == -2 */
    crc = crcstep (crc, (u32) (sl % uw));           /* must be 0    */
    return crc;
}
