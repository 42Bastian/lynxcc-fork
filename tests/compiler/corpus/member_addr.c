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
** Member-address arithmetic in every shape the ArrayRef bug family can
** take (see design/LYNX_MEMBER_ADDR_CAST_FIX_DESIGN.md and the shape-
** pinned tests/compiler/member_addr_cast.py — this is the execution-
** checked version). Constant AND variable subscripts of &s->member, with
** and without casts, reads and writes, plus explicit pointer arithmetic
** as the always-correct reference shape.
*/

#include "audit.h"

typedef struct {
    u8  a, b, c;                /* offsets 0..2 */
    u16 u;                      /* offset 3     */
    u16 vsize;                  /* offset 5     */
} S;

static S garr[4];
static S *gp = &garr[2];

u32 t_member_addr (void)
{
    u32 crc = 0;
    u16 i;
    u8 vi;

    for (i = 0; i < 4; ++i) {
        garr[i].a = (u8) (i + 1);
        garr[i].b = (u8) (i + 0x10);
        garr[i].c = (u8) (i + 0x20);
        garr[i].u = (u16) (0x1111u * (u16) (i + 1));
        garr[i].vsize = (u16) (0x0101u * (u16) (i + 1));
    }

    /* constant subscript through a cast to byte pointer (the bug shape) */
    ((u8 *) &gp->vsize)[1] = 0xAB;
    ((u8 *) &gp->u)[0] = 0xCD;
    crc = crcstep (crc, (u32) gp->vsize);
    crc = crcstep (crc, (u32) gp->u);

    /* constant subscript, no cast needed (element type already u16) */
    (&gp->u)[1] = 0xBEEF;       /* writes gp->vsize (u at 3, vsize at 5) */
    crc = crcstep (crc, (u32) gp->vsize);

    /* reads with a constant subscript */
    crc = crcstep (crc, (u32) ((u8 *) &gp->vsize)[0]);
    crc = crcstep (crc, (u32) ((u8 *) &gp->vsize)[1]);
    crc = crcstep (crc, (u32) (&gp->u)[1]);

    /* variable subscript (the sibling path that was always correct) */
    for (vi = 0; vi < 2; ++vi) {
        ((u8 *) &gp->u)[vi] = (u8) (0x40 + vi);
        crc = crcstep (crc, (u32) ((u8 *) &gp->u)[vi]);
    }
    crc = crcstep (crc, (u32) gp->u);

    /* explicit pointer arithmetic — reference shape */
    *((u8 *) &gp->vsize + 1) = 0x77;
    crc = crcstep (crc, (u32) gp->vsize);

    /* member of array element (no pointer): both subscript kinds */
    ((u8 *) &garr[1].vsize)[1] = 0x55;
    for (vi = 0; vi < 2; ++vi)
        crc = crcstep (crc, (u32) ((u8 *) &garr[1].vsize)[vi]);
    crc = crcstep (crc, (u32) garr[1].vsize);

    /* whole-array fold: nothing else may have been clobbered */
    for (i = 0; i < 4; ++i) {
        crc = crcstep (crc, (u32) garr[i].a);
        crc = crcstep (crc, (u32) garr[i].b);
        crc = crcstep (crc, (u32) garr[i].c);
        crc = crcstep (crc, (u32) garr[i].u);
        crc = crcstep (crc, (u32) garr[i].vsize);
    }
    return crc;
}
