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
** Pre/post increment and decrement inside complex lvalues: subscripts,
** pointer walks, struct members, compound assignment targets. One side
** effect per expression only — the point is sequencing of the pending
** inc/dec against the surrounding lvalue computation, not UB roulette.
*/

#include "audit.h"

typedef struct {
    u8  n;
    u16 w[4];
} R;

static u8  buf[16];
static R   rec[3];
static u16 wsum;

u32 t_postinc_lvalue (void)
{
    u32 crc = 0;
    u8 i, j;
    u8 *p;
    R *rp;

    for (i = 0; i < 16; ++i)
        buf[i] = (u8) (i * 3u);

    /* post-inc index on the store side */
    i = 2;
    buf[i++] = 0xA1;            /* writes buf[2], i -> 3 */
    crc = crcstep (crc, (u32) ((u32) buf[2] + ((u32) i << 8)));

    /* pre-dec index on the store side */
    buf[--i] = 0xB2;            /* i -> 2, writes buf[2] */
    crc = crcstep (crc, (u32) ((u32) buf[2] + ((u32) i << 8)));

    /* post-inc index on the LOAD side feeding a compound assign */
    j = 5;
    wsum = 0;
    wsum += (u16) buf[j++];     /* buf[5], j -> 6 */
    wsum += (u16) buf[j++];     /* buf[6], j -> 7 */
    crc = crcstep (crc, (u32) ((u32) wsum + ((u32) j << 16)));

    /* pointer post-inc in stores and loads */
    p = buf;
    *p++ = 0x11;
    *p++ = 0x22;
    crc = crcstep (crc, (u32) *--p);            /* reads back 0x22 */
    crc = crcstep (crc, (u32) (u16) (p - buf)); /* == 1 */

    /* inc/dec of a struct member used to index a member array */
    rec[1].n = 1;
    rec[1].w[0] = 0x1000; rec[1].w[1] = 0x2000;
    rec[1].w[2] = 0x3000; rec[1].w[3] = 0x4000;
    rp = &rec[1];
    rp->w[rp->n++] = 0xCAFE;    /* writes w[1], n -> 2 */
    crc = crcstep (crc, (u32) rp->w[1]);
    crc = crcstep (crc, (u32) rp->n);
    rp->w[--rp->n] += 0x0101;   /* n -> 1, w[1] += */
    crc = crcstep (crc, (u32) rp->w[1]);
    crc = crcstep (crc, (u32) rp->n);

    /* post-inc whose VALUE is used, target is a member */
    rec[0].n = 7;
    j = rec[0].n++;             /* j = 7, n -> 8 */
    crc = crcstep (crc, (u32) ((u32) j + ((u32) rec[0].n << 8)));

    /* chained pointer walk with compound assign through post-inc */
    p = buf + 3;
    *p++ ^= 0x0F;
    *p++ |= 0xF0;
    *p++ &= 0x3C;
    crc = crcstep (crc, (u32) (u16) (p - buf));
    for (i = 0; i < 16; ++i)
        crc = crcstep (crc, (u32) buf[i]);
    return crc;
}
