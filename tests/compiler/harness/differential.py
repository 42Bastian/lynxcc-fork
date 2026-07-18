#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
#
# Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.

"""
Differential optimization audit driver
(design/LYNX_COMPILER_AUDIT_DESIGN.md sec. T1/T3/T4).

Builds one batch cart from the given test .c files at every optimization
level, executes each build on headless GearLynx, and compares the per-
function CRC tables. The unoptimized build (O0) is the oracle; any
divergence at -O/-Oi/-Ors is a compiler bug. A host-cc twin build is the
second oracle: host vs O0 disagreement means the harness or the test's
semantics are broken, not the compiler.

The static asm scan (cartbuild.scan_asm) rides along on every build:
generated code must never touch $FC00-$FDFF unless the source is marked
"hw-ok".

On a divergence, --bisect names the culprit optimizer pass by re-building
the single offending function with --disable-opt subsets (binary search
over `cc65 --list-opt-steps`); if disabling ALL passes still diverges the
bug is in base codegen, not a peephole.

Exit: 0 clean, 1 divergence/scan hit, 2 infrastructure problem.
SKIPs execution (still exit 0) when the emulator is absent; the asm scan
and host-oracle comparison still run.

Usage:
    differential.py [options] file.c [file.c ...]
        --levels O0,O,Oi,Ors   levels to test (default all; O0 always added)
        --builddir DIR         work dir (default harness/build)
        --tag NAME             subdirectory tag inside builddir (default batch)
        --max-frames N         emulation frame budget per cart (default 3600)
        --no-emu               build + scan + host oracle only
        --no-host              skip the host twin oracle
        --bisect               on divergence, bisect the culprit pass
        --asm-diff             on divergence, print .s diff stats per level
"""

import argparse
import os
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import cartbuild
import emurun
import hostrun

ROOT = cartbuild.ROOT


def list_opt_steps():
    out = subprocess.run([os.path.join(ROOT, "bin", "cc65"),
                          "--list-opt-steps"],
                         capture_output=True, text=True)
    steps = [l.strip() for l in out.stdout.splitlines()
             if l.strip() and not l.strip().startswith("Usage")]
    return [s for s in steps if s.isidentifier()]


def build_all(sources, levels, builddir, tag, extra=None):
    builds, scan_hits = {}, []
    for lv in levels:
        d = os.path.join(builddir, tag, lv)
        if os.path.isdir(d):
            shutil.rmtree(d)
        builds[lv] = cartbuild.build(sources, lv, d, extra_cflags=extra,
                                     quiet=True)
        for s in builds[lv]["asm"]:
            for no, line in cartbuild.scan_asm(s):
                scan_hits.append((lv, os.path.basename(s), no, line))
    return builds, scan_hits


def diverging(base, other, names):
    return [n for n, a, b in zip(names, base, other) if a != b]


def bisect_passes(source, level, builddir, max_frames):
    """Minimise the set of optimizer passes that must stay ENABLED for the
    single-function cart to diverge from O0. Returns (culprits, base_codegen)."""
    steps = list_opt_steps()
    if not steps:
        return [], False

    def crc_with_disabled(disabled, tag):
        d = os.path.join(builddir, "bisect", tag)
        if os.path.isdir(d):
            shutil.rmtree(d)
        extra = []
        for s in disabled:
            extra += ["--disable-opt", s]
        b = cartbuild.build([source], level, d, extra_cflags=extra, quiet=True)
        return emurun.run_cart(b, max_frames=max_frames)[0]

    oracle = crc_with_disabled(steps, "all-off")   # everything disabled
    bad = crc_with_disabled([], "all-on")
    if oracle == bad:
        return [], False          # can't reproduce on the isolated function
    # if even all-off diverged from the O0 build's value the bug is upstream
    # of the optimizer; caller compares oracle against the O0 table entry.

    # Delta-debug: find a minimal set of passes whose ENABLING diverges.
    enabled = list(steps)
    chunk = max(1, len(enabled) // 2)
    it = 0
    while chunk >= 1 and it < 64:
        it += 1
        shrunk = False
        for i in range(0, len(enabled), chunk):
            trial = enabled[:i] + enabled[i + chunk:]
            disabled = [s for s in steps if s not in trial]
            if crc_with_disabled(disabled, "trial") != oracle:
                enabled = trial
                shrunk = True
                break
        if not shrunk:
            if chunk == 1:
                break
            chunk = max(1, chunk // 2)
    return enabled, False


def asm_diff_stats(builds, base_lv, func, levels):
    import difflib
    sname = func[2:] + ".s"      # t_foo -> foo.s
    base = [a for a in builds[base_lv]["asm"] if a.endswith(sname)][0]
    with open(base) as f:
        base_lines = f.readlines()
    for lv in levels:
        if lv == base_lv:
            continue
        other = [a for a in builds[lv]["asm"] if a.endswith(sname)][0]
        with open(other) as f:
            other_lines = f.readlines()
        d = list(difflib.unified_diff(base_lines, other_lines,
                                      base, other, n=1))
        print("---- %s vs %s: %d diff lines (%s)"
              % (base_lv, lv, len(d), other))


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("sources", nargs="+")
    ap.add_argument("--levels", default=",".join(cartbuild.LEVELS))
    ap.add_argument("--builddir",
                    default=os.path.join(os.path.dirname(
                        os.path.abspath(__file__)), "build"))
    ap.add_argument("--tag", default="batch")
    ap.add_argument("--max-frames", type=int, default=3600)
    ap.add_argument("--no-emu", action="store_true")
    ap.add_argument("--no-host", action="store_true")
    ap.add_argument("--bisect", action="store_true")
    ap.add_argument("--asm-diff", action="store_true")
    args = ap.parse_args(argv)

    sources = [os.path.abspath(s) for s in args.sources]
    for s in sources:
        if not os.path.exists(s):
            print("no such file: %s" % s)
            return 2
    levels = args.levels.split(",")
    if "O0" not in levels:
        levels.insert(0, "O0")

    builds, scan_hits = build_all(sources, levels, args.builddir, args.tag)
    names = builds["O0"]["names"]
    print("built %d function(s) at %s" % (len(names), ",".join(levels)))

    failures = 0
    for lv, f, no, line in scan_hits:
        print("ASMSCAN %-4s %s:%d: hardware-page access not in source: %s"
              % (lv, f, no, line.strip()))
        failures += 1

    # --- host twin oracle --------------------------------------------------
    host = None
    if not args.no_host:
        hostable = [s for s in sources if not cartbuild.is_no_host(s)]
        if hostable:
            host = hostrun.run_host(hostable,
                                    os.path.join(args.builddir, args.tag,
                                                 "host"))
            if host is None:
                print("host oracle: SKIP (no host cc)")

    # --- execution ---------------------------------------------------------
    if args.no_emu or not emurun.emulator_available():
        if not args.no_emu:
            print("SKIP execution: GearLynx emulator/BIOS not present "
                  "(tests/emu/gearlynx)")
        return 1 if failures else 0

    crcs = {lv: emurun.run_cart(builds[lv], max_frames=args.max_frames)
            for lv in levels}

    known = set(cartbuild.func_name(s) for s in sources
                if cartbuild.is_known_bug(s))
    known_hit = set()

    if host is not None:
        for i, n in enumerate(names):
            if n in host and host[n] != crcs["O0"][i]:
                if n in known:
                    print("XFAIL   %-24s host=%08X O0=%08X (known-bug)"
                          % (n, host[n], crcs["O0"][i]))
                    known_hit.add(n)
                else:
                    print("ORACLE-MISMATCH %-24s host=%08X O0=%08X "
                          "(semantics bug in the test, OR a base-codegen "
                          "miscompile live at every -O level)"
                          % (n, host[n], crcs["O0"][i]))
                    failures += 1

    clean = True
    for lv in levels:
        if lv == "O0":
            continue
        bad = diverging(crcs["O0"], crcs[lv], names)
        for n in bad:
            i = names.index(n)
            if n in known:
                print("XFAIL %-4s %-22s O0=%08X %s=%08X (known-bug)"
                      % (lv, n, crcs["O0"][i], lv, crcs[lv][i]))
                known_hit.add(n)
                continue
            print("DIVERGE %-4s %-24s O0=%08X %s=%08X"
                  % (lv, n, crcs["O0"][i], lv, crcs[lv][i]))
            clean = False
            failures += 1
            if args.asm_diff:
                asm_diff_stats(builds, "O0", n, [lv])
            if args.bisect:
                src = [s for s in sources
                       if cartbuild.func_name(s) == n][0]
                culprits, _ = bisect_passes(src, lv, args.builddir,
                                            args.max_frames)
                if culprits:
                    print("  culprit pass(es): %s" % ", ".join(culprits))
                else:
                    print("  not reproducible in isolation, or base codegen "
                          "(diverges with every pass disabled)")

    # A known-bug file that stopped diverging means the fix has landed:
    # the stale marker must be removed (and the file becomes a normal,
    # always-green corpus regression), so flag it loudly.
    for n in sorted(known - known_hit):
        if host is not None or len(levels) > 1:
            print("XPASS   %-24s known-bug marker is STALE — the bug no "
                  "longer reproduces; remove the marker" % n)
            failures += 1

    if clean and not failures:
        print("differential: OK (%d functions x %d levels, %d known-bug)"
              % (len(names), len(levels), len(known_hit)))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
