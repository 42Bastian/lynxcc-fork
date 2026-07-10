<!--
SPDX-License-Identifier: CC-BY-4.0
Lynx Game Development SDK documentation, (c) 2026 the lynxcc authors,
licensed under Creative Commons Attribution 4.0 International.
See doc/licenses.html.
-->

# Vendored External Tools Design (`tools/extern/`)

Status: **IMPLEMENTED 2026-07-10**. Source of truth for how third-party tools
are vendored into the SDK and built with the rest of `tools/`. First tool:
`sprpck`. Supersedes the root-level `extern/` placeholder proposed in
`LYNX_SDK_LAYOUT_DESIGN.md` §3/§11 — vendored tools live under `tools/extern/`,
not a top-level `extern/`.

---

## 1. Goal and constraints

Bring genuinely third-party tools into the tree while keeping three things true
at once:

1. **A live link to the upstream repository** is preserved, so the tool can be
   re-synced and its provenance is auditable.
2. **The upstream licence is respected** — the tool is not relicensed, and its
   copyright/licence files are retained.
3. **The tool builds with the rest of lynxcc** — one `make`, output into the
   shared root `bin/`, no manual per-tool build step.

The hard rule that shapes the whole design: **external code is never modified.**
That rules out editing upstream Makefiles, adding SPDX headers, reformatting, or
dropping metadata files into the vendored tree.

## 2. Location and linkage

Each tool is a **git subtree** at `tools/extern/<tool>/`, added with:

```
git subtree add  --prefix tools/extern/<tool> <url> <branch> --squash
git subtree pull --prefix tools/extern/<tool> <url> <branch> --squash   # to update
```

Subtree (not submodule) is chosen deliberately: the working tree stays
single-clone and always buildable, upstream files are physically present for the
build to consume, and the squashed merge commit records the exact upstream SHA
for provenance. A submodule would fight the "self-contained, always-buildable
tree" invariant the SDK relies on and force `--recursive` clones.

The vendored prefix (`tools/extern/<tool>/`) is treated as **read-only**. All
project-owned artefacts live *outside* the prefix so a `subtree pull` never
collides with them:

- `tools/extern/README.md` — policy + provenance registry (pinned commits).
- `tools/Makefile` — the `EXTERN_PROGS` list and `EXTERN_template` build glue.
- `doc/licenses.html` — the upstream licence registry entry.

> Note: the sandbox proxy blocks Codeberg, so `subtree add`/`pull` for
> `sprpck` are run on a developer machine, not in-sandbox.

## 3. Build integration

The tool cannot carry SDK build files, so `tools/Makefile` compiles its sources
directly. Vendored tools are declared next to the project's own `PROGS`:

```make
EXTERN_PROGS    = sprpck
sprpck_SRC      = extern/sprpck/src   # the tool's own (unmodifiable) source dir
sprpck_CPPFLAGS = -DUNIX              # upstream-required defines
ALL_PROGS       = $(PROGS) $(EXTERN_PROGS)
```

`EXTERN_template` mirrors the existing `PROG_template` but (a) globs
`$($1_SRC)/*.c` instead of `$1/*.c`, (b) compiles with `EXTERN_CFLAGS`
(`-O3 -w` — optimisation and dependency generation kept, strict `-Wall -Wextra`
dropped because we may not fix warnings in code we do not own) plus the tool's
own `$($1_CPPFLAGS)`, and (c) still emits objects to `../wrk/$1` and the binary
to the shared `../bin/$1`. Because the output paths match, the existing
`all`/`clean`/`mostlyclean`/`install`/`zip`/`avail` targets treat vendored tools
identically once they iterate `$(ALL_PROGS)` instead of `$(PROGS)`.

Per-tool source globs matter because upstream layouts differ: `sprpck` keeps its
sources in `src/` (`io.c`, `sprpck.c`), so a flat `$1/*.c` glob would miss them;
`lynxdir` keeps its `.cpp` sources at its repo root, so `lynxdir_SRC =
extern/lynxdir` and the glob picks them up there.

### 3.1. C++ tools (`EXTERN_CXX_PROGS`)

`EXTERN_template` compiles and links C sources with `$(CC)`. Tools written in
C++ (first case: `lynxdir`, `lynxdir.cpp` + `lynxrom.cpp`) need the C++ driver
for both compilation and linking, so they use a separate, parallel path rather
than overloading the C one:

```make
EXTERN_CXX_PROGS = lynxdir
lynxdir_SRC      = extern/lynxdir     # .cpp sources live at the tool's repo root
ALL_PROGS        = $(PROGS) $(EXTERN_PROGS) $(EXTERN_CXX_PROGS)
```

`EXTERN_CXX_template` is identical in shape to `EXTERN_template` but (a) globs
`$($1_SRC)/*.cpp`, (b) compiles with `EXTERN_CXXFLAGS` (same relaxed `-O3 -w`
policy as `EXTERN_CFLAGS`) plus the tool's own `$($1_CPPFLAGS)`, and (c) both
compiles and **links** with `$(CXX)` so the C++ runtime is pulled in. Objects
still land in `../wrk/$1` and the binary in the shared `../bin/$1`, so the
`all`/`clean`/`install`/`zip`/`avail` targets — which iterate `$(ALL_PROGS)` —
treat C and C++ vendored tools identically. A separate template (rather than a
per-tool language switch inside `EXTERN_template`) keeps each path simple and the
C tools' recipe byte-unchanged.

## 4. Licence handling

Each tool keeps its own licence as a distinct component; it is **not** absorbed
into the SDK's MPL-2.0. The upstream `LICENSE` file stays in the vendored tree,
and an entry is added to `doc/licenses.html` §4 (bundled third-party components)
recording the licence, copyright, and that the code is unmodified.

Apache-2.0 (sprpck's licence) coexists cleanly with the MPL-2.0 fork under the
SDK's existing per-component model: it is one more third-party bucket, kept
segregated so the "fork-authored files are MPL, everything else keeps its own
notice" invariant (`LYNX_LICENSE_POLICY_DESIGN.md`) holds. Apache §4(b)'s
"state that you changed the files" obligation is moot precisely because the
vendored code is never changed; the project-owned build glue that lives outside
the tree is the only thing the SDK authored.

**Tools with no declared licence.** `lynxdir` ships **no licence file** — its
upstream repository declares only `(c) Björn Spruck 2010-2017`, which under
default copyright law reserves all rights. The vendoring policy still applies
(byte-identical, not relicensed), but the `doc/licenses.html` §4.4 entry and the
`tools/extern/README.md` registry both **flag this explicitly**: the tool is used
as-is pending an explicit grant from the author, and anyone redistributing the
SDK must resolve it upstream or drop `tools/extern/lynxdir/`. Vendoring an
unlicensed tool is a deliberate, documented exception — not the norm — and the
preferred resolution is an upstream licence.

## 5. Current tools

| Tool | Upstream | Pinned | Licence | Notes |
|------|----------|--------|---------|-------|
| `sprpck` | codeberg.org/42Bastian/sprpck (`master`) | `b4cdc2202a` | Apache-2.0 | Lynx sprite/bitmap packer. Complementary to `sp65` (adds action points, PI1/BMP input, batch mode, LYXASS palette, `-p0` cc65-object output). Documented on `doc/sprpck.html`; exercised by the `examples/suzy/sprpcktest` sample (BMP + ASCII SPS inputs, `.spr` linked via ca65 `.incbin`). |
| `lynxdir` | github.com/bspruck/lynxdir (`master`) | `3e46f9610b` | **None declared** (all rights reserved, © 2010–2017 Björn Spruck) | Lynx ROM builder (C++). `.mak`-driven cart assembly with EPYX/BLL/NewMini loaders. Overlaps `lnx`/`lnx bll` but covers loader layouts and multi-file ROM assembly it does not; not wired into `cl65` or examples. Documented on `doc/lynxdir.html`; no example sample. Licence caveat flagged in §4.4 and the README. |

## 6. Adding another tool (checklist)

1. `git subtree add --prefix tools/extern/<tool> <url> <branch> --squash`.
2. In `tools/Makefile`: append `<tool>` to `EXTERN_PROGS` (C) or
   `EXTERN_CXX_PROGS` (C++, §3.1); define `<tool>_SRC` and any `<tool>_CPPFLAGS`.
3. Add the tool to the `tools/extern/README.md` registry (pinned commit, licence).
4. Add its upstream licence entry to `doc/licenses.html` §4 and regenerate the
   doc search index (`make -C doc doc-search-index`). If the tool declares **no**
   licence, flag that explicitly in both the registry and §4 rather than leaving
   it blank.
5. Full rebuild; confirm `bin/<tool>` builds and the rest of the tree is
   unchanged.
