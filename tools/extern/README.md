<!--
SPDX-License-Identifier: CC-BY-4.0
Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
-->

# Vendored third-party tools (`tools/extern/`)

This directory holds external tools that ship with **lynxcc** but are **not**
authored by this project. Each subdirectory is a pristine copy of an upstream
repository, brought in as a **git subtree** and built alongside the rest of
`tools/`. The full rationale and build design is
`design/LYNX_EXTERN_TOOLS_DESIGN.md`; this file is the quick reference and the
provenance registry.

## Rules

1. **Never modify vendored code.** Everything under `extern/<tool>/` must stay
   byte-identical to upstream. No SPDX tags, no reformatting, no local patches,
   and no metadata files added *inside* the vendored tree — a `git subtree pull`
   would clobber them, and edits would break the clean upstream link.
2. **Project-owned glue lives outside the vendored prefix.** This `README.md`,
   the build wiring in `tools/Makefile` (`EXTERN_PROGS` + the `EXTERN_template`),
   and the licence registry in `doc/licenses.html` are the project's files and
   are edited freely. They reference the vendored sources; they never live among
   them.
3. **Build, don't fork.** Because upstream code can't be edited, the tool is
   compiled from its own source layout via a per-tool source glob and any
   upstream-required defines, with strict warnings disabled (`EXTERN_CFLAGS`) —
   we do not lint code we do not own. The binary installs into the shared root
   `bin/`, exactly like the project's own tools.
4. **Respect the upstream licence.** Each tool keeps its own licence, recorded
   below and in `doc/licenses.html`. Vendored tools are **not** relicensed under
   the SDK's MPL-2.0.

## Updating a vendored tool

Run from the repository root (needs network to the upstream forge — note the
sandbox proxy blocks Codeberg, so pulls run on a developer machine):

```
git subtree pull --prefix tools/extern/<tool> <url> <branch> --squash
```

After a pull, update the pinned commit in the table below and re-run a full
rebuild.

## Registry

| Tool | Upstream | Branch | Pinned commit | Licence | Build |
|------|----------|--------|---------------|---------|-------|
| `sprpck` | https://codeberg.org/42Bastian/sprpck | `master` | `b4cdc2202a` | Apache-2.0 | `EXTERN_PROGS`, sources `extern/sprpck/src/*.c`, `-DUNIX`; → `bin/sprpck` |

### sprpck — Lynx Sprite Packer

Converts images (PCX, BMP, PI1, raw, SPS) into Atari Lynx sprite data, with
packing/literal optimisation, action points, tiling, palette export (C / ASM /
LYXASS) and cc65-object output (`-p0`). Copyright © 1997–2021 42Bastian Schick
and Matthias Domin, with contributions from Karri Kaksonen and LordKraken;
licensed under Apache-2.0 (see `sprpck/LICENSE`).

It overlaps in purpose with the SDK's own `sp65` sprite converter but is
complementary: `sprpck` adds action points, PI1/BMP input, batch mode and LYXASS
palette output that `sp65`'s `lynx-sprite` mode does not cover.
