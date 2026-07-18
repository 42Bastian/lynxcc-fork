#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
#
# Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.

"""
Integration tests: boot built examples on the headless GearLynx emulator and
compare a deterministic screenshot against a committed golden hash.

Each example is launched through tests/emu/gearlynx/run.sh (which configures the
BIOS and starts the MCP server), reset, stepped a fixed number of frames with no
input, and screenshotted. The PNG is SHA-256'd and checked against
tests/golden/<name>.sha256. Because the emulator is deterministic and there is
no input, the capture is reproducible run to run.

The emulator binary, its SDL libs and the Lynx BIOS are NOT shipped (see
tests/emu/gearlynx/README.md). When any of them is missing this script SKIPS
(exit 0) rather than failing, so CI without the emulator stays green; the unit
tests are the always-on gate.

Usage:
    gearlynx_check.py [--update] [--frames N] [example ...]

    --update     write/refresh the golden hashes instead of comparing
    --frames N   frames to step before the capture (default 90)
    example ...  restrict to these example names (e.g. lynxdemo suzy/spritetest)
"""

import base64
import hashlib
import json
import os
import socket
import subprocess
import sys
import time

# Examples to verify, relative to examples/. Curated to cover each subsystem;
# extend freely — add the name here and run with --update to mint its golden.
EXAMPLES = [
    "lynxdemo",            # starter: scaled sprite + scaled text (graphics)
    "suzy/spritetest",     # Suzy sprite engine, packed vs literal
    "suzy/spritesheet",    # sp65 --sprite-sheet driver: generated frame table
    "suzy/spriteslice",    # sp65 --slice/--pop: hand-tabled frames
    "suzy/sprpcktest",     # sprpck: BMP + ASCII SPS inputs, .spr via .incbin
    "suzy/fonttest",       # TGI scaled fonts
    "suzy/collision",      # Suzy collision buffer + depository read-back
    "suzy/palette",        # gfx_setpalette vs gfx_setpalette16: same colours
    "mikey/setbpp",        # Mikey display-DMA bit depths
    "mikey/sndtune",       # snd engine: compiled ABC tune + direct SFX
    "games/breakout",      # a complete game frame
    "games/tetris",        # menu-driven falling-block puzzle
    "games/lander",        # lunar-lander physics, rotation sheets, music+sfx
]

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
EMU_DIR = os.path.join(ROOT, "tests", "emu", "gearlynx")
GOLDEN_DIR = os.path.join(ROOT, "tests", "golden")
EXAMPLES_DIR = os.path.join(ROOT, "examples")


def emulator_available():
    """All of run.sh, the binary and the BIOS must be present to run."""
    needed = [
        os.path.join(EMU_DIR, "run.sh"),
        os.path.join(EMU_DIR, "gearlynx"),
        os.path.join(EMU_DIR, "lynxboot.img"),
    ]
    return all(os.path.exists(p) for p in needed)


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


class Emu:
    """One headless GearLynx instance driven over MCP/JSON-RPC."""

    def __init__(self, rom):
        self.port = free_port()
        env = dict(os.environ, GLYNX_PORT=str(self.port))
        self.proc = subprocess.Popen(
            ["bash", "run.sh", rom],
            cwd=EMU_DIR, env=env,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        self._id = 0
        self.url = "http://127.0.0.1:%d/mcp" % self.port
        self._wait_ready()

    def _rpc(self, method, params=None):
        # A fresh curl per call: the emulator's MCP server does not keep HTTP
        # connections alive cleanly, and a one-shot request is plenty fast on
        # localhost. --max-time guards against a wedged server.
        self._id += 1
        body = json.dumps({
            "jsonrpc": "2.0", "id": self._id,
            "method": method, "params": params or {},
        })
        out = subprocess.run(
            ["curl", "-s", "--max-time", "8",
             "-H", "Content-Type:application/json",
             "-H", "Accept:application/json",
             self.url, "-d", body],
            capture_output=True, text=True,
        )
        return json.loads(out.stdout)

    def _wait_ready(self, timeout=15):
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                self._rpc("initialize", {
                    "protocolVersion": "2024-11-05", "capabilities": {},
                    "clientInfo": {"name": "ci", "version": "1"},
                })
                return
            except Exception:
                time.sleep(0.3)
        raise RuntimeError("emulator did not come up on port %d" % self.port)

    def call(self, name, args=None):
        return self._rpc("tools/call", {"name": name, "arguments": args or {}})

    def screenshot_sha(self, frames):
        self.call("debug_reset")
        for _ in range(frames):
            self.call("debug_step_frame")
        resp = self.call("get_screenshot")
        content = resp["result"]["content"]
        b64 = "".join(p.get("data", "") for p in content
                      if p.get("type") == "image")
        if not b64:
            raise RuntimeError("empty screenshot")
        return hashlib.sha256(base64.b64decode(b64)).hexdigest()

    def close(self):
        self.proc.terminate()
        try:
            self.proc.wait(timeout=5)
        except Exception:
            self.proc.kill()


def golden_path(name):
    return os.path.join(GOLDEN_DIR, name.replace("/", "__") + ".sha256")


def example_rom(name):
    """Locate a built example .lnx. Flat examples live at <name>.lnx; multi-file
    examples live in their own subdirectory as <name>/<basename>.lnx (their name
    already points at the directory). Returns the path or None if not built."""
    flat = os.path.join(EXAMPLES_DIR, name + ".lnx")
    if os.path.exists(flat):
        return flat
    nested = os.path.join(EXAMPLES_DIR, name, os.path.basename(name) + ".lnx")
    if os.path.exists(nested):
        return nested
    return None


def run(names, frames, update):
    if not emulator_available():
        print("SKIP: GearLynx emulator or BIOS not present "
              "(tests/emu/gearlynx) — see its README. Unit tests still gate.")
        return 0

    os.makedirs(GOLDEN_DIR, exist_ok=True)
    failures = 0
    for name in names:
        rom = example_rom(name)
        if rom is None:
            print("FAIL %-20s no .lnx built (run `make -C examples all`)" % name)
            failures += 1
            continue
        emu = Emu(rom)
        try:
            got = emu.screenshot_sha(frames)
        finally:
            emu.close()

        gpath = golden_path(name)
        if update:
            with open(gpath, "w") as f:
                f.write(got + "\n")
            print("update %-20s %s" % (name, got))
            continue

        if not os.path.exists(gpath):
            print("FAIL %-20s no golden (run with --update)" % name)
            failures += 1
            continue
        want = open(gpath).read().strip()
        if got == want:
            print("PASS %-20s %s" % (name, got))
        else:
            print("FAIL %-20s got %s want %s" % (name, got, want))
            failures += 1

    if failures:
        print("integration: %d failure(s)" % failures)
        return 1
    print("integration: OK" if not update else "integration: goldens written")
    return 0


def main(argv):
    update = False
    frames = 90
    names = []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "--update":
            update = True
        elif a == "--frames":
            i += 1
            frames = int(argv[i])
        else:
            names.append(a)
        i += 1
    return run(names or EXAMPLES, frames, update)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
