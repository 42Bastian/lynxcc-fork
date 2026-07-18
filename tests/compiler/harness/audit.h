/* SPDX-License-Identifier: MPL-2.0
**
** This Source Code Form is subject to the terms of the Mozilla Public License,
** v. 2.0. If a copy of the MPL was not distributed with this file, You can
** obtain one at https://mozilla.org/MPL/2.0/.
**
** Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
*/

/*
** Shared header for compiler-audit test functions
** (design/LYNX_COMPILER_AUDIT_DESIGN.md).
**
** Every test function has the shape
**
**     u32 t_<name>(void);        (<name> == its .c file's basename)
**
** and folds everything it computes into a running CRC with crcstep().
** The same source compiles for BOTH oracles:
**
**   - the cc65 target build (any -O level), where u16/u32 map onto
**     cc65's 16-bit int / 32-bit long, and
**   - the host "twin" build (cc -DAUDIT_HOST), where <stdint.h> pins the
**     exact same widths.
**
** All arithmetic in test functions must be explicitly cast so the two
** agree by construction: operate in the unsigned domain, cast to the
** declared width on every store (see the semantics notes in
** tests/compiler/gen/SEMANTICS.md).
**
** Files that use fork extensions the host cc cannot parse (Suzy !* !/ !%,
** __zeropage) carry a "no-host" marker comment; the harness skips them
** when building the host twin.
*/

#ifndef AUDIT_H
#define AUDIT_H

#ifdef AUDIT_HOST
#include <stdint.h>
typedef uint8_t  u8;
typedef int8_t   i8;
typedef uint16_t u16;
typedef int16_t  i16;
typedef uint32_t u32;
typedef int32_t  i32;
#define ZP /* __zeropage is a lynxcc extension; plain storage on the host */
#else
typedef unsigned char  u8;
typedef signed   char  i8;
typedef unsigned int   u16;
typedef signed   int   i16;
typedef unsigned long  u32;
typedef signed   long  i32;
#define ZP __zeropage
#endif

/* Fold one 32-bit value into the running CRC. Pure shifts/xor/add on
** unsigned 32-bit — identical semantics on host and target, and its
** target code goes through the long runtime helpers at every -O level,
** which is itself part of the audit surface. */
static u32 crcstep (u32 c, u32 v)
{
    c = (u32) (c ^ v);
    c = (u32) ((u32) (c << 7) | (u32) (c >> 25));
    c = (u32) (c + 0x9E3779B9UL);
    return c;
}

#endif /* AUDIT_H */
