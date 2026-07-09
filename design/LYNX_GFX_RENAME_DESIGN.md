<!--
SPDX-License-Identifier: CC-BY-4.0
Lynx Game Development SDK documentation, (c) 2026 the lynxcc authors,
licensed under Creative Commons Attribution 4.0 International.
See doc/licenses.html.
-->

# Design: Renaming the `tgi_` Graphics API to the `gfx_` "Lynx graphics" API

Status: **IMPLEMENTED** (2026-06-26).
Source of truth for renaming the graphics module's public surface from the
`tgi_` / `TGI_` prefix to `gfx_` / `GFX_`, while keeping the old `tgi_` names
as deprecated, removable backward-compatibility shims.

Companion to `LYNX_GFX_DESIGN.md` (the static direct-call graphics library this
rename re-skins) and its font follow-ups `LYNX_GFX_FONT5X5_DESIGN.md` and
`LYNX_GFX_FONTVAR_DESIGN.md`. Those documents describe the *mechanism*; this one
only changes *names and terminology*. No runtime behaviour, codegen, memory map,
or call contract changes.

> **Implementation notes (2026-06-26).** Two refinements were made versus the
> first draft of this spec:
>
> 1. **`TGI_COLOR_*` palette aliases.** The default-palette aliases in
>    `include/lynx/lynx.h` (`TGI_COLOR_BLACK` … `TGI_COLOR_WHITE`) were not
>    enumerated in §2.1 but carry the public `TGI_` prefix, so they were renamed
>    to `GFX_COLOR_*` with deprecated `TGI_COLOR_*` aliases kept in `lynx.h`
>    under the same `LYNX_NO_TGI_COMPAT` switch. They are listed in the §5.1
>    constants table.
> 2. **Umbrella header.** `include/lynx/lynx.h` did not previously include the
>    graphics header at all (examples included `<lynx/tgi.h>` explicitly), so
>    "repoint lynx.h to gfx.h" (§4.2) was implemented by adding
>    `#include <lynx/gfx.h>` to `lynx.h`, giving `<lynx.h>` users the canonical
>    `gfx_` names by default.
>
> Out of scope for this rename: the cc65 o65 loadable-driver file format that
> the toolchain recognised at the time (`.tgi` extension in
> `compiler/common/filetype.c`, `doc/cl65.html`, and the `tgi_install` worked
> example in `doc/co65.html`) — that was the generic o65 driver subsystem, not
> the Lynx graphics API. (That o65/co65 subsystem has since been removed entirely
> as dead non-Lynx code, so those references no longer exist.)

## 1. Why rename

The graphics module is the last part of **lynxcc** that still carries its own
historical prefix. Every other subsystem uses a plain, descriptive name
(`joy_*`, `ser_*`, `suzy*`/Suzy math, the C library), but graphics is still
`tgi_*` — short for "Tiny Graphics Interface", the cc65 loadable-driver
framework that `LYNX_TGI_DESIGN.md` already tore out and replaced with a
direct-call static library. The name now describes a thing that no longer
exists: there is no interface to a loadable driver, no driver kernel, no
generality across display modes. It is simply *the* graphics library for the one
and only Lynx display mode.

Two consequences follow, and this design addresses both:

1. **Symbols.** The public prefix becomes `gfx_` (functions/macros) and `GFX_`
   (constants). `gfx` reads as plain "graphics"; because the whole tree is
   Lynx-only there is no ambiguity about *whose* graphics, so a longer
   `lynxgfx_`/`lynx_gfx_` prefix would only add typing. Old `tgi_` names survive
   as deprecated aliases (§4) so existing programs keep compiling.

2. **Terminology.** Everywhere the documentation currently says "TGI" (prose,
   headings, diagrams, page titles, the function reference) it should instead say
   **Lynx graphics**. "TGI" stops appearing as a concept; it survives only as a
   deprecated symbol prefix that the compatibility section documents.

This is a cosmetic/ergonomic change with a compatibility guarantee. It is
deliberately decoupled from any behavioural work so that a reviewer can confirm
the implementation is a pure rename.

## 2. Scope: what the rename touches

The current `tgi_`/`TGI_` footprint (from a tree-wide grep) is:

| Area | Files (representative) | Notes |
|---|---|---|
| Public header | `include/lynx/tgi.h` | The whole API surface + macros + constants |
| Pulled in via | `include/lynx/lynx.h`, `include/stdio.h` | Umbrella includes / cross-refs |
| Internal asminc | `asminc/tgi-kernel.inc` | Private constants (geometry, page/collision addrs) |
| Library source | `libraries/graphics/tgi-*.s` (15 files) | The implementation + fonts |
| Other libs | `libraries/core`, `libraries/math` | Stray references (e.g. comments, imports) |
| Examples | `examples/**` (suzy, games, memory, mikey, network) | ~16 `.c` files call the API |
| Template | `templates/basic/src/main.c` | Starter project |
| Docs | `doc/*.html` (21 pages incl. `lynx_tgi_fonts.html`, `funcref.html`) | Prose + the function reference |
| Design docs | `design/LYNX_TGI*_DESIGN.md` (3 files) | Source-of-truth prose |

Per `CLAUDE.md`, the code change and every doc that documents these symbols must
move together in the same pass.

### 2.1 Public symbols (need `tgi_` shims)

These are the documented entry points in `tgi.h`. Each gets a `gfx_` canonical
name and a `tgi_` deprecated alias.

Functions:
`tgi_init`, `tgi_clear`, `tgi_clearrows`, `tgi_sprite`, `tgi_flip`,
`tgi_setviewpage`, `tgi_setdrawpage`, `tgi_busy`, `tgi_updatedisplay`,
`tgi_setframerate`, `tgi_setcollisiondetection`, `tgi_setbpp`, `tgi_setcolor`,
`tgi_getcolor`, `tgi_setbgcolor`, `tgi_setpalette`, `tgi_getpalette`,
`tgi_getdefpalette`, `tgi_gotoxy`, `tgi_outtext`, `tgi_outtextxy`,
`tgi_settextscale`, `tgi_settextstyle`, `tgi_setfont`, `tgi_settextdir`,
`tgi_gettextwidth`, `tgi_gettextheight`.

Zero-cost query macros:
`tgi_getxres`, `tgi_getmaxx`, `tgi_getyres`, `tgi_getmaxy`,
`tgi_getcolorcount`, `tgi_getmaxcolor`, `tgi_getpagecount`.

Constants:
`TGI_XRES`, `TGI_YRES`, `TGI_COLORCOUNT`, `TGI_PAGECOUNT`,
`TGI_FONT_BITMAP`, `TGI_FONT_COMPACT`, `TGI_FONT_VARIABLE`,
`TGI_TEXT_HORIZONTAL`, `TGI_TEXT_VERTICAL`.

### 2.2 Internal symbols (renamed in place, NO shims)

These never appear in the public header; they are library-private exports/imports
shared between `.s` modules. They are renamed `tgi_*` → `gfx_*` directly with no
compatibility alias, because nothing outside the library may reference them:

`tgi_advtab`, `tgi_bgindex`, `tgi_buildptr`, `tgi_cls`, `tgi_defpalette`,
`tgi_draw`, `tgi_drawindex`, `tgi_drawpage`, `tgi_font`, `tgi_fontadv`,
`tgi_fontheight`, `tgi_fontvar`, `tgi_ioctl`, `tgi_pitch`, `tgi_vbl`.

Internal asminc constants (in `tgi-kernel.inc` → `gfx.inc`): `TGI_XRES`,
`TGI_YRES`, `TGI_COLORCOUNT`, `TGI_PAGECOUNT`, `TGI_PAGE0_ADDR`,
`TGI_PAGE1_ADDR`, `TGI_COLLBUF_ADDR`, `TGI_FLIPOFFS_4BPP`, `TGI_FLIPOFFS_2BPP`
become `GFX_*`.

## 3. File renames

Source and header files are renamed (git `mv`, preserving history) so the
on-disk names match the new prefix:

| Old | New |
|---|---|
| `include/lynx/tgi.h` | `include/lynx/gfx.h` |
| `asminc/tgi-kernel.inc` | `asminc/gfx.inc` (the `-kernel` suffix is dropped — there is no driver/kernel left) |
| `libraries/graphics/tgi-clear.s` | `libraries/graphics/gfx-clear.s` |
| `libraries/graphics/tgi-collision.s` | `libraries/graphics/gfx-collision.s` |
| `libraries/graphics/tgi-color.s` | `libraries/graphics/gfx-color.s` |
| `libraries/graphics/tgi-core.s` | `libraries/graphics/gfx-core.s` |
| `libraries/graphics/tgi-font.s` | `libraries/graphics/gfx-font.s` |
| `libraries/graphics/tgi-font5x5.s` | `libraries/graphics/gfx-font5x5.s` |
| `libraries/graphics/tgi-fontvar.s` | `libraries/graphics/gfx-fontvar.s` |
| `libraries/graphics/tgi-init.s` | `libraries/graphics/gfx-init.s` |
| `libraries/graphics/tgi-page.s` | `libraries/graphics/gfx-page.s` |
| `libraries/graphics/tgi-palette.s` | `libraries/graphics/gfx-palette.s` |
| `libraries/graphics/tgi-rate.s` | `libraries/graphics/gfx-rate.s` |
| `libraries/graphics/tgi-setfont.s` | `libraries/graphics/gfx-setfont.s` |
| `libraries/graphics/tgi-text.s` | `libraries/graphics/gfx-text.s` |
| `libraries/graphics/tgi-text5x5.s` | `libraries/graphics/gfx-text5x5.s` |
| `libraries/graphics/tgi-textvar.s` | `libraries/graphics/gfx-textvar.s` |
| `doc/lynx_tgi_fonts.html` | `doc/lynx_gfx_fonts.html` |
| `design/LYNX_TGI_DESIGN.md` | `design/LYNX_GFX_DESIGN.md` |
| `design/LYNX_TGI_FONT5X5_DESIGN.md` | `design/LYNX_GFX_FONT5X5_DESIGN.md` |
| `design/LYNX_TGI_FONTVAR_DESIGN.md` | `design/LYNX_GFX_FONTVAR_DESIGN.md` |

`libraries/graphics/` keeps its name (already neutral). The built library files
(`lynx-graphics.lib`, the SDK manifest in `libraries.mk`) already use the
"graphics" name, not "tgi", so they are unaffected — see
`LYNX_SDK_LAYOUT_DESIGN.md` §6 and `LYNX_CL65_AUTOLIBS_DESIGN.md`.

A **new** `include/lynx/tgi.h` is then created as a thin deprecated compatibility
shim (§4) — same path as the old header, new contents.

## 4. Backward-compatibility shims

Decision: shims are **deprecated and removable** (not permanent). Old `tgi_*`
code keeps compiling today, but the names are documented as deprecated and a
clean removal path exists (§6).

### 4.1 Mechanism: header-level aliases, zero cost

The canonical symbols are `gfx_*`. The compatibility layer is **pure
preprocessor aliasing** in a separate header, so it adds no code, no data, and
does not defeat ld65 smart linking (a program that only uses the alias still
links exactly the one `gfx_*` object it calls):

`include/lynx/gfx.h` — the real header. Header guard `_GFX_H`. All declarations,
macros and constants use `gfx_`/`GFX_`. This is the file the rest of the SDK and
all new code include.

`include/lynx/tgi.h` — the deprecated shim. It includes `gfx.h` and defines one
alias per public symbol:

```c
#ifndef _TGI_H
#define _TGI_H

/* DEPRECATED. The Lynx graphics API moved from the tgi_/TGI_ prefix to
** gfx_/GFX_. Include <lynx/gfx.h> and use the gfx_ names. These aliases exist
** only for backward compatibility and will be removed in a future release.
** Define LYNX_NO_TGI_COMPAT before including to opt out early and surface any
** lingering tgi_ uses as errors.
*/
#include <lynx/gfx.h>

#ifndef LYNX_NO_TGI_COMPAT

/* Functions: object-like aliases (they name real link symbols) */
#define tgi_init                gfx_init
#define tgi_clear               gfx_clear
#define tgi_clearrows           gfx_clearrows
#define tgi_sprite              gfx_sprite
/* ... one #define per function symbol in §2.1 ... */

/* Query macros: the alias must itself be FUNCTION-LIKE, because cc65's
** preprocessor does not re-expand a bare object-like alias into the underlying
** gfx_getxxx() function-like macro. */
#define tgi_getxres()           gfx_getxres()
/* ... one function-like #define per query macro in §2.1 ... */

/* Constants */
#define TGI_XRES                GFX_XRES
#define TGI_FONT_BITMAP         GFX_FONT_BITMAP
/* ... */

#endif /* LYNX_NO_TGI_COMPAT */
#endif /* _TGI_H */
```

Because the aliases are object-like macros that expand to the new identifiers,
calls, address-of, and constant uses all work unchanged. There is no second set
of link symbols: `_tgi_init` no longer exists as an exported symbol — the macro
rewrites the call to `_gfx_init` at preprocess time.

### 4.2 The umbrella header

`include/lynx/lynx.h` currently pulls in `tgi.h`. It is changed to include
`gfx.h` (the canonical header) so that programs including `<lynx.h>` get the new
names by default and are *not* silently put on the deprecated path. Code that
wants the old names must include `<lynx/tgi.h>` explicitly. `include/stdio.h`'s
cross-reference comment is updated to mention `gfx_*` text functions.

### 4.3 Assembly callers

No public asm alias module is provided: the public API is C, and the documented
contract is the C header. The internal `.s` modules are renamed wholesale (§2.2),
so intra-library asm references move together. If a future need arises for
link-level `_tgi_*` aliases (e.g. a third-party pre-built `.o`), that can be a
small `gfx-compat.s` of `_tgi_x := _gfx_x` exports — noted here as a deliberate
non-goal for now, not an oversight.

### 4.4 Why not `#warning` on use

cc65's preprocessor has no portable per-macro deprecation attribute, so a
`tgi_*` use cannot self-warn the way a C++ `[[deprecated]]` would. Deprecation is
therefore communicated by (a) documentation — the shim header comment, the
function reference, and `migrating.html`; and (b) the `LYNX_NO_TGI_COMPAT`
opt-out switch, which lets a project prove it is clean before the shims are
deleted. This keeps the removal in §6 mechanical.

## 5. Documentation terminology

Throughout `doc/*.html` and the design docs, the concept formerly called "TGI"
is renamed to **Lynx graphics**:

- Prose and headings: "the TGI library" → "the Lynx graphics library"; "TGI
  fonts" → "Lynx graphics fonts"; etc. Per `CLAUDE.md`, the project name
  **lynxcc** stays bolded; "Lynx graphics" is ordinary prose (not a brand chrome
  element), so it is not specially styled.
- `funcref.html`: the graphics section's entries and the section title switch to
  `gfx_*` / "Lynx graphics". A short note records that `tgi_*` names are
  deprecated aliases for the `gfx_*` functions.
- `doc/index.html` and any nav/cards: the "TGI fonts" page link points to the
  renamed `lynx_gfx_fonts.html` with updated label.
- SVG diagrams that label call paths or boxes "TGI" are relabelled "Lynx
  graphics" / `gfx_*`, following `design/DOC_SVG_STYLE_DESIGN.md` (viewBox 720,
  theme CSS variables, font conventions, `<figure>`/`<figcaption>` wrapper).
- `migrating.html`: add a **dedicated section titled "TGI Deprecation"** (a new
  top-level `<h2>`, with its own TOC entry and `id`, following the existing
  numbered-section pattern). It states that the `tgi_*`/`TGI_*` prefix is
  deprecated in favour of `gfx_*`/`GFX_*`, explains the `LYNX_NO_TGI_COMPAT`
  opt-out and the eventual removal, and carries the full old→new equivalence
  table from §5.1 so a porting user has one place to look up every replacement.
- `history.html`: a dated entry recording the rename and the compatibility
  guarantee.
- The three renamed design docs have their titles/prose updated from "TGI" to
  "Lynx graphics"; cross-references elsewhere (e.g. `LYNX_SDK_LAYOUT_DESIGN.md`,
  font docs, header comments that cite `design/LYNX_TGI_DESIGN.md`) are
  repointed to the new `design/LYNX_GFX_DESIGN.md` path.

### 5.1 TGI → Lynx graphics equivalence table

This is the canonical old→new mapping. It lives in the design doc as the source
of truth and is reproduced verbatim in the `migrating.html` "TGI Deprecation"
section. Every old name maps to exactly one new name; semantics, arguments and
return values are unchanged (it is a pure rename).

Functions:

| Deprecated (`tgi_*`) | Replacement (`gfx_*`) |
|---|---|
| `tgi_init` | `gfx_init` |
| `tgi_clear` | `gfx_clear` |
| `tgi_clearrows` | `gfx_clearrows` |
| `tgi_sprite` | `gfx_sprite` |
| `tgi_flip` | `gfx_flip` |
| `tgi_setviewpage` | `gfx_setviewpage` |
| `tgi_setdrawpage` | `gfx_setdrawpage` |
| `tgi_busy` | `gfx_busy` |
| `tgi_updatedisplay` | `gfx_updatedisplay` |
| `tgi_setframerate` | `gfx_setframerate` |
| `tgi_setcollisiondetection` | `gfx_setcollisiondetection` |
| `tgi_setbpp` | `gfx_setbpp` |
| `tgi_setcolor` | `gfx_setcolor` |
| `tgi_getcolor` | `gfx_getcolor` |
| `tgi_setbgcolor` | `gfx_setbgcolor` |
| `tgi_setpalette` | `gfx_setpalette` |
| `tgi_getpalette` | `gfx_getpalette` |
| `tgi_getdefpalette` | `gfx_getdefpalette` |
| `tgi_gotoxy` | `gfx_gotoxy` |
| `tgi_outtext` | `gfx_outtext` |
| `tgi_outtextxy` | `gfx_outtextxy` |
| `tgi_settextscale` | `gfx_settextscale` |
| `tgi_settextstyle` | `gfx_settextstyle` |
| `tgi_setfont` | `gfx_setfont` |
| `tgi_settextdir` | `gfx_settextdir` |
| `tgi_gettextwidth` | `gfx_gettextwidth` |
| `tgi_gettextheight` | `gfx_gettextheight` |

Query macros:

| Deprecated | Replacement |
|---|---|
| `tgi_getxres` | `gfx_getxres` |
| `tgi_getmaxx` | `gfx_getmaxx` |
| `tgi_getyres` | `gfx_getyres` |
| `tgi_getmaxy` | `gfx_getmaxy` |
| `tgi_getcolorcount` | `gfx_getcolorcount` |
| `tgi_getmaxcolor` | `gfx_getmaxcolor` |
| `tgi_getpagecount` | `gfx_getpagecount` |

Constants:

| Deprecated | Replacement |
|---|---|
| `TGI_XRES` | `GFX_XRES` |
| `TGI_YRES` | `GFX_YRES` |
| `TGI_COLORCOUNT` | `GFX_COLORCOUNT` |
| `TGI_PAGECOUNT` | `GFX_PAGECOUNT` |
| `TGI_FONT_BITMAP` | `GFX_FONT_BITMAP` |
| `TGI_FONT_COMPACT` | `GFX_FONT_COMPACT` |
| `TGI_FONT_VARIABLE` | `GFX_FONT_VARIABLE` |
| `TGI_TEXT_HORIZONTAL` | `GFX_TEXT_HORIZONTAL` |
| `TGI_TEXT_VERTICAL` | `GFX_TEXT_VERTICAL` |

Header: include `<lynx/gfx.h>` instead of `<lynx/tgi.h>` (or just `<lynx.h>`,
which now pulls in `gfx.h`).

Source-file license/banner comments in the renamed `.s`/`.h`/`.inc` files have
their box titles updated to the new filenames and their descriptive text from
"TGI" to "Lynx graphics", but SPDX tags and copyright attribution lines are left
exactly as-is (per the license policy in `LYNX_LICENSE_POLICY_DESIGN.md`; "TGI"
inside an attribution sentence such as "Originally the Tiny Graphics Interface by
Ullrich von Bassewitz" is historical attribution and is preserved verbatim).

## 6. Removal path (future)

Because shims are deprecated rather than permanent, removal is a later, separate,
mechanical step:

1. Announce in `history.html` / release notes; keep shims for at least one
   tagged release.
2. Verify no in-tree user remains: every example, template, and test builds with
   `LYNX_NO_TGI_COMPAT` defined (or with `tgi.h` absent). The internal library
   already uses `gfx_*` after this rename, so only examples/templates/docs need
   sweeping.
3. Delete `include/lynx/tgi.h`, drop the `LYNX_NO_TGI_COMPAT` guard, and remove
   the deprecation notes from the docs (leaving only the historical mention in
   `migrating.html`/`history.html`).

Until then the default build keeps `tgi.h` present and functional.

## 7. Implementation order (for the future code pass)

1. `git mv` the source/header/inc/doc/design files per §3.
2. In the renamed library `.s` files and `gfx.inc`, rename every `tgi_*`
   export/import and `TGI_*` constant to `gfx_*` / `GFX_*` (public and internal).
3. Rewrite `include/lynx/gfx.h` with the `gfx_`/`GFX_` API (guard `_GFX_H`).
4. Add the new deprecated `include/lynx/tgi.h` shim (§4.1).
5. Repoint `include/lynx/lynx.h` to `gfx.h`; fix the `stdio.h` cross-ref.
6. Sweep `examples/**`, `templates/basic/**`, and stray refs in
   `libraries/core` and `libraries/math` to the `gfx_*` names (so the tree
   exercises the canonical API, not the shims).
7. Update all docs and design-doc prose/diagrams per §5, including the
   `design/LYNX_TGI_DESIGN.md` → `LYNX_GFX_DESIGN.md` cross-reference fixes.
8. Grep the whole tree for residual `tgi`/`TGI` (case-insensitive); every
   remaining hit must be either the intentional shim in `tgi.h`, a historical
   attribution line, or a deprecation note — anything else is a miss.
9. Full sandbox rebuild (toolchain + libraries + examples), unit tests, and the
   GearLynx integration goldens; the rename must be byte-neutral — every `.lnx`
   identical to its pre-rename golden, since no code changed.

## 8. Verification that this is behaviour-neutral

The rename must not alter a single emitted byte. The acceptance check is the
existing test harness from `LYNX_SDK_LAYOUT_DESIGN.md` phase 7: after the change,
`tests/run.sh` passes (unit model 927343 checks / 0 mismatches) and the curated
GearLynx integration set produces `.lnx` images byte-identical to the current
goldens. Any diff means a symbol was mis-mapped or a constant changed value, not
a legitimate behaviour change.
