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
** Deep struct/array/pointer mixes: nested structs with member arrays,
** arrays of structs holding pointers into other arrays of structs,
** unions punning between the byte and word view (written and read
** through the SAME member, so both oracles agree), and every access
** spelled through both constant and variable subscripts.
*/

#include "audit.h"

typedef struct {
    u8  id;
    u16 pos[3];
} Node;

typedef struct {
    Node  n[2];
    Node *link;
    u8    pad;
} Pair;

typedef union {
    u16 w;
    u8  b[2];
} W;

static Node pool[4];
static Pair pairs[3];
static W    wu;

u32 t_structmix (void)
{
    u32 crc = 0;
    u8 i, j;
    Pair *pp;
    Node *np;
    u16 *wp;

    for (i = 0; i < 4; ++i) {
        pool[i].id = (u8) (0xC0 + i);
        for (j = 0; j < 3; ++j)
            pool[i].pos[j] = (u16) ((u16) (i + 1) * (u16) (0x111u * (u16) (j + 1)));
    }
    for (i = 0; i < 3; ++i) {
        pairs[i].n[0].id = (u8) (i * 2u);
        pairs[i].n[1].id = (u8) (i * 2u + 1u);
        for (j = 0; j < 3; ++j) {
            pairs[i].n[0].pos[j] = (u16) (0x1000u + (u16) i + (u16) j);
            pairs[i].n[1].pos[j] = (u16) (0x2000u + (u16) i + (u16) j);
        }
        pairs[i].link = &pool[(u8) (3 - i)];
        pairs[i].pad = 0x5A;
    }

    /* nested member array via pointer chains, constant subscripts */
    pp = &pairs[1];
    crc = crcstep (crc, (u32) pp->n[0].pos[2]);
    crc = crcstep (crc, (u32) pp->n[1].pos[0]);
    crc = crcstep (crc, (u32) pp->link->pos[1]);
    crc = crcstep (crc, (u32) pp->link->id);

    /* same shapes, variable subscripts */
    for (i = 0; i < 3; ++i)
        for (j = 0; j < 3; ++j)
            crc = crcstep (crc, (u32) pairs[i].n[(u8) (j & 1)].pos[j]);

    /* write through a chained lvalue, read back through another spelling */
    pairs[0].link->pos[1] = 0xBEE5;
    crc = crcstep (crc, (u32) pool[3].pos[1]);
    np = pairs[0].link;
    np->pos[0] = (u16) (np->pos[0] + 1u);
    crc = crcstep (crc, (u32) pool[3].pos[0]);

    /* pointer to a member array element, walked */
    wp = &pp->n[0].pos[0];
    wp[1] = 0x7777;
    crc = crcstep (crc, (u32) pp->n[0].pos[1]);
    for (i = 0; i < 3; ++i)
        crc = crcstep (crc, (u32) wp[i]);

    /* address-of arithmetic across the nested boundary */
    np = &pp->n[0];
    crc = crcstep (crc, (u32) (np + 1)->id);        /* pp->n[1].id */
    crc = crcstep (crc, (u32) (&pp->n[1] - &pp->n[0]));

    /* union: write word, read word; write bytes, read bytes */
    wu.w = 0x1234;
    crc = crcstep (crc, (u32) wu.w);
    wu.b[0] = 0xAB;
    wu.b[1] = 0xCD;
    crc = crcstep (crc, (u32) wu.b[0]);
    crc = crcstep (crc, (u32) wu.b[1]);

    /* whole-pool fold */
    for (i = 0; i < 4; ++i) {
        crc = crcstep (crc, (u32) pool[i].id);
        for (j = 0; j < 3; ++j)
            crc = crcstep (crc, (u32) pool[i].pos[j]);
    }
    return crc;
}
