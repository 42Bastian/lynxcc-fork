#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
#
# Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.

"""
GearLynx executor for audit batch carts
(design/LYNX_COMPILER_AUDIT_DESIGN.md sec. T3).

Boots a cart on the headless GearLynx (tests/emu/gearlynx, MCP/JSON-RPC),
resets, clears the completion magic, steps frames until the cart raises it
(or a frame budget runs out), then reads the CRC result table back over
read_memory. Follows the same MCP-over-curl pattern as
tests/integration/gearlynx_check.py.
"""

import json
import os
import socket
import subprocess
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
EMU_DIR = os.path.join(ROOT, "tests", "emu", "gearlynx")

RAM_AREA = 0  # list_memory_areas: id 0 = full $0000-$FFFF CPU view


def emulator_available():
    needed = [os.path.join(EMU_DIR, p)
              for p in ("run.sh", "gearlynx", "lynxboot.img")]
    return all(os.path.exists(p) for p in needed)


def _free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


class Emu:
    """One headless GearLynx instance driven over MCP/JSON-RPC."""

    def __init__(self, rom):
        self.port = _free_port()
        env = dict(os.environ, GLYNX_PORT=str(self.port))
        self.proc = subprocess.Popen(
            ["bash", "run.sh", os.path.abspath(rom)],
            cwd=EMU_DIR, env=env,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        self._id = 0
        self.url = "http://127.0.0.1:%d/mcp" % self.port
        self._wait_ready()

    def _rpc(self, method, params=None):
        self._id += 1
        body = json.dumps({"jsonrpc": "2.0", "id": self._id,
                           "method": method, "params": params or {}})
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
                    "clientInfo": {"name": "audit", "version": "1"},
                })
                return
            except Exception:
                time.sleep(0.3)
        raise RuntimeError("emulator did not come up on port %d" % self.port)

    def call(self, name, args=None):
        return self._rpc("tools/call", {"name": name, "arguments": args or {}})

    def read_bytes(self, addr, size):
        resp = self.call("read_memory", {
            "area": RAM_AREA, "offset": "%04X" % addr, "size": size,
        })
        text = resp["result"]["content"][0]["text"]
        return bytes(int(b, 16) for b in json.loads(text)["data"].split())

    def write_bytes(self, addr, data):
        self.call("write_memory", {
            "area": RAM_AREA, "offset": "%04X" % addr,
            "bytes": " ".join("%02X" % b for b in data),
        })

    def close(self):
        self.proc.terminate()
        try:
            self.proc.wait(timeout=5)
        except Exception:
            self.proc.kill()


def run_cart(build, max_frames=3600, poll=5):
    """Execute one built batch cart (dict from cartbuild.build). Returns the
    list of 32-bit CRCs in function order. Raises on timeout."""
    from cartbuild import MAGIC
    emu = Emu(build["rom"])
    try:
        emu.call("debug_reset")
        emu.write_bytes(build["done_addr"], [0, 0, 0, 0])
        frames = 0
        while frames < max_frames:
            for _ in range(poll):
                emu.call("debug_step_frame")
            frames += poll
            if tuple(emu.read_bytes(build["done_addr"], 4)) == MAGIC:
                break
        else:
            raise RuntimeError("cart %s: no completion magic after %d frames"
                               % (build["rom"], max_frames))
        raw = emu.read_bytes(build["results_addr"], 4 * len(build["names"]))
        return [raw[i] | (raw[i + 1] << 8) | (raw[i + 2] << 16)
                | (raw[i + 3] << 24) for i in range(0, len(raw), 4)]
    finally:
        emu.close()
