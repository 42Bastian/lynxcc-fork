<!--
SPDX-License-Identifier: CC-BY-4.0
Lynx Game Development SDK documentation, (c) 2026 the lynxcc authors,
licensed under Creative Commons Attribution 4.0 International.
See doc/licenses.html.
-->

# `lnx` — `.lnx` Header Inspector / Patcher Tool Design

Status: **IMPLEMENTED 2026-06-22** (phase 8 of `LYNX_SDK_LAYOUT_DESIGN.md`).
Source of truth for the standalone `lnx` SDK utility under `tools/lnx/`.

`lnx` is the first entry in `tools/` — the home for **new** standalone SDK
binaries that are not part of the cc65-derived compiler suite
(`LYNX_SDK_LAYOUT_DESIGN.md` §8). It is a post-build inspector/editor for the
64-byte `.lnx` cartridge header: it does **not** link or generate cartridge
images (that stays `ld65` + `cfg/*.cfg`), and it does **not** convert sprites
(that is `sp65`). It only reads and rewrites the header that sits in front of an
already-linked image, plus wraps a raw image with a fresh header.

---

## 1. Why this tool exists

A linked `.lnx` carries a fixed 64-byte header whose cart-name, manufacturer and
rotation fields are baked in by `runtime/lynx/exehdr.s` at assembly time. Today
the only way to change "Cart name"/"Manufacturer"/rotation for a given game is to
edit `exehdr.s` and relink — there is no per-game knob. `lnx` makes the header a
**post-link, per-game configuration**: a project keeps a small JSON file
describing its cartridge metadata, and `lnx patch --config game.json game.lnx`
stamps that metadata onto the built image. The build itself is unchanged; the
identity of the cartridge is data, not source.

This directly serves the SDK goal of per-game configuration without forking the
shared runtime.

---

## 2. The `.lnx` header (64 bytes)

The header is emitted by `runtime/lynx/exehdr.s` and consumed by emulators
(Handy, Mednafen) and flash carts. Layout, little-endian words:

| Offset | Size | Field | Notes |
| --- | --- | --- | --- |
| 0 | 4 | magic | ASCII `"LYNX"` |
| 4 | 2 | `page_size_bank0` | bank-0 cart block size (e.g. `1024`) |
| 6 | 2 | `page_size_bank1` | bank-1 block size (`0` if single bank) |
| 8 | 2 | `version` | format version, `1` |
| 10 | 32 | `cartname` | NUL-padded ASCII |
| 42 | 16 | `manufname` | NUL-padded ASCII |
| 58 | 1 | `rotation` | `0` none, `1` left, `2` right |
| 59 | 1 | `audin` | AUDIN used for addressing: `0` no, `1` yes |
| 60 | 1 | `eeprom` | EEPROM flag bit field (below) |
| 61 | 3 | `spare` | reserved, written as zero |

The **EEPROM flag** byte (offset 60) is a bit field:

- **bits 0-2** — EEPROM chip: `0` none, `1` 93c46 (the common 128-byte part),
  `2` 93c56, `3` 93c66, `4` 93c76, `5` 93c86. Size in bits is `2^(chip+9)`.
- **bits 3-5** — reserved (zero).
- **bit 6** — `0` real EEPROM chip, `1` LynxSD save file (set when both are
  supported).
- **bit 7** — word size: `0` 16-bit (common), `1` 8-bit.

`lnx` treats a file as headed if its first four bytes are `LYNX`; otherwise it is
"raw" (a headerless image, as produced by `ld65` with a no-header cfg, or any
binary the user wants to wrap).

The string fields are stored fixed-width and NUL-padded. `lnx` truncates an
over-long value to fit (31 chars + NUL for `cartname`, 15 + NUL for `manufname`)
and warns; it never overruns the field.

---

## 3. Command-line interface

```
lnx <command> [options] <file>

Commands:
  info    <file.lnx>            Print the header fields in human-readable form
  dump    <file.lnx>            Hex + ASCII dump of the raw 64-byte header
  patch   [field-opts] <file>   Rewrite header fields of an existing .lnx
  create  [field-opts] <raw>    Wrap a raw image with a fresh 64-byte header

Field options (patch / create):
  --config <file.json>          Read header fields from a JSON config (§4)
  --cartname <str>              Set the 32-byte cart name
  --manufacturer <str>          Set the 16-byte manufacturer name
  --rotation none|left|right    Set the rotation flag (also accepts 0|1|2)
  --audin 0|1                   Set the AUDIN-addressing flag
  --eeprom <n>                  Set the whole EEPROM flag byte (0..255 bit field)
  --eeprom-chip <name|0-5>      Set EEPROM chip (none|93c46|93c56|93c66|93c76|93c86)
  --eeprom-lynxsd 0|1           Set the LynxSD-save bit (bit 6)
  --eeprom-word 8|16            Set the EEPROM word size (bit 7)
  --bank0 <n>                   Set bank-0 page size
  --bank1 <n>                   Set bank-1 page size
  --version <n>                 Set the header version word
  -o, --output <file>           Write result here (default: patch in place;
                                create requires -o)

General:
  -h, --help                    Usage
  -V, --version                 Tool version
```

Notes:

- `info` and `dump` are read-only.
- `patch` edits an existing headed file. With no `-o` it rewrites the header in
  place (the body is untouched); with `-o` it copies the file and rewrites the
  copy's header. It errors if the input is not a headed `.lnx`.
- `create` takes a raw (headerless) image and writes `<header><raw body>` to the
  `-o` target. A fresh header starts from documented defaults
  (`bank0=1024 bank1=0 version=1 rotation=none audin=0 eeprom=0`, blank names)
  and is then overlaid with the config/flags.
- **Precedence:** defaults (create) or the existing header (patch) → `--config`
  values → explicit CLI field flags. So a flag always wins over the same field in
  the config, letting a build pass a one-off override on top of the checked-in
  per-game JSON.

---

## 4. JSON per-game configuration

The per-game config is a flat JSON object. Every key is optional; only present
keys change the header. Unknown keys are an error (catches typos rather than
silently doing nothing). Example `game.json`:

```json
{
  "cartname": "Sybil's Quest",
  "manufacturer": "lynxcc SDK",
  "rotation": "left",
  "audin": 0,
  "eeprom_flag": 1,
  "bank0_page_size": 1024,
  "bank1_page_size": 0,
  "version": 1
}
```

Key → header field mapping:

| JSON key | Header field | JSON type |
| --- | --- | --- |
| `cartname` | `cartname` | string |
| `manufacturer` | `manufname` | string |
| `rotation` | `rotation` | string `none`/`left`/`right` or number `0`/`1`/`2` |
| `audin` | `audin` | number `0`/`1` or boolean |
| `eeprom_flag` (alias `eeprom`) | `eeprom` whole byte | number `0`..`255` (bit field, §2) |
| `eeprom_chip` | `eeprom` bits 0-2 | string chip name or number `0`..`5` |
| `eeprom_lynxsd` | `eeprom` bit 6 | number `0`/`1` or boolean |
| `eeprom_word_size` | `eeprom` bit 7 | number `8` or `16` |
| `bank0_page_size` | `page_size_bank0` | number |
| `bank1_page_size` | `page_size_bank1` | number |
| `version` | `version` | number |

A minimal hand-written JSON reader lives in `tools/lnx/jsoncfg.c` — it accepts a
single flat object of string and number values, the strict subset this config
needs (objects, strings with the common `\` escapes, integers, and the `true`/
`false`/`null` literals for forward-tolerance). It deliberately does **not**
implement nested objects, arrays, or floating point: the config has no use for
them, and rejecting them keeps the tool free of a third-party JSON dependency
(`tools/lnx` links nothing beyond libc, per `LYNX_SDK_LAYOUT_DESIGN.md` §5/§8).

### 4.1 EEPROM byte composition

The EEPROM flag byte can be set either as a whole (`--eeprom` / `eeprom_flag`)
or field by field (`--eeprom-chip`/`--eeprom-lynxsd`/`--eeprom-word`, or the
`eeprom_chip`/`eeprom_lynxsd`/`eeprom_word_size` keys). These are composed onto a
base byte in a **fixed order, independent of the order they were supplied**
(`LnxEepromSpec` / `LnxEepromCompose` in `lnxhdr.c`): the base is the raw byte if
one was given, else the header's existing EEPROM byte; then the chip code
overlays bits 0-2, the LynxSD flag bit 6, and the word size bit 7. Unset
fields leave the base byte's bits untouched, so a single field can be flipped
without disturbing the others. Because the config's fields are gathered and
composed as one step after the object is parsed, the result never depends on key
order in the JSON file.

This composition runs once per source (config, then CLI), so the standard
precedence holds at the bit level too: a `--eeprom-lynxsd 0` flag clears the bit
even if the config set it, and `--eeprom-chip` overrides a chip chosen in the
config while leaving the config's word-size choice intact.

---

## 5. Source layout

```
tools/
├── Makefile          # builds lnx into the root bin/ (mirrors compiler/Makefile)
├── lnx.vcxproj       # MSVC project (emits to ..\bin\), registered in cc65.sln
└── lnx/
    ├── main.c        # CLI parse + dispatch (info/dump/patch/create)
    ├── lnxhdr.h/.c   # LnxHeader struct: load/save, field setters, pretty-print
    └── jsoncfg.h/.c  # minimal JSON reader + apply-to-header
```

`tools/lnx` does **not** depend on `compiler/common`; it is plain C99 + libc, so
the `tools/` Makefile is a trimmed copy of `compiler/Makefile`'s program pattern
with no `common.a` link and no `common` include dir. All binaries still land in
the single root `bin/` (the §2 hard constraint of the layout design); the
top-level `Makefile` builds `compiler` then `tools` so the toolchain exists first
(§12).

---

## 6. Behaviour guarantees / verification

- **Body-preserving.** `patch` only rewrites bytes `0..63`; `info`/`dump` never
  write. A `patch` with no field options is a no-op on the bytes (idempotent).
- **Round-trip.** `lnx patch --config c.json f.lnx` followed by `lnx info f.lnx`
  reports exactly the configured values; re-running `patch` with the same config
  leaves the file byte-identical.
- **Field bounds.** Over-long names are truncated to the fixed field width with a
  warning; the 64-byte header size never changes.
- **Graceful raw detection.** `patch`/`info`/`dump` on a non-`LYNX` file error
  with a clear message pointing at `create`; `create` on a file that already
  starts with `LYNX` warns (it would double-head an image).

Verification (per `CLAUDE.md`, full rebuild each change):

1. `lnx info`/`dump` against a freshly built `examples/*.lnx` matches the
   `exehdr.s` defaults (`Cart name` / `Manufacturer` / rotation 0).
2. `lnx patch --config` on a copy, then `info`, shows the new values and leaves
   the post-header body byte-identical (`cmp` from offset 64).
3. `create` from a raw `ld65` body reproduces a byte-identical header to the
   `patch` path for the same fields.

---

## 7. Scope and non-goals (this phase)

In scope (phase 8): header `info`, `dump`, `patch` (CLI flags **and** JSON
config), and `create` (raw → `.lnx`), covering every documented header field —
names, rotation, bank page sizes, version, the AUDIN flag, and the EEPROM flag
bit field (`info` decodes the EEPROM chip/word-size/LynxSD bits for the reader).

Deferred (future `tools/lnx` work, tracked here so the omission is intentional,
not an oversight):

- **Directory/segment listing and extraction.** The Lynx directory that follows
  the header (`runtime/lynx/defdir.s`) describes the loadable blocks; a future
  `lnx list` / `lnx extract` could walk it. Not built now — the per-game header
  configuration the SDK needed is the header, not the directory.
- **`.lnx` → raw `strip`.** Trivial (drop the first 64 bytes) but unneeded until
  a consumer asks; `create` covers the raw → `.lnx` direction that the build path
  uses.
- **BLL/BS93 object → bootable cart ROM.** Wrapping a linked BLL object (from
  `cfg/lynx-bll.cfg`) with the secondary loader and a one-entry directory is
  designed separately in `design/LYNX_LNX_BLL_ROM_DESIGN.md` (a new `lnx bll`
  command). It is a post-build format conversion, not linking, so it fits `lnx`.
- *(No EEPROM gaps.)* Both the whole byte and every individual field (chip,
  LynxSD, word size) are settable from CLI and JSON, and `info` decodes them
  (§4.1). Nothing EEPROM-related is deferred.

These keep `lnx` from drifting into `ld65`'s territory (`LYNX_SDK_LAYOUT_DESIGN.md`
§8: "a post-build inspector/editor, not a second linker").

---

## 8. Documentation & memory sync

Per `CLAUDE.md`, this design ships alongside:

- `doc/lnx.html` — user guide (CLI, JSON config, header table), carded under
  "Programs" on `index.html` alongside the other utility tool pages
  (`ar65`/`co65`/`da65`/`sp65`), and using the standard doc nav. (The nav lists
  only the four primary tools — `cc65`/`ca65`/`ld65`/`cl65` — so utility tools
  are reached via the index cards, the existing convention.)
- `README.md` — `tools/` line in the repository-layout overview.
- `LYNX_SDK_LAYOUT_DESIGN.md` — §8 table row and phase-8 status flipped to
  IMPLEMENTED.
- an auto-memory note for the phase.
