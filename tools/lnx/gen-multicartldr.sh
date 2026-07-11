#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
#
# Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
#
# gen-multicartldr.sh -- regenerate libraries/core/multicartldr_gen.s from the
# relocatable multicart runtime loader (runtime/lynx/multicartldr.s). It links
# that loader on its own with cfg/lynx-multicartldr.cfg, which locates CODE at
# $0040 and emits a headerless raw image, then captures every byte as the
# _multicart_loader blob that multicart_run copies to $0040 at launch time.
#
# The output is a COMMITTED generated artifact; run this after a full build to
# refresh it (top-level: make multicart-loader-gen). See
# design/LYNX_MULTICART_DESIGN.md.

set -eu

# Locate the repo root relative to this script (tools/lnx/gen-multicartldr.sh).
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)

CL65="${CL65:-$root/bin/cl65}"
CFG="$root/cfg/lynx-multicartldr.cfg"
SRC="$root/runtime/lynx/multicartldr.s"
OUT="$root/libraries/core/multicartldr_gen.s"

export CC65_HOME="${CC65_HOME:-$root}"

if [ ! -x "$CL65" ]; then
    echo "gen-multicartldr.sh: $CL65 not found; build the toolchain first" >&2
    exit 1
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# Link the loader alone at $0040; the config emits the raw CODE image with no
# header, so the whole output file is the blob.
"$CL65" -t lynx -C "$CFG" -o "$tmp/loader.bin" "$SRC"
len=$(wc -c < "$tmp/loader.bin" | tr -d ' ')

{
    cat <<EOF
;
; SPDX-License-Identifier: MPL-2.0
;
; This Source Code Form is subject to the terms of the Mozilla Public License,
; v. 2.0. If a copy of the MPL was not distributed with this file, You can
; obtain one at https://mozilla.org/MPL/2.0/.
;
; Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
;
; multicartldr_gen.s -- GENERATED, DO NOT EDIT.
;
; The relocatable multicart runtime loader (runtime/lynx/multicartldr.s) linked
; at \$0040 by cfg/lynx-multicartldr.cfg and captured byte for byte. multicart_run
; (libraries/core/multicart.s) copies these ${len} bytes to \$0040 and jumps there
; to load a bundled game ROM off the cartridge.
;
; Regenerate with tools/lnx/gen-multicartldr.sh (top-level: make
; multicart-loader-gen). See design/LYNX_MULTICART_DESIGN.md.

        .export _multicart_loader
        .export multicart_loader_size : absolute = ${len}

        .segment "RODATA"

_multicart_loader:
EOF
    od -An -v -tu1 "$tmp/loader.bin" | awk '
        { line = "        .byte ";
          for (i = 1; i <= NF; i++) {
              line = line $i;
              if (i < NF) line = line ",";
          }
          print line }'
} > "$OUT"

echo "gen-multicartldr.sh: wrote $OUT ($len bytes)"
