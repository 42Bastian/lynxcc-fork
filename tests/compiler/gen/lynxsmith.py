#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
#
# Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.

"""
lynxsmith — deterministic random C program generator for the compiler
audit (design/LYNX_COMPILER_AUDIT_DESIGN.md sec. T2; semantics rules in
SEMANTICS.md next to this file).

Emits self-checking test functions in the cc65 subset, one file per
function (file fs<seed>_<i>.c defines u32 t_fs<seed>_<i>(void)), each
folding everything it computes into a CRC. Well-defined by construction:
explicitly cast operations in an unsigned compute domain, masked shifts,
guarded divisors, masked subscripts, no uninitialized reads. The same
source builds with cc65 (any -O) and with a host compiler (-DAUDIT_HOST)
— except --suzy files, which use fork-only syntax and are marked no-host.

Usage:
    lynxsmith.py --seed N [--funcs M] [--out DIR] [--suzy]

Same seed, same files, byte for byte — a CI stage can pin a seed and any
find is reproducible from the seed alone.
"""

import argparse
import os
import random
import sys

TYPES = {
    "u8":  (8,  False), "i8":  (8,  True),
    "u16": (16, False), "i16": (16, True),
    "u32": (32, False), "i32": (32, True),
}


def dom(t):
    """Unsigned compute domain for a result type."""
    return "u32" if TYPES[t][0] == 32 else "u16"


def dbits(t):
    return 32 if dom(t) == "u32" else 16


class Fn:
    def __init__(self, name, rng, suzy):
        self.name = name
        self.rng = rng
        self.suzy = suzy
        self.vars = []          # (name, type)  scalars
        self.arrs = []          # (name, type, size)  size is a power of two
        self.body = []
        self.decl = []
        self.tmp = 0

    # ---- variable pool ---------------------------------------------------

    def declare(self):
        r = self.rng
        for i in range(r.randint(3, 5)):
            t = r.choice(list(TYPES))
            n = "v%d" % i
            self.vars.append((n, t))
            # storage class BEFORE the type: cc65 2.19 rejects the
            # (legal) postfix spelling — upstream fixed that in
            # 8a7f566387, see upstream/FIXES.md
            self.decl.append("    %s%s %s = (%s) 0x%Xu;"
                             % ("" if r.random() < 0.7 else "register ",
                                t, n, t, r.getrandbits(TYPES[t][0])))
        for i in range(r.randint(1, 2)):
            t = r.choice(["u8", "u16", "i16", "u32"])
            size = r.choice([4, 8])
            n = "a%d" % i
            self.arrs.append((n, t, size))
            init = ", ".join("(%s) 0x%Xu" % (t, r.getrandbits(TYPES[t][0]))
                             for _ in range(size))
            self.decl.append("    static %s %s[%d] = { %s };"
                             % (t, n, size, init))

    def fix_decl(self):
        """cc65 rejects 'register' on some shapes only at -Ors margins;
        keep register scalars but never register + 32-bit (costly, and
        the 2-slot bank makes it pointless)."""
        self.decl = [d.replace("register u32", "u32")
                      .replace("register i32", "i32") for d in self.decl]

    # ---- expressions -----------------------------------------------------

    def const(self, u):
        bits = 32 if u == "u32" else 16
        return "(%s) 0x%X%s" % (u, self.rng.getrandbits(bits),
                                "UL" if bits == 32 else "u")

    def atom(self, u):
        r = self.rng
        c = r.random()
        if c < 0.35 or not self.vars:
            return self.const(u)
        if c < 0.75:
            n, t = r.choice(self.vars)
            return "(%s) %s" % (u, n)
        n, t, size = r.choice(self.arrs)
        if r.random() < 0.5:
            idx = "%d" % r.randrange(size)              # constant subscript
        else:
            vn, vt = r.choice(self.vars)
            idx = "(u8) ((u8) %s & %du)" % (vn, size - 1)  # variable one
        return "(%s) %s[%s]" % (u, n, idx)

    def expr(self, u, depth):
        r = self.rng
        if depth <= 0:
            return self.atom(u)
        op = r.choice(["+", "-", "*", "&", "|", "^", "<<", ">>",
                       "/", "%", "<", ">=", "==", "?"])
        a = self.expr(u, depth - 1)
        b = self.atom(u) if r.random() < 0.5 else self.expr(u, depth - 1)
        if op in ("+", "-", "*", "&", "|", "^"):
            return "(%s) ((%s) %s %s (%s) %s)" % (u, u, a, op, u, b)
        if op in ("<<", ">>"):
            m = dbits("u32" if u == "u32" else "u16") - 1
            return "(%s) ((%s) %s %s ((%s) %s & %du))" % (u, u, a, op, u, b, m)
        if op in ("/", "%"):
            return "(%s) ((%s) %s %s ((%s) %s | 1u))" % (u, u, a, op, u, b)
        if op == "?":
            c = self.atom(u)
            return ("(%s) ((%s) %s != 0u ? (%s) %s : (%s) %s)"
                    % (u, u, c, u, a, u, b))
        return "(%s) ((%s) ((%s) %s %s (%s) %s))" % (u, u, u, a, op, u, b)

    def typed(self, t, depth):
        return "(%s) %s" % (t, self.expr(dom(t), depth))

    # ---- statements ------------------------------------------------------

    def stmt(self, depth=2, indent="    "):
        r = self.rng
        c = r.random()
        if c < 0.40:
            n, t = r.choice(self.vars)
            return "%s%s = %s;" % (indent, n, self.typed(t, depth))
        if c < 0.55:
            n, t, size = self.arrs[r.randrange(len(self.arrs))]
            if r.random() < 0.5:
                idx = "%d" % r.randrange(size)
            else:
                vn, vt = r.choice(self.vars)
                idx = "(u8) ((u8) %s & %du)" % (vn, size - 1)
            return "%s%s[%s] = %s;" % (indent, n, idx, self.typed(t, depth))
        if c < 0.65:
            n, t = r.choice([v for v in self.vars] or [("v0", "u16")])
            if TYPES[t][1]:     # compound assign only on unsigned (SEMANTICS)
                return "%s%s = %s;" % (indent, n, self.typed(t, depth))
            op = r.choice(["+=", "^=", "|=", "&="])
            return "%s%s %s %s;" % (indent, n, op, self.typed(t, 1))
        if c < 0.80 and indent == "    ":
            cond = self.expr("u16", 1)
            s1 = self.stmt(1, indent + "    ")
            s2 = self.stmt(1, indent + "    ")
            return ("%sif ((u16) %s != 0u) {\n%s\n%s} else {\n%s\n%s}"
                    % (indent, cond, s1, indent, s2, indent))
        if indent == "    ":
            self.tmp += 1
            it = "it%d" % self.tmp
            k = r.randint(2, 6)
            s1 = self.stmt(1, indent + "    ")
            return ("%s{ u8 %s; for (%s = 0; %s < %du; ++%s) {\n%s\n%s} }"
                    % (indent, it, it, it, k, it, s1, indent))
        n, t = self.rng.choice(self.vars)
        return "%s%s = %s;" % (indent, n, self.typed(t, 1))

    def suzy_stmts(self):
        """Suzy-operator statements next to their software-C twins."""
        r = self.rng
        out = []
        v16 = [(n, t) for n, t in self.vars if TYPES[t][0] == 16] \
            or [("v0", self.vars[0][1])]
        a, at = r.choice(v16)
        b, bt = r.choice(v16)
        c, ct = r.choice(v16)
        out.append("    crc = crcstep (crc, (u32) (u16) "
                   "((u16) %s !* (u16) %s));" % (a, b))
        out.append("    crc = crcstep (crc, (u32) (u16) "
                   "((u16) %s * (u16) %s));" % (a, b))
        out.append("    crc = crcstep (crc, (u32) (u16) ((u16) %s !/ "
                   "((u16) %s | 1u)));" % (a, b))
        out.append("    crc = crcstep (crc, (u32) (u16) ((u16) %s / "
                   "((u16) %s | 1u)));" % (a, b))
        out.append("    crc = crcstep (crc, (u32) (u16) ((u16) %s !%% "
                   "((u16) %s | 1u)));" % (b, c))
        out.append("    crc = crcstep (crc, (u32) (u16) ((u16) %s %% "
                   "((u16) %s | 1u)));" % (b, c))
        out.append("    crc = crcstep (crc, (u32) (u16) ((u16) %s !* (u16) %s "
                   "!/ ((u16) %s | 1u)));" % (a, b, c))
        out.append("    crc = crcstep (crc, (u32) (u16) (((u32) (u16) %s * "
                   "(u32) (u16) %s) / (u32) ((u16) %s | 1u)));" % (a, b, c))
        return out

    # ---- assembly --------------------------------------------------------

    def emit(self):
        r = self.rng
        self.declare()
        self.fix_decl()
        n_stmt = r.randint(8, 16)
        stmts = [self.stmt() for _ in range(n_stmt)]
        fold = []
        for n, t in self.vars:
            u = "u%d" % TYPES[t][0]
            fold.append("    crc = crcstep (crc, (u32) (%s) %s);" % (u, n))
        for n, t, size in self.arrs:
            u = "u%d" % TYPES[t][0]
            self.tmp += 1
            it = "it%d" % self.tmp
            fold.append("    { u8 %s; for (%s = 0; %s < %du; ++%s)\n"
                        "        crc = crcstep (crc, (u32) (%s) %s[%s]); }"
                        % (it, it, it, size, it, u, n, it))
        lines = []
        if self.suzy:
            lines.append("/* no-host: generated in --suzy mode "
                         "(fork-only operators) */")
        lines += [
            "/* generated by lynxsmith.py — DO NOT EDIT; regenerate from "
            "the seed */",
            '#include "audit.h"',
            "",
            "u32 %s (void)" % self.name,
            "{",
            "    u32 crc = 0;",
        ]
        lines += self.decl
        lines.append("")
        lines += stmts
        if self.suzy:
            lines += self.suzy_stmts()
        lines += fold
        lines += ["    return crc;", "}", ""]
        return "\n".join(lines)


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, required=True)
    ap.add_argument("--funcs", type=int, default=8)
    ap.add_argument("--out", default=None)
    ap.add_argument("--suzy", action="store_true")
    args = ap.parse_args(argv)

    out = args.out or os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                   "out", "s%d" % args.seed)
    os.makedirs(out, exist_ok=True)
    paths = []
    for i in range(args.funcs):
        rng = random.Random((args.seed << 8) ^ i ^ (0x515A if args.suzy else 0))
        name = "fs%d_%d%s" % (args.seed, i, "s" if args.suzy else "")
        fn = Fn("t_" + name, rng, args.suzy)
        path = os.path.join(out, name + ".c")
        with open(path, "w") as f:
            f.write(fn.emit())
        paths.append(path)
    print("\n".join(paths))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
