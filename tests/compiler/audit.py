#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
#
# Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.

"""
Compiler-audit CI stage (design/LYNX_COMPILER_AUDIT_DESIGN.md sec. 4/6).

Runs the differential-optimization audit over

  1. the directed corpus (tests/compiler/corpus/*.c, including the
     expected-to-diverge known-bug repros in corpus/known/), and
  2. a fixed-seed lynxsmith batch (small N, so the stage stays
     CI-friendly; larger seed sweeps run on demand straight through
     gen/lynxsmith.py + harness/differential.py).

Batches are executed in emulator-sized chunks; each chunk is one cart
per -O level on headless GearLynx. SKIPS execution (exit 0) when the
emulator or bin/cc65 is absent — the asm scan and host-oracle comparison
still run wherever possible, matching how tests/run.sh treats the other
emulator-dependent stages.

Usage: audit.py [--quick] [--seed N] [--funcs M] [--chunk K]
       --quick    corpus only, no generator batch
"""

import argparse
import glob
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(HERE, "harness"))

# The pinned CI seed: any find is reproducible from this number alone.
DEFAULT_SEED = 20260719


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--quick", action="store_true")
    ap.add_argument("--seed", type=int, default=DEFAULT_SEED)
    ap.add_argument("--funcs", type=int, default=8)
    ap.add_argument("--chunk", type=int, default=6)
    args = ap.parse_args(argv)

    if not os.path.exists(os.path.join(ROOT, "bin", "cc65")):
        print("SKIP: bin/cc65 not built")
        return 0

    import differential

    files = sorted(glob.glob(os.path.join(HERE, "corpus", "*.c")))
    files += sorted(glob.glob(os.path.join(HERE, "corpus", "known", "*.c")))

    if not args.quick:
        gen = os.path.join(HERE, "gen", "out", "ci-s%d" % args.seed)
        subprocess.run(
            [sys.executable, os.path.join(HERE, "gen", "lynxsmith.py"),
             "--seed", str(args.seed), "--funcs", str(args.funcs),
             "--out", gen],
            check=True, capture_output=True)
        files += sorted(glob.glob(os.path.join(gen, "*.c")))

    rc = 0
    for i in range(0, len(files), args.chunk):
        chunk = files[i:i + args.chunk]
        print("-- audit chunk %d/%d --"
              % (i // args.chunk + 1,
                 (len(files) + args.chunk - 1) // args.chunk))
        r = differential.main(["--tag", "audit%d" % (i // args.chunk)] + chunk)
        rc = rc or r
    if rc == 0:
        print("compiler audit: OK (%d files)" % len(files))
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
