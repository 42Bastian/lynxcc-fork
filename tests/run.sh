#!/usr/bin/env bash
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
#
# Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.

#
# CI / local entry point for the Lynx Game Development SDK test suite.
#
# Order:
#   1. unit/        host-built arithmetic checks — always run, no emulator.
#   2. compiler/    codegen regressions: compile repro C with the built
#                   bin/cc65 and pattern-check the emitted asm. SKIPS (exit 0)
#                   if bin/cc65 is not built.
#   3. integration/ boot the built examples on headless GearLynx and diff a
#                   screenshot against tests/golden/. SKIPS itself (still exit 0)
#                   when the emulator or BIOS is absent, so CI without GearLynx
#                   stays green on the unit gate alone.
#
# The examples must already be built (the top-level `make` does this before
# `make tests`); the integration step reports a hard failure if a .lnx is
# missing.
#
# Usage: tests/run.sh [--frames N]   (extra args pass through to the
#        integration driver, e.g. --frames 120 or a list of example names)
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "== unit =="
make -C "$HERE/unit"

echo
echo "== compiler =="
python3 "$HERE/compiler/member_addr_cast.py"
python3 "$HERE/compiler/forkdiff_t5.py"

echo
echo "== compiler audit =="
# Differential-optimization audit: directed corpus + fixed-seed lynxsmith
# batch, executed per -O level on headless GearLynx (skips execution when
# the emulator is absent). See design/LYNX_COMPILER_AUDIT_DESIGN.md and
# tests/compiler/harness/.
python3 "$HERE/compiler/audit.py"

echo
echo "== integration =="
python3 "$HERE/integration/gearlynx_check.py" "$@"

echo
echo "all tests passed"
