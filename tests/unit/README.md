<!--
SPDX-License-Identifier: MPL-2.0

This Source Code Form is subject to the terms of the Mozilla Public License,
v. 2.0. If a copy of the MPL was not distributed with this file, You can
obtain one at https://mozilla.org/MPL/2.0/.

Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
-->

# Unit tests (host-built)

Ordinary C programs compiled with the **host** compiler — no cc65, no emulator.
They lock in the arithmetic invariants the on-target runtime depends on, so they
run anywhere and finish in milliseconds. They are the fast first gate in
`tests/run.sh` and in CI.

## Running

```bash
make            # build + run every test; nonzero exit on any failure
make clean
HOSTCC=gcc make # override the host compiler
```

## What's here

- **`suzymath.c`** — reimplements the algorithm shape of the Suzy math routines
  (`libraries/math/`) in C and sweeps it against a trusted 64-bit reference:
  the software modulo recompute, the `d < 256` divisor-normalisation fast path
  (`design/LYNX_SUZY_DIVNORM`), the 32-bit-intermediate fused `muldiv`
  (`a !* b !/ c`), and the signed divide/mod sign-fixup. A broken invariant
  fails the sweep instead of waiting for an emulator run to surface it.

## Adding a test

Drop a `name.c` that exits nonzero on failure into this directory and add
`name` to the `TESTS` list in the `Makefile`. Keep them host-only and fast;
anything that needs the real hardware semantics belongs in `../integration`,
driven by the GearLynx emulator.
