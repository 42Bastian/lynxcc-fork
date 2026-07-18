#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
#
# Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.

"""
Compiler codegen regression: subscripting the address of a struct member
reached through a pointer must keep the member offset.

cc65 2.19's ArrayRef miscompiled

    ((unsigned char*)&s->vsize)[1] = v;     /* s is a struct pointer   */
    (&s->u)[1] = v;                         /* cast not required       */
    v = ((unsigned char*)&s->vsize)[1];     /* reads too               */

when the subscript was a CONSTANT: the pending member offset carried in the
expression descriptor (primary register holds s, IVal holds offsetof) was
*overwritten* by the subscript instead of added to it, so the access went to
((unsigned char*)s)[1] — in the raycaster this wrote the wall heights over
sprctl1 and left every vsize zero. Variable subscripts and explicit pointer
arithmetic (*(p + 1)) always went through the addition path and were correct.

Fixed in compiler/cc65/expr.c (ArrayRef, constant-subscript pointer branch):
the pending IVal is now added, not clobbered. Full analysis in
design/LYNX_MEMBER_ADDR_CAST_FIX_DESIGN.md.

This test compiles a set of repro functions with the freshly built cc65 and
asserts the emitted store/load uses the COMBINED offset (member + scaled
subscript). It pins today's codegen shapes on purpose: if the code generator
legitimately changes, update the expectations here alongside it.

Usage:  member_addr_cast.py            (auto-locates ../../bin/cc65)
Exit:   0 pass, 1 fail, 0 with SKIP if bin/cc65 is not built.
"""

import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
CC65 = os.path.join(ROOT, "bin", "cc65")

SRC = r"""
typedef struct {
    unsigned char a, b, c;      /* offsets 0..2                 */
    unsigned int  u;            /* offset 3                     */
    unsigned int  vsize;        /* offset 5 (the raycaster bug) */
} S;

S garr[4];                      /* sizeof (S) == 7              */
unsigned char rd;

/* const subscript, cast to byte pointer: store must hit s+6 (5+1) */
void w_cast (S* s, unsigned char v) { ((unsigned char*)&s->vsize)[1] = v; }

/* const subscript, no cast, int elements: store must hit s+5 (3+1*2) */
void w_nocast (S* s, unsigned char v) { (&s->u)[1] = v; }

/* read side: load must come from s+6 */
void r_cast (S* s) { rd = ((unsigned char*)&s->vsize)[1]; }

/* struct OBJECT (const base address): must fold to garr+2*7+5+1 = garr+20 */
void w_obj (unsigned char v) { ((unsigned char*)&garr[2].vsize)[1] = v; }
"""

# Each check: (function label, regex that must match inside the function
# body, human-readable description of the required offset).
CHECKS = [
    ("_w_cast",  r"ldy\s+#\$06", "combined offset 6 (vsize+1) in the store"),
    ("_w_nocast", r"ldy\s+#\$05", "combined offset 5 (u + one int) in the store"),
    ("_r_cast",  r"ldy\s+#\$06", "combined offset 6 (vsize+1) in the load"),
    # The const-base path may fold fully (garr+20) or split base+index
    # (garr+19 with Y=1); both reach byte 20.
    ("_w_obj",   r"_garr\+20|_garr\+19", "constant address garr+19/+20"),
]

# The regression itself: the buggy compiler indexed with the bare subscript.
# No function body may pair its indexed store/load with the offset-less
# ldy #$01 that the bug produced for w_cast/r_cast.
BUG_SIGNATURE = ("_w_cast", r"ldy\s+#\$01")


def body_of(asm, label):
    m = re.search(r"\.proc\s+%s:.*?\.endproc" % re.escape(label), asm, re.S)
    if not m:
        print("FAIL: no %s in generated asm" % label)
        sys.exit(1)
    return m.group(0)


def main():
    if not os.path.exists(CC65):
        print("SKIP: bin/cc65 not built")
        return 0

    with tempfile.TemporaryDirectory() as tmp:
        c = os.path.join(tmp, "repro.c")
        s = os.path.join(tmp, "repro.s")
        with open(c, "w") as f:
            f.write(SRC)
        # Same flags the examples use; the bug reproduced at every -O level.
        r = subprocess.run([CC65, "-Ors", "--codesize", "500", "-o", s, c],
                           capture_output=True, text=True)
        if r.returncode != 0:
            print("FAIL: cc65 errored:\n" + r.stderr)
            return 1
        asm = open(s).read()

    failures = 0
    for label, pattern, what in CHECKS:
        if not re.search(pattern, body_of(asm, label)):
            print("FAIL %-10s missing %s (pattern %r)" % (label, what, pattern))
            failures += 1
        else:
            print("PASS %-10s %s" % (label, what))

    label, pattern = BUG_SIGNATURE
    if re.search(pattern, body_of(asm, label)):
        print("FAIL %-10s bug signature present: subscript used without "
              "the member offset" % label)
        failures += 1

    if failures:
        return 1
    print("compiler codegen: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
