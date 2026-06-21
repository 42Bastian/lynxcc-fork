# License Consolidation Design

Status: **IMPLEMENTED** (2026-06-21). The per-manual *License* / *Copyright*
sections have been moved out of the individual `doc/*.html` pages into a single
authoritative `doc/licenses.html`, without violating any of the licenses
involved. This document records the design and the completed edits.

## 1. Motivation

The same license text is currently duplicated at the bottom of multiple manuals
(`doc/lynx.html` §12 "License", `doc/cc65.html` "Copyright"), and a copy also
lives in the top-level `LICENSE` file. Duplication means the notices can drift
out of sync and every new manual is expected to append its own copy.

This matters more as the tree becomes the **Lynx Game Development SDK**: the
cc65-derived toolchain is now one component among several, and new SDK tools and
libraries will be added over time, each potentially carrying its own notice.
A single `doc/licenses.html` becomes the canonical **registry** for every
license and copyright notice in the SDK — manuals link to it, and new components
register their notices there rather than scattering copies through the docs.

## 2. Can this be done without violating the license?

Yes, subject to the conditions below. Every notice in the tree today is the cc65
**zlib-style** license (plus one older notice, §4); the cc65-derived toolchain is
the SDK component these cover. The operative restriction is clause 3: *"This
notice may not be removed or altered from any source distribution."*

What that clause binds is the **distribution**, not each individual document:

- **"may not be removed"** — the notice must remain *present somewhere* in the
  source distribution. Relocating it from many manuals into one shipped
  `doc/licenses.html` (alongside the existing `LICENSE` file) does not remove it
  from the distribution. As long as `doc/licenses.html` is part of every
  distribution, the requirement is met.
- **"may not be altered"** — the canonical text must be reproduced **verbatim**.
  No paraphrasing, summarising, or wording fixes in the consolidated copy
  (including pre-existing typos such as "the refers to this file" in the Dunning
  notice — preserved as-is).

This is not legal advice; it is a reading of the license text. The conditions in
§3 are what make the consolidation compliant.

## 3. Compliance conditions (must all hold)

1. **`doc/licenses.html` ships in every distribution** — it is part of the
   `doc/` tree that the release install (`make install`/`zip`, see the SDK
   layout design) includes. The
   notice is therefore never absent from the distribution.
2. **All notices reproduced verbatim** in `doc/licenses.html`:
   - the cc65 zlib-style license;
   - the **John R. Dunning** original-compiler copyright (RA65/LINK65/LIBR65 and
     parts of the preprocessor/parser) — this is the easy-to-miss one: it lives
     **only** in `doc/cc65.html` today and is *not* in the root `LICENSE` file,
     so dropping the `cc65.html` Copyright section without carrying it forward
     would remove it from the distribution and violate its own clause 3.
3. **Per-source-file notices are left untouched.** The header comments in
   `include/zlib.h` (Piotr Fusik; Gailly & Adler) and `include/lz4.h` (Mega Cat
   Studios) and the `libsrc/` sources are *source* notices; clause 3 keeps them
   in their files. Consolidation only touches the prose docs, not source
   headers. `doc/licenses.html` additionally *inventories* these for
   completeness, but the authoritative copies stay in the source files.
4. **No notice is the only copy.** After the edit, each notice exists in at least
   `doc/licenses.html` (and the root `LICENSE` for the main license, and the
   source headers for the bundled components).

## 4. What `doc/licenses.html` contains

1. **§2 cc65 license** — the package-wide zlib-style license, verbatim (wording
   follows the root `LICENSE` file, the distribution's authoritative copy).
2. **§3 Original compiler copyright** — the full John R. Dunning notice,
   verbatim from `doc/cc65.html`, in a `<pre class="verb">` block.
3. **§4 Bundled third-party components** — inventory of the decompression
   notices that otherwise live only in source headers: zlib/inflate (Piotr
   Fusik; based on zlib by Jean-loup Gailly and Mark Adler) and lz4 (Mega Cat
   Studios), both under the same zlib-style terms.

The page follows `doc/` house style (topbar/nav/`doc.css`/`doc.js`,
`<pre class="verb">` for verbatim blocks) and adds a **Licenses** nav entry.

## 5. Edits to existing files (docs-track-code)

Per `CLAUDE.md`, the same pass that adds `doc/licenses.html` updates every place
that points at the old per-manual sections:

| File | Change |
| --- | --- |
| `doc/licenses.html` | **NEW** — canonical consolidated notices (done). |
| `doc/cc65.html` | Done. §14 *Copyright* body (the Dunning `<pre>` block + zlib text) replaced with a one-line pointer to `licenses.html`; heading kept. |
| `doc/lynx.html` | Done. §12 *License* body replaced with a one-line pointer to `licenses.html`; heading and TOC link kept. |
| binutils manuals (`doc/ar65.html` §3, `doc/ca65.html` §18, `doc/cl65.html` §5, `doc/co65.html` §5, `doc/da65.html` §5, `doc/ld65.html` §7, `doc/sp65.html` §8) | Done. Each carried its own *Copyright* section with the full zlib license body. The verbatim license body (`'as-is'` paragraph through the `<ol>`) was replaced with a one-line pointer to `licenses.html`; the heading and the per-tool copyright-attribution line (e.g. "(C) Copyright 1998–2005 Ullrich von Bassewitz") are retained. These were not listed in the original draft of this table — they were found during implementation by grepping every `doc/*.html` for the license body, which is the docs-track-code requirement. |
| `doc/history.html` | Done. Both pointers repointed to `licenses.html`: (a) §4 "Licensing" and (b) the abstract line. The cc65 copyright statement (Copyright © 1998–2012 Ullrich von Bassewitz and others) is retained; only the *where to find the full notice* pointer changed. |
| `doc/index.html` | Done. **Licenses** card added linking `licenses.html`. |
| nav bar (all `doc/*.html`) | Done. `<a href="licenses.html">Licenses</a>` entry added to every manual's topbar nav. |

The manuals keep a brief heading + link rather than going silent, so a reader
landing mid-document still finds the path to the notices.

## 6. Non-goals / explicitly preserved

- The root `LICENSE` file is **kept** unchanged — it remains an authoritative
  top-level copy of the main license.
- Source-file header notices are **not** modified or removed.
- No license *text* is reworded; only its *location in the docs* changes.

## 7. Verification checklist

- [x] `doc/licenses.html` contains the zlib license and the Dunning notice
      verbatim (Dunning block diffed byte-for-byte against the pre-edit
      `doc/cc65.html`; zlib wording matches the root `LICENSE`).
- [x] Dunning notice no longer exists *only* in `cc65.html` — the `RA65,
      LINK65, LIBR65` notice now appears in `doc/licenses.html` and nowhere else
      under `doc/`.
- [x] `include/zlib.h` and `include/lz4.h` header notices still present and
      unaltered.
- [x] Every `doc/*.html` links to `licenses.html` (nav); `index.html` card
      present.
- [x] `history.html` repointed in **both** places (§4 Licensing section and the
      abstract line); cc65 copyright statement retained.
- [x] No `doc/*.html` other than `licenses.html` still contains a verbatim
      license body — grep for `provided 'as-is'` / `may not be removed or
      altered` returns only `licenses.html`. (Covers the seven binutils manuals
      above in addition to `cc65.html` and `lynx.html`.)
- [x] `doc/Makefile` build still succeeds (the `all` target is a no-op; the
      `install`/`zip` `$(wildcard *.html)` automatically picks up
      `licenses.html`); pages parse with balanced tags and use only theme CSS
      variables, so they render in light and dark themes.
- [x] The release install (`make install`/`zip`) carries `doc/licenses.html`:
      it copies the whole `doc/` tree, which includes `licenses.html`.

## 8. Future SDK components

As the SDK gains tools and libraries (see the SDK layout design), each new
component registers its license/copyright in `doc/licenses.html` as part of the
same change that adds it — the docs-track-code rule extended to licensing:

- A component under the project's own zlib-style terms needs no new section; the
  §1/§2 package-wide statement already covers it.
- A component with a **different** license, or a vendored third party under
  `extern/`, adds a verbatim subsection under §4 "Bundled third-party
  components" (and keeps its own source-header notice).

This keeps `doc/licenses.html` a complete, single-glance inventory of the SDK's
licensing as it grows, with no per-manual duplication to maintain.
