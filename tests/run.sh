#!/usr/bin/env bash
#
# CI / local entry point for the Lynx Game Development SDK test suite.
#
# Order:
#   1. unit/        host-built arithmetic checks — always run, no emulator.
#   2. integration/ boot the built examples on headless GearLynx and diff a
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
echo "== integration =="
python3 "$HERE/integration/gearlynx_check.py" "$@"

echo
echo "all tests passed"
