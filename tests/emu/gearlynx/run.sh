#!/usr/bin/env bash
#
# Launch Gearlynx (Atari Lynx emulator) headless with its MCP server, for
# automated ROM verification inside the Linux sandbox.
#
# Built from source (github.com/drhelius/Gearlynx, v1.2.14) against a headless
# SDL3 3.4.10 because the released binaries target a newer glibc than the
# sandbox provides. See README.md for the full build recipe.
#
# Usage:
#   ./run.sh [extra gearlynx args] <rom.lnx>
#
# Env:
#   GLYNX_PORT   MCP HTTP port (default 7777)
#   GLYNX_BIOS   path to lynxboot.img (default: ./lynxboot.img next to this script)
#
# The MCP server then listens on http://127.0.0.1:$GLYNX_PORT/mcp (JSON-RPC).
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$HERE"                       # so the MCP resources dir (./mcp) resolves

export LD_LIBRARY_PATH="$HERE/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

PORT="${GLYNX_PORT:-7777}"
BIOS="${GLYNX_BIOS:-$HERE/lynxboot.img}"

# Portable mode keeps config/state beside the binary instead of ~/.config.
touch "$HERE/portable.ini"

# Regenerate config each launch: the BIOS path is absolute and the sandbox
# mount path changes between sessions, so it cannot be baked in once.
# Version=2 stops Gearlynx treating the file as outdated and wiping BiosPath.
cat > "$HERE/config.ini" <<EOF
[General]
Version=2

[Emulator]
BiosPath=$BIOS
EOF

exec "$HERE/gearlynx" --headless --mcp-http --mcp-http-port "$PORT" "$@"
