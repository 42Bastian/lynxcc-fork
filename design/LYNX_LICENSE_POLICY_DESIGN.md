<!--
SPDX-License-Identifier: CC-BY-4.0
Lynx Game Development SDK documentation, (c) 2026 the lynxcc authors,
licensed under Creative Commons Attribution 4.0 International.
See doc/licenses.html.
-->
# License Policy Design (per-component SDK licensing)

Status: **IMPLEMENTED** (2026-06-23). This is the source-of-truth note for
phase 9 of `design/LYNX_SDK_LAYOUT_DESIGN.md` (§13.1). It records the
per-component licensing pattern adopted for the **Lynx Game Development SDK**,
the exact per-file scope, the SPDX header texts, the `LICENSE*` files added, and
the `doc/licenses.html` registry edits. No code behaviour changes in this phase:
it touches license headers, `LICENSE*` files, and the license registry only.

This builds on `design/LYNX_LICENSE_CONSOLIDATION_DESIGN.md`, which made
`doc/licenses.html` the single authoritative registry for every notice in the
distribution. That note's §8 ("future SDK components") is the rule this phase
follows: a component with a **different** license registers its verbatim text in
`doc/licenses.html`. The cc65 zlib-style baseline and the bundled third-party
notices are unchanged; this phase *layers* three additional licenses on top.

## 1. Motivation

Until now the whole tree was distributed under the cc65 zlib-style license plus
the inventory of upstream/third-party notices in `doc/licenses.html`. As the
tree became an SDK (toolchain + runtime + libraries + tools + examples +
templates + docs), a single license stopped matching how the pieces are meant to
be used. Many game-development toolchains license their components differently:
the engine/toolchain under a file-scoped copyleft, the starter code permissively
so users can copy it into their own games, and the prose docs under a
documentation license. Phase 9 adopts that pattern.

## 2. Component → license mapping

| Component | License | Scope |
| --- | --- | --- |
| SDK / toolchain | **MPL-2.0** | **Only** the new source files this fork has *authored*. Inherited cc65 files keep their existing zlib-style / Dunning / third-party notices unchanged. |
| Example games | **MIT** | `examples/` — permissive starter code, no copyleft reach into user games. |
| Project templates | **MIT** | `templates/` — same rationale as examples. |
| Documentation | **CC-BY 4.0** | `doc/*.html` and the `design/*_DESIGN.md` sources that feed them. |

Everything not covered by these three stays exactly as before: the package-wide
cc65 zlib-style license (root `LICENSE`), the John R. Dunning notice, and the
bundled third-party decompression notices (`include/lynx/zlib.h`,
`include/lynx/lz4.h` and their `libraries/compress/` sources).

### 2.1 Why these three

- **MPL-2.0 is file-scoped by design.** Its copyleft attaches per *file* (it
  speaks of "Covered Software" = the files carrying the Exhibit A notice), so
  applying it to only the fork's own new sources — without disturbing the
  zlib-style notices on inherited cc65 files or the bundled third-party headers —
  is exactly the granularity MPL expects. The package remains a compliant mix:
  MPL files, zlib-style files, and third-party files side by side.
- **Examples/templates as MIT** avoids imposing any SDK license on the games
  users build from the starter code, which is the entire point of shipping it.
- **Docs as CC-BY 4.0** is the conventional license for prose/reference
  material; attribution is satisfied by the existing authorship/footer lines and
  the SPDX tag.

## 3. Per-file scope: how "fork-authored" is determined

MPL applies to **files the fork authored from scratch**, never to inherited cc65
files (which keep their own notices) and never to bundled third-party files. The
fork has an `upstream/master` remote and a `V2.19` baseline, so this is decided
mechanically, not by guesswork. A file is **fork-authored (MPL)** iff **both**:

1. **It is not an upstream file.** Its content does not originate in cc65: its
   current path is not in the `upstream/master` tree, and tracing its creation
   with `git log --follow --diff-filter=A` does not reach a path that exists in
   the `upstream/master` tree. This catches in-place modifications and renamed/
   moved inherited files (e.g. `libsrc/common/_hextab.c` →
   `libraries/libc/_hextab.c`) and keeps them out of the MPL set.
2. **It carries no pre-existing attribution.** The file contains no cc65/Dunning/
   third-party notice — no author/copyright line, no "This file is part of cc65"
   boilerplate, no `cc65.github.io` / `See "LICENSE" file` pointer, and no
   credit to a named third party's prior work (e.g. the Lynx graphics modules extracted
   from Karri Kaksonen's lynx-160-102-16 driver and the serial modules extracted
   from his ComLynx driver keep his copyright line and an extraction note under
   the cc65 license, rather than being relicensed).

Both gates are required because each alone has a failure mode: `git --follow`
rename detection occasionally mis-pairs a genuinely new fork file with a deleted
upstream file (false "inherited"), and the attribution scan alone would miss
notice-less inherited files. Their intersection is robust and auditable.

The practical consequences:

- **Modified-but-inherited files keep their cc65 notices** and get no MPL header.
  Most of the fork's compiler work is edits to existing cc65 files (e.g. the
  Suzy `!*`/`!/`/`!%` operators were added by editing the existing expression,
  instruction-table and codegen sources), so the compiler suite gets **no** new
  MPL headers — consistent with "existing files keep their notices unchanged."
- **Files derived from Karri Kaksonen's cc65 drivers keep his copyright.** The
  static Lynx graphics modules split out of his `lynx-160-102-16` driver
  (`gfx-core/text/clear/collision/color/font/init/page/palette/rate.s`) and the
  ComLynx serial modules split out of his `lynx-comlynx` driver
  (`ser-core/open/close/get/put/status.s`) carry his copyright line and an
  "extracted and adapted from ..." note. They stay under the cc65 package
  license (root `LICENSE`), not MPL — the conservative choice for derived work,
  even where the fork substantially rewrote a routine (e.g. the `ser_close`
  behaviour fix).
- **Genuinely new fork files get MPL.** The Suzy hardware-math routines, the
  static joystick read (`joy-read.s`, a clean reimplementation), the compact-font
  additions to the Lynx graphics text system (`gfx-setfont/font5x5/text5x5/fontvar.s`), the
  new public `lynx/suzymath.h` header, the `lnx` tool, the build orchestration
  (`libraries.mk`, `tools/Makefile`, `build-windows.ps1`), and the `tests/`
  harness.

### 3.1 The MPL-2.0 file set (35 files, authoritative)

```
asminc/ser.inc
build-windows.ps1
include/lynx/suzymath.h
libraries.mk
libraries/core/joy-read.s
libraries/graphics/gfx-font5x5.s
libraries/graphics/gfx-fontvar.s
libraries/graphics/gfx-setfont.s
libraries/graphics/gfx-text5x5.s
libraries/math/suzyasync.s
libraries/math/suzydiv.s
libraries/math/suzymod.s
libraries/math/suzymul.s
libraries/math/suzymuldiv.s
libraries/math/suzyudiv.s
libraries/math/suzyumod.s
tests/README.md
tests/golden/README.md
tests/golden/games__breakout.sha256        (data file: covered via tests/golden/README.md, no in-file header)
tests/golden/lynxdemo.sha256               (data file)
tests/golden/mikey__setbpp.sha256          (data file)
tests/golden/suzy__fonttest.sha256         (data file)
tests/golden/suzy__spritetest.sha256       (data file)
tests/integration/README.md
tests/integration/gearlynx_check.py
tests/run.sh
tests/unit/Makefile
tests/unit/README.md
tests/unit/suzymath.c
tools/Makefile
tools/lnx/jsoncfg.c
tools/lnx/jsoncfg.h
tools/lnx/lnxhdr.c
tools/lnx/lnxhdr.h
tools/lnx/main.c
```

The six `.sha256` golden files are single-line digests that cannot carry a
comment without breaking the test reader, so per MPL Exhibit A their notice lives
in `tests/golden/README.md` (the "LICENSE file in a relevant directory" path the
MPL explicitly allows) instead of an in-file header.

Every Lynx graphics and ComLynx serial module derived from Karri Kaksonen's cc65 drivers
is **deliberately excluded** from the MPL set: the static Lynx graphics modules
(`gfx-core/text/clear/collision/color/font/init/page/palette/rate.s`) and the
serial modules (`ser-core/open/close/get/put/status.s`) carry his copyright line
and an extraction note, and stay under the cc65 package license. Only the
compact-font additions (`gfx-setfont/font5x5/text5x5/fontvar.s`) and the
clean-rewrite `joy-read.s`, which are fork-authored from scratch, are MPL. This
exclusion is intentional — not an oversight — and recorded here so it does not
later read as one.

## 4. SPDX headers

Every covered source file (except the data-only `.sha256` goldens) gets an
`SPDX-License-Identifier` tag plus a short standard notice, in the file's native
comment syntax, inserted at the very top — after a `#!` shebang or an HTML
`<!DOCTYPE html>` line where present. The SPDX tags make the per-file scope
**machine-checkable**.

**MPL-2.0** (block/`/* ** */`, `;`, or `#` comment per file type):

```
SPDX-License-Identifier: MPL-2.0

This Source Code Form is subject to the terms of the Mozilla Public License,
v. 2.0. If a copy of the MPL was not distributed with this file, You can
obtain one at https://mozilla.org/MPL/2.0/.

Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
```

**MIT** (examples/templates):

```
SPDX-License-Identifier: MIT

Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
Provided under the MIT License; copy it into your own projects freely.
See the LICENSE file in this directory.
```

**CC-BY-4.0** (`doc/*.html` as an HTML comment, `design/*.md` as a leading HTML
comment):

```
SPDX-License-Identifier: CC-BY-4.0
Lynx Game Development SDK documentation, (c) 2026 the lynxcc authors,
licensed under Creative Commons Attribution 4.0 International.
See doc/licenses.html.
```

## 5. LICENSE files

| File | Purpose |
| --- | --- |
| `LICENSE` (root) | Authoritative copy of the cc65 zlib-style package license (the baseline that inherited cc65 and bundled third-party files point to), now prefaced by a short multi-license overview that names the MPL-2.0/MIT/CC-BY-4.0 layers and points to their files. The verbatim zlib-style body is preserved unchanged below that preamble. |
| `LICENSE-MPL-2.0.txt` (root) | **NEW.** Full verbatim MPL-2.0 text — the copy referenced by every MPL file's Exhibit A notice. |
| `examples/LICENSE` | **NEW.** MIT text covering `examples/`. |
| `templates/LICENSE` | **NEW.** MIT text covering `templates/`. |
| `doc/LICENSE` | **NEW.** Full verbatim CC-BY-4.0 legal code covering the documentation. |

Per-source SPDX headers and these `LICENSE*` files are the authoritative copies;
`doc/licenses.html` is the human-readable registry that inventories them all.

## 6. `doc/licenses.html` registry edits

The registry gains three new sections, each reproducing the operative license
verbatim (MIT in full; MPL-2.0 and CC-BY-4.0 via their canonical Exhibit/notice
text plus a pointer to the full `LICENSE-MPL-2.0.txt` / `doc/LICENSE` bodies that
ship in the distribution), and a summary paragraph explaining the per-component
split. The Contents list and the §1 summary are updated to match. The cc65
zlib-style license, the Dunning notice, and the bundled third-party notices are
unchanged.

A note clarifies that where a documentation page reproduces a third party's
license verbatim (notably `doc/licenses.html` itself), that quoted text remains
under its own terms — the page's CC-BY tag covers the page's own prose, not the
quoted licenses.

## 7. Compatibility and compliance

- **No inherited or third-party notice is removed or altered.** MPL §3.4 forbids
  removing notices from Covered Software; this phase only *adds* notices to
  fork-authored files and leaves every cc65/Dunning/third-party notice in place.
- **The mix is compliant.** MPL-2.0's file scope means MPL files can sit beside
  zlib-style and third-party files in one distribution.
- **`CC65_HOME` discovery is untouched** (§2 of the SDK layout design): no data
  directory is renamed or moved, no binary path logic changes.
- **No behaviour change.** Headers are comments; a full sandbox rebuild
  (toolchain + libraries + examples) and `make tests` pass unchanged, and the
  `.lnx` outputs are byte-identical to the pre-phase build.

## 8. Verification checklist

- [x] Each of the 35 MPL files (except `.sha256` goldens) carries an
      `SPDX-License-Identifier: MPL-2.0` header; the goldens are covered via
      `tests/golden/README.md`.
- [x] No file bearing a cc65 / Dunning / third-party notice received an MPL
      header (all Lynx graphics and ComLynx serial modules derived from Karri Kaksonen's
      drivers, and all moved cc65 sources, excluded).
- [x] `examples/` and `templates/` source files carry `SPDX-License-Identifier:
      MIT`; each directory has a `LICENSE`.
- [x] `doc/*.html` and `design/*.md` carry `SPDX-License-Identifier: CC-BY-4.0`;
      `doc/LICENSE` present.
- [x] `LICENSE-MPL-2.0.txt` and `doc/LICENSE` contain the full verbatim license
      bodies; root `LICENSE` keeps its verbatim zlib-style body, now prefaced
      by a multi-license overview.
- [x] `doc/licenses.html` registers MPL-2.0, MIT and CC-BY-4.0 with verbatim
      text and an updated Contents list; cc65/Dunning/third-party notices intact.
- [x] Full sandbox rebuild green; `make tests` pass; `.lnx` byte-identical.
- [x] `design/LYNX_SDK_LAYOUT_DESIGN.md` §13.1 and §13 phase 9 marked
      IMPLEMENTED; `design/LYNX_LICENSE_CONSOLIDATION_DESIGN.md` §8 points here;
      `README.md` license overview updated.
