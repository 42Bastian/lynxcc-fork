<!--
SPDX-License-Identifier: MPL-2.0

This Source Code Form is subject to the terms of the Mozilla Public License,
v. 2.0. If a copy of the MPL was not distributed with this file, You can
obtain one at https://mozilla.org/MPL/2.0/.

Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
-->

# Integration tests (GearLynx)

Boots the built example ROMs on the headless GearLynx emulator and checks that
each renders the expected frame, turning the long-standing "GearLynx 0-diff
framebuffer" practice into an automated gate.

## How it works

`gearlynx_check.py` for each example:

1. launches `../emu/gearlynx/run.sh <example>.lnx` (which sets up the BIOS and
   starts the emulator's MCP/JSON-RPC server) on a free port;
2. resets the machine and steps a fixed number of frames (`--frames`, default
   90) **with no controller input**, so the capture is deterministic;
3. requests a screenshot over MCP and SHA-256s the PNG;
4. compares that hash to `../golden/<name>.sha256`.

Because the emulator is deterministic and nothing is pressed, the same example
produces the same hash every run (verified across repeated runs).

## Examples covered

The `EXAMPLES` list in `gearlynx_check.py` is a curated set spanning the
subsystems — the `lynxdemo` starter, `suzy/spritetest`, `suzy/fonttest`,
`mikey/setbpp`, `games/breakout`. Add a name to that list and run with
`--update` to mint its golden.

## Usage

```bash
python3 gearlynx_check.py                  # compare all against golden
python3 gearlynx_check.py --frames 120     # different capture point
python3 gearlynx_check.py lynxdemo         # a subset
python3 gearlynx_check.py --update         # (re)write goldens
```

The script **skips** (exit 0) when the emulator binary, its SDL libs or the BIOS
are absent — see `../emu/gearlynx/README.md`. It only needs `python3` and
`curl`; the per-frame RPC is a one-shot `curl` because the emulator's HTTP
server does not keep connections alive cleanly.
