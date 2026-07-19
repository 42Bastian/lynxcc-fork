#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
#
# Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.

"""
Phase-4 fork-diff review (T5) regressions - design/LYNX_COMPILER_AUDIT_DESIGN.md
sec. 9. Two findings that cannot be execution-checked by the corpus:

F1  OptDeadCode vs. retained jump-table labels (coptind.c).
    A 'case k: for(;;);' body inside a table-dispatched switch is a single
    'jmp' that jumps to itself and carries the case label. The case label is
    CLF_RETAINED (referenced from the rodata jump table, invisible to the
    optimizer), but OptDeadCode's self-jump exception used to count only the
    visible reference and delete the entry - the table then dispatched into
    whatever followed. This test pins the surviving shape: the generated asm
    must still contain a self-jump, and must still dispatch through the table.

F2  Fused 'a !* b !/ c' with a long divisor (expr.c).
    The Suzy divider takes 16 bits; the divisor's type is only known after
    the fusion is committed, so a silent 16-bit truncation used to result
    (c == 0x10000 even became a divide by zero). It is now a compile error;
    the parenthesized spelling '(a !* b) !/ c' takes the standalone path and
    must keep compiling (it falls back to the software long division).

Usage:  forkdiff_t5.py               (auto-locates ../../bin/cc65,
                                      override with the CC65 env variable)
Exit:   0 pass, 1 fail, 0 with SKIP if bin/cc65 is not built.
"""

import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
CC65 = os.environ.get("CC65", os.path.join(ROOT, "bin", "cc65"))

# F1: dense char switch (>= 5 cases) so g_switchtable dispatches through a
# rodata jump table; case 3 is an infinite self-loop preceded by a 'break'
# (an OF_DEAD jmp), which is exactly the shape OptDeadCode used to delete.
SRC_F1 = r"""
unsigned char g;

void f (unsigned char v)
{
    switch (v) {
        case 0: g = 1; break;
        case 1: g = 2; break;
        case 2: g = 3; break;
        case 3: for (;;) ;
        case 4: g = 5; break;
        case 5: g = 6; break;
    }
}
"""

SRC_F2_BAD = r"""
int r;
void f (int a, int b, long c) { r = (int) (a !* b !/ c); }
"""

SRC_F2_GOOD = r"""
int r;
void f (int a, int b, long c) { r = (int) ((a !* b) !/ c); }
"""


def compile_c(src, flags):
    with tempfile.TemporaryDirectory() as tmp:
        c = os.path.join(tmp, "t.c")
        s = os.path.join(tmp, "t.s")
        with open(c, "w") as f:
            f.write(src)
        r = subprocess.run([CC65] + flags + ["-o", s, c],
                           capture_output=True, text=True)
        asm = open(s).read() if os.path.exists(s) else ""
    return r, asm


def has_self_jump(asm):
    """True if some 'jmp/bra T' targets a label attached to its own entry:
    either a 'T:' prefix on the same line, or 'T:' on the immediately
    preceding pure-label lines (an entry may carry several labels, each
    output on its own line)."""
    lines = [ln.strip() for ln in asm.splitlines()]
    for i, ln in enumerate(lines):
        m = re.match(r"(?:(L\w+):\s*)?(?:jmp|bra)\s+(L\w+)$", ln)
        if not m:
            continue
        own, target = m.group(1), m.group(2)
        if own == target:
            return True
        j = i - 1
        while j >= 0 and re.match(r"L\w+:$", lines[j]):
            if lines[j] == target + ":":
                return True
            j -= 1
    return False


def main():
    if not os.path.exists(CC65):
        print("SKIP: bin/cc65 not built")
        return 0

    failures = 0

    # ---- F1: the self-loop case body must survive optimization ----
    r, asm = compile_c(SRC_F1, ["-O"])
    if r.returncode != 0:
        print("FAIL F1: cc65 errored:\n" + r.stderr)
        failures += 1
    else:
        if not re.search(r"jmp\s+\(L\w+,x\)", asm):
            print("FAIL F1: switch no longer dispatches through a jump "
                  "table - update this test alongside g_switchtable")
            failures += 1
        elif not has_self_jump(asm):
            print("FAIL F1: 'case 3: for(;;);' self-jump was deleted - "
                  "OptDeadCode ignored the retained jump-table label")
            failures += 1
        else:
            print("PASS F1: retained self-loop survives OptDeadCode")

    # ---- F2: long divisor in the fused chain is a compile error ----
    r, _ = compile_c(SRC_F2_BAD, ["-O"])
    if r.returncode == 0:
        print("FAIL F2: fused '!* !/' with a long divisor compiled - the "
              "divisor would be silently truncated to 16 bits")
        failures += 1
    elif "16-bit" not in (r.stderr or ""):
        print("FAIL F2: compile failed but not with the fused-divisor "
              "diagnostic:\n" + r.stderr)
        failures += 1
    else:
        print("PASS F2: long fused divisor rejected with a diagnostic")

    # ---- F2 control: the parenthesized spelling must keep compiling ----
    r, _ = compile_c(SRC_F2_GOOD, ["-O"])
    if r.returncode != 0:
        print("FAIL F2: '(a !* b) !/ c' escape hatch no longer compiles:\n"
              + r.stderr)
        failures += 1
    else:
        print("PASS F2: '(a !* b) !/ c' standalone spelling compiles")

    if failures:
        return 1
    print("fork-diff T5 regressions: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
