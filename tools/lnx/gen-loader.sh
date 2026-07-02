#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
#
# Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
#
# gen-loader.sh -- regenerate tools/lnx/blloader_gen.c from the SDK's own
# bootloader (runtime/lynx/bootldr.s). For each supported cart block size it
# links a minimal program with cfg/lynx.cfg (overriding __BANK0BLOCKSIZE__),
# strips the 64-byte LYNX header, and captures the 203-byte loader region
# (offsets 0x00..0xCA, i.e. up to __STARTOFDIRECTORY__). The tool then writes
# its own 8-byte directory + body after this region.
#
# The output is a COMMITTED generated artifact; run this after a full build to
# refresh it (top-level `make lnx-loader-gen`). See
# design/LYNX_LNX_BLL_ROM_DESIGN.md.

set -eu

# Locate the repo root relative to this script (tools/lnx/gen-loader.sh).
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)

CL65="${CL65:-$root/bin/cl65}"
CFG="$root/cfg/lynx.cfg"
OUT="$here/blloader_gen.c"

export CC65_HOME="${CC65_HOME:-$root}"

if [ ! -x "$CL65" ]; then
    echo "gen-loader.sh: $CL65 not found; build the toolchain first" >&2
    exit 1
fi

LOADER_LEN=203          # 0xCB == __STARTOFDIRECTORY__

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/stub.c" <<'EOF'
/* Minimal program used only to link the SDK loader region; never shipped. */
int main(void) { for (;;) {} return 0; }
EOF

emit_array() {
    # $1 = block size hex, $2 = C identifier suffix
    bs=$1; name=$2
    "$CL65" -t lynx -C "$CFG" -Wl -D,__BANK0BLOCKSIZE__=$bs \
            -o "$tmp/out.lnx" "$tmp/stub.c"
    # Strip the 64-byte LYNX header, keep the 203-byte loader region.
    tail -c +65 "$tmp/out.lnx" | head -c $LOADER_LEN > "$tmp/loader.bin"
    got=$(wc -c < "$tmp/loader.bin")
    if [ "$got" -ne "$LOADER_LEN" ]; then
        echo "gen-loader.sh: loader region for $bs was $got bytes (want $LOADER_LEN)" >&2
        exit 1
    fi
    printf 'const unsigned char bll_loader_%s[BLL_LOADER_LEN] = {\n' "$name"
    od -An -v -tu1 "$tmp/loader.bin" | awk '
        { for (i = 1; i <= NF; i++) printf "    %d,", $i;
          printf "\n" }'
    printf '};\n\n'
}

{
    cat <<EOF
/*
** SPDX-License-Identifier: MPL-2.0
**
** This Source Code Form is subject to the terms of the Mozilla Public License,
** v. 2.0. If a copy of the MPL was not distributed with this file, You can
** obtain one at https://mozilla.org/MPL/2.0/.
**
** Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
*/

/*
** blloader_gen.c -- GENERATED, DO NOT EDIT.
**
** The Lynx secondary-loader region (offsets 0x00..0xCA, 203 bytes) for each
** supported cart block size, extracted from the SDK's own bootloader
** (runtime/lynx/bootldr.s, (c) Karri Kaksonen 2011) by linking a minimal
** program with cfg/lynx.cfg and stripping the 64-byte LYNX header. The three
** variants differ only in the block-size immediate at offset 0xC4
** (0xFE/0xFC/0xF8 == 0x100 - (blocksize >> 8)).
**
** Regenerate with tools/lnx/gen-loader.sh (top-level: make lnx-loader-gen).
** See design/LYNX_LNX_BLL_ROM_DESIGN.md.
*/

#include "blloader_gen.h"

EOF
    emit_array 0x0200 512
    emit_array 0x0400 1024
    emit_array 0x0800 2048
} > "$OUT"

echo "gen-loader.sh: wrote $OUT"
