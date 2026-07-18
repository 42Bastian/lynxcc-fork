#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
#
# Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.

"""
Batch-cart builder for the compiler audit
(design/LYNX_COMPILER_AUDIT_DESIGN.md sec. T3).

Takes a list of test .c files (each defining `u32 t_<basename>(void)`, see
harness/audit.h), generates a driver main() that calls every function in
order and stores each 32-bit result little-endian into a result table, then
writes a 4-byte completion magic. Builds the whole batch into one .lnx at a
given optimization level, so one emulator boot amortises over the batch.

Everything for one (batch, level) lands in its own build directory:
sources are copied there so cc65/cl65 intermediates (.s/.o/.map) stay out
of the corpus tree and levels never collide. The kept .s files feed the
static asm scan and per-level diffing. Linking runs from the repo root with
CC65_HOME set so cl65 resolves cfg/lynx.cfg and the SDK library manifest.

Result-table addresses are recovered from the VICE label file (-Ln), not a
hard-coded address, so the layout can never drift out from under the runner.
"""

import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
CL65 = os.path.join(ROOT, "bin", "cl65")

MAGIC = (0x4C, 0x59, 0x4E, 0x58)  # "LYNX"

# Optimization levels under test (design sec. T1). Key -> cc65 flags.
# "O0" (no optimizer) is the oracle build.
LEVELS = {
    "O0":  [],
    "O":   ["-O"],
    "Oi":  ["-Oi"],
    "Ors": ["-Ors"],
}


def func_name(path):
    """foo/bar/sdiv_bounds.c -> t_sdiv_bounds"""
    return "t_" + os.path.splitext(os.path.basename(path))[0]


def is_no_host(path):
    """True if the file opts out of the host twin oracle (fork-only syntax)."""
    with open(path) as f:
        return "no-host" in f.read(2048)


def is_known_bug(path):
    """True if the file is an expected-to-diverge regression for a known,
    not-yet-fixed miscompile (design sec. 5: the repro lands in the corpus
    BEFORE the fix). The harness reports divergence as XFAIL and flags the
    marker as stale once the file starts passing."""
    with open(path) as f:
        return "known-bug" in f.read(2048)


def gen_driver(names):
    """Driver source: call each test fn, store CRCs, raise the magic."""
    lines = [
        '#include "audit.h"',
        "",
    ]
    for n in names:
        lines.append("extern u32 %s (void);" % n)
    lines += [
        "",
        "unsigned char audit_results[%d];" % (4 * len(names)),
        "volatile unsigned char audit_done[4];",
        "",
        "static void store (unsigned char *p, u32 v)",
        "{",
        "    p[0] = (unsigned char) v;",
        "    p[1] = (unsigned char) (v >> 8);",
        "    p[2] = (unsigned char) (v >> 16);",
        "    p[3] = (unsigned char) (v >> 24);",
        "}",
        "",
        "int main (void)",
        "{",
    ]
    for i, n in enumerate(names):
        lines.append("    store (audit_results + %d, %s ());" % (4 * i, n))
    lines += [
        "    audit_done[0] = 0x%02X;" % MAGIC[0],
        "    audit_done[1] = 0x%02X;" % MAGIC[1],
        "    audit_done[2] = 0x%02X;" % MAGIC[2],
        "    audit_done[3] = 0x%02X;" % MAGIC[3],
        "    for (;;) ;",
        "    return 0;",
        "}",
        "",
    ]
    return "\n".join(lines)


def parse_labels(lblfile):
    """VICE label file -> {name: address} for the audit symbols."""
    want = {"._audit_results": "results", "._audit_done": "done"}
    out = {}
    with open(lblfile) as f:
        for line in f:
            m = re.match(r"al\s+([0-9A-Fa-f]+)\s+(\S+)", line)
            if m and m.group(2) in want:
                out[want[m.group(2)]] = int(m.group(1), 16)
    missing = set(want.values()) - set(out)
    if missing:
        raise RuntimeError("labels missing from %s: %s" % (lblfile, missing))
    return out


def build(sources, level, builddir, extra_cflags=None, quiet=False):
    """Build one batch cart. Returns dict with rom path, label addresses,
    function order and the kept per-function .s paths."""
    if level not in LEVELS:
        raise ValueError("unknown level %r" % level)
    os.makedirs(builddir, exist_ok=True)

    local = []
    for src in sources:
        dst = os.path.join(builddir, os.path.basename(src))
        shutil.copyfile(src, dst)
        local.append(dst)
    names = [func_name(s) for s in local]
    if len(set(names)) != len(names):
        raise RuntimeError("duplicate test basenames in batch")

    driver = os.path.join(builddir, "audit_main.c")
    with open(driver, "w") as f:
        f.write(gen_driver(names))

    env = dict(os.environ, CC65_HOME=ROOT)
    cc65 = os.path.join(ROOT, "bin", "cc65")

    # Compile each unit to .s explicitly (cl65 would delete the intermediate
    # asm after assembling, and the kept .s feeds the scan and diffing) ...
    asms = []
    for src in [driver] + local:
        sfile = os.path.splitext(src)[0] + ".s"
        cmd = ([cc65, "-g"] + LEVELS[level] + (extra_cflags or [])
               + ["-I", HERE, "-o", sfile, src])
        r = subprocess.run(cmd, cwd=ROOT, env=env,
                           capture_output=True, text=True)
        if r.returncode != 0:
            raise RuntimeError("compile failed (%s):\n%s\n%s"
                               % (level, " ".join(cmd), r.stderr or r.stdout))
        if not quiet and (r.stderr or "").strip():
            sys.stderr.write(r.stderr)
        asms.append(sfile)

    # ... then assemble + link the batch through cl65 (SDK auto-libs).
    rom = os.path.join(builddir, "audit.lnx")
    lbl = os.path.join(builddir, "audit.lbl")
    cmd = [CL65, "-g", "-o", rom,
           "-m", os.path.join(builddir, "audit.map"), "-Ln", lbl] + asms
    r = subprocess.run(cmd, cwd=ROOT, env=env, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError("link failed (%s):\n%s\n%s"
                           % (level, " ".join(cmd), r.stderr or r.stdout))

    labels = parse_labels(lbl)
    return {
        "rom": rom,
        "results_addr": labels["results"],
        "done_addr": labels["done"],
        "names": names,
        "asm": [os.path.splitext(p)[0] + ".s" for p in local],
    }


# ----------------------------------------------------------------------------
# Static asm scan (design sec. 4): generated code must never touch the
# hardware pages $FC00-$FDFF unless the SOURCE asked for it (files marked
# "hw-ok"). Generalises the f93733c8f Opt65C02BitOps guard into a checked
# invariant over every pass.

# An INSTRUCTION operand in $FC00-$FDFF. Data directives (.word/.byte...)
# holding such a value are just constants, and #$FCxx immediates are
# constants too — only a memory-addressing operand can touch hardware.
HW_RE = re.compile(r"\$F[CD][0-9A-F]{2}\b", re.IGNORECASE)


def scan_asm(sfile):
    """Return list of (lineno, line) hardware-page hits in generated asm."""
    src = os.path.splitext(sfile)[0] + ".c"
    if os.path.exists(src):
        with open(src) as f:
            if "hw-ok" in f.read(2048):
                return []
    hits = []
    with open(sfile) as f:
        for no, line in enumerate(f, 1):
            code = line.split(";", 1)[0]
            stripped = code.strip()
            if not stripped or stripped.startswith("."):
                continue                      # directive or blank
            m = HW_RE.search(code)
            if m and code[:m.start()].rstrip()[-1:] != "#":
                hits.append((no, line.rstrip()))
    return hits
