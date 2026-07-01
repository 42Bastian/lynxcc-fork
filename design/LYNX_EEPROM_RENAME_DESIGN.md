<!--
SPDX-License-Identifier: CC-BY-4.0
Lynx Game Development SDK documentation, (c) 2026 the lynxcc authors,
licensed under Creative Commons Attribution 4.0 International.
See doc/licenses.html.
-->

# Design: Renaming the `lynx_ee*` EEPROM API to the `eeprom_<chip>_<op>` Scheme

Status: **IMPLEMENTED** (2026-07-02).

Source of truth for renaming the serial-EEPROM read/write family from the
`lynx_ee`-prefixed, operation-then-chip form (`lynx_eeread_93c46`) to a plain
`eeprom_`-prefixed, chip-then-operation form (`eeprom_93c46_read`), while keeping
the old `lynx_ee*` names as deprecated, removable backward-compatibility defines.

This follows the same pattern and compatibility guarantee as the `tgi_`→`gfx_`
rename in `LYNX_GFX_RENAME_DESIGN.md`. That document renamed the graphics API;
this one renames the EEPROM API. It changes *names only* — no runtime behaviour,
codegen, memory map, register access, or call contract changes. A reviewer should
be able to confirm the implementation is a pure rename.

## 1. Why rename

The EEPROM entry points are the last public functions in **lynxcc** that carry
the historical `lynx_ee` prefix, and their naming is awkward on two counts:

1. **Prefix.** `lynx_ee` fuses the platform prefix (`lynx_`) with an abbreviated
   subsystem tag (`ee`) into a single opaque token. Every other subsystem uses a
   clean, spelled-out prefix (`gfx_`, `joy_`, `ser_`, `snd_`, `sfx_`). The EEPROM
   API should read the same way: `eeprom_`. Because the whole tree is Lynx-only
   there is no ambiguity about *whose* EEPROM, so a redundant `lynx_` platform
   prefix only adds typing — exactly the reasoning applied to `gfx_` in
   `LYNX_GFX_RENAME_DESIGN.md` §1.

2. **Component order.** The current names put the operation before the chip
   model (`ee` + `read`/`write` + `_93c46`). Grouping by chip reads more
   naturally and sorts better: all `eeprom_93c46_*` calls cluster together, then
   all `eeprom_93c66_*`, then `eeprom_93c86_*`. The canonical order is therefore
   `eeprom_` + `<chip>` + `_` + `<op>`, e.g. `eeprom_93c46_read`.

Old `lynx_ee*` names survive as deprecated aliases (§4) so existing programs keep
compiling. This is a cosmetic/ergonomic change with a compatibility guarantee,
deliberately decoupled from any behavioural work.

## 2. Scope: what the rename touches

The current `lynx_ee*` footprint (from a tree-wide grep) is:

| Area | Files | Notes |
|---|---|---|
| Public header | `include/lynx/lynx.h` | The 6 declarations + the EEPROM section prose |
| Library source | `libraries/core/eeprom46.s`, `eeprom66.s`, `eeprom86.s` | The `.export`ed link symbols + banner/comment references |
| Cross-ref comment | `libraries/core/open.s` | Prose comment pointing at the family |
| Docs | `doc/funcref.html` | 6 reference entries + the index anchors |
| Docs | `doc/lynx.html` | EEPROM section prose |
| Docs | `doc/migrating.html` | §4 checklist item names the family; the new deprecation section (§5 below) |

Per `CLAUDE.md`, the code change and every doc that documents these symbols must
move together in the same pass.

### 2.1 Public symbols (need `lynx_ee*` shims)

The six documented entry points in `lynx.h`. Each gets an `eeprom_<chip>_<op>`
canonical name and a `lynx_ee*` deprecated alias:

| Deprecated (`lynx_ee*`) | Replacement (`eeprom_<chip>_<op>`) |
|---|---|
| `lynx_eeread_93c46`  | `eeprom_93c46_read`  |
| `lynx_eeread_93c66`  | `eeprom_93c66_read`  |
| `lynx_eeread_93c86`  | `eeprom_93c86_read`  |
| `lynx_eewrite_93c46` | `eeprom_93c46_write` |
| `lynx_eewrite_93c66` | `eeprom_93c66_write` |
| `lynx_eewrite_93c86` | `eeprom_93c86_write` |

Signatures, arguments and return values are unchanged:

```c
unsigned __fastcall__ eeprom_93c46_read  (unsigned char cell);
unsigned __fastcall__ eeprom_93c66_read  (unsigned addr);
unsigned __fastcall__ eeprom_93c86_read  (unsigned addr);
void     __fastcall__ eeprom_93c46_write (unsigned addr, unsigned val);
void     __fastcall__ eeprom_93c66_write (unsigned addr, unsigned val);
void     __fastcall__ eeprom_93c86_write (unsigned addr, unsigned val);
```

### 2.2 Internal symbols (renamed in place, NO shims)

The `.s` files also define file-local, non-exported helper labels
(`EE_Send9Bit`, `EE_Read16Bit`, `EE_Send16Bit`, `EE_wait`, the `EEloopN`
labels, and the `EE_C_*` command equates). These are private to each module and
never appear in the public header, so they are **not** part of the rename's
public surface. They may be left as-is; the only *required* edit inside the `.s`
files is the `.export` line and the C-prototype banner comment. (If a future
cleanup wants them consistently spelled, that is cosmetic and out of scope here.)

The link symbols that *do* change are the exported ones:
`_lynx_eeread_93c46` → `_eeprom_93c46_read`, and so on for all six. Because the
compatibility layer is a preprocessor `#define` (§4), no second set of link
symbols is created — `_lynx_eeread_93c46` simply ceases to exist as an exported
symbol; the macro rewrites the call to `_eeprom_93c46_read` at preprocess time.

## 3. File renames

The source files are named after the chip, not the operation prefix
(`eeprom46.s`, `eeprom66.s`, `eeprom86.s`), so they already fit the new scheme
and are **not** renamed. Only symbol/text edits are needed inside them. No
header file is renamed either — the declarations live in the shared
`include/lynx/lynx.h`, which stays put.

## 4. Backward-compatibility shims

Decision: shims are **deprecated and removable** (not permanent), matching
`LYNX_GFX_RENAME_DESIGN.md` §4.

### 4.1 Mechanism: header-level defines, zero cost

The canonical symbols are `eeprom_*`. The compatibility layer is pure
preprocessor aliasing in `include/lynx/lynx.h`, so it adds no code, no data, and
does not defeat ld65 smart linking (a program that only uses an alias still links
exactly the one `eeprom_*` object it calls). Placed just after the six canonical
declarations in the EEPROM section:

```c
/* DEPRECATED. The EEPROM API moved from the lynx_ee* prefix (operation-then-chip)
** to the eeprom_<chip>_<op> scheme. Use the eeprom_* names. These aliases exist
** only for backward compatibility and will be removed in a future release.
** Define LYNX_NO_EEPROM_COMPAT before including <lynx.h> to opt out early and
** surface any lingering lynx_ee* uses as errors.
*/
#ifndef LYNX_NO_EEPROM_COMPAT
#define lynx_eeread_93c46   eeprom_93c46_read
#define lynx_eeread_93c66   eeprom_93c66_read
#define lynx_eeread_93c86   eeprom_93c86_read
#define lynx_eewrite_93c46  eeprom_93c46_write
#define lynx_eewrite_93c66  eeprom_93c66_write
#define lynx_eewrite_93c86  eeprom_93c86_write
#endif /* LYNX_NO_EEPROM_COMPAT */
```

These are object-like macros expanding to the new identifiers, so calls and
address-of uses all work unchanged. There is no `__fastcall__` signature to
re-declare — the alias names the same real function.

### 4.2 Why the `LYNX_NO_EEPROM_COMPAT` switch

cc65's preprocessor has no portable per-macro deprecation attribute, so a
`lynx_ee*` use cannot self-warn. Deprecation is communicated by (a) documentation
— the header comment, the function reference, and `migrating.html`; and (b) the
`LYNX_NO_EEPROM_COMPAT` opt-out, which lets a project prove it is clean before the
shims are deleted, keeping the removal in §6 mechanical. This mirrors
`LYNX_NO_TGI_COMPAT`.

### 4.3 Assembly callers

No public asm alias module is provided: the documented contract is the C header.
If a future need arises for link-level `_lynx_ee*` aliases (e.g. a third-party
pre-built `.o`), that can be a small `eeprom-compat.s` of
`_lynx_eeread_93c46 := _eeprom_93c46_read` exports — noted here as a deliberate
non-goal, not an oversight.

## 5. Documentation

- `include/lynx/lynx.h`: the EEPROM section declarations and their doc comments
  switch to the `eeprom_*` names; the cart-access prose that references
  "the lynx_eeread_93cNN / lynx_eewrite_93cNN family" is updated to
  "the eeprom_93cNN_read / eeprom_93cNN_write family".
- `libraries/core/open.s`: the same cross-reference comment is updated.
- `doc/funcref.html`: the six reference entries and their index anchors are
  renamed (`id=`, headings, `<code>` prototypes, and the "See also" cross-links).
  Entry order is regrouped by chip (`eeprom_93c46_read`, `eeprom_93c46_write`,
  `eeprom_93c66_read`, …) so the reference reads in the new canonical grouping. A
  short note records that the `lynx_ee*` names are deprecated aliases.
- `doc/lynx.html`: EEPROM prose updated to the new names.
- `doc/migrating.html`: the §4 checklist item that currently names
  `lynx_eeread_93c46`/etc. is updated to the `eeprom_*` names, **and** a new
  dedicated deprecation section is added (§5.1 below).
- `history.html`: a dated entry recording the rename and the compatibility
  guarantee.

### 5.1 The migration-document section

Add a new top-level `<h2>` to `doc/migrating.html` titled **"EEPROM API
rename"** — following the existing numbered-section pattern (its own TOC entry
and `id`, placed after the current final section; renumber as needed). It states
that the `lynx_ee*` prefix is deprecated in favour of `eeprom_<chip>_<op>`,
explains the `LYNX_NO_EEPROM_COMPAT` opt-out and the eventual removal, and carries
the full old→new equivalence table from §2.1 verbatim so a porting user has one
place to look up every replacement. This parallels the existing
"TGI Deprecation" section (`id="sect-tgi-deprecation"`).

## 6. Removal path (future)

Because shims are deprecated rather than permanent, removal is a later, separate,
mechanical step:

1. Announce in `history.html` / release notes; keep shims for at least one tagged
   release.
2. Verify no in-tree user remains: everything builds with
   `LYNX_NO_EEPROM_COMPAT` defined.
3. Delete the six `#define`s and the `LYNX_NO_EEPROM_COMPAT` guard from `lynx.h`,
   and remove the deprecation notes from the docs (leaving only the historical
   mention in `migrating.html`/`history.html`).

## 7. Implementation order (for the future code pass)

1. In `eeprom46.s`/`eeprom66.s`/`eeprom86.s`, rename each `.export _lynx_ee*` and
   its label + prototype banner comment to `_eeprom_<chip>_<op>`.
2. In `include/lynx/lynx.h`, rename the six declarations to `eeprom_*`, add the
   `LYNX_NO_EEPROM_COMPAT`-guarded alias block (§4.1), and fix the cart-access
   prose.
3. Fix the cross-reference comment in `libraries/core/open.s`.
4. Update `doc/funcref.html` (entries, anchors, ordering, note), `doc/lynx.html`,
   `doc/migrating.html` (§4 item + new deprecation section), and `history.html`.
5. Grep the whole tree for residual `lynx_ee` (case-insensitive); every remaining
   hit must be either the intentional alias block in `lynx.h`, a deprecation note,
   or a historical mention — anything else is a miss.
6. Full sandbox rebuild (toolchain + libraries + examples), unit tests, and the
   GearLynx integration goldens; the rename must be byte-neutral — every `.lnx`
   identical to its pre-rename golden, since no code changed.

## 8. Verification that this is behaviour-neutral

The rename must not alter a single emitted byte. Acceptance check is the existing
harness: `tests/run.sh` passes (unit model 927343 checks / 0 mismatches) and the
curated GearLynx set produces `.lnx` images byte-identical to the current
goldens. Any diff means a symbol was mis-mapped, not a legitimate behaviour
change.
