#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
#
# Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.

"""
Host "twin" oracle (design/LYNX_COMPILER_AUDIT_DESIGN.md sec. T2).

Compiles the SAME test sources with the host C compiler (-DAUDIT_HOST pins
exact 8/16/32-bit types via <stdint.h> in audit.h) plus a printing driver,
runs the binary, and returns the per-function CRCs. A second, independent
oracle next to the unoptimized cc65 build: if the two oracles disagree with
each other the harness or the test's semantics are broken, not the compiler.

Files marked "no-host" (fork-only syntax: Suzy operators, __zeropage tests
that need target layout) are the caller's job to filter out.
"""

import os
import shutil
import subprocess

from cartbuild import func_name

HERE = os.path.dirname(os.path.abspath(__file__))


def host_cc():
    for cc in (os.environ.get("CC"), "cc", "gcc", "clang"):
        if cc and shutil.which(cc):
            return cc
    return None


def run_host(sources, builddir):
    """Build + run the host twin. Returns {func_name: crc} or None if no
    host compiler is available."""
    cc = host_cc()
    if cc is None:
        return None
    os.makedirs(builddir, exist_ok=True)
    names = [func_name(s) for s in sources]

    driver = os.path.join(builddir, "host_main.c")
    with open(driver, "w") as f:
        f.write('#include <stdio.h>\n#include "audit.h"\n\n')
        for n in names:
            f.write("extern u32 %s (void);\n" % n)
        f.write("\nint main (void)\n{\n")
        for n in names:
            f.write('    printf ("%s %%08lX\\n", (unsigned long) %s ());\n'
                    % (n, n))
        f.write("    return 0;\n}\n")

    exe = os.path.join(builddir, "host_audit")
    cmd = [cc, "-O2", "-fwrapv", "-DAUDIT_HOST", "-I", HERE,
           "-o", exe, driver] + list(sources)
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError("host build failed:\n%s" % (r.stderr or r.stdout))

    out = subprocess.run([exe], capture_output=True, text=True, timeout=30)
    if out.returncode != 0:
        raise RuntimeError("host twin crashed (exit %d)" % out.returncode)
    crcs = {}
    for line in out.stdout.splitlines():
        n, v = line.split()
        crcs[n] = int(v, 16)
    return crcs
