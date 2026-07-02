<!--
SPDX-License-Identifier: CC-BY-4.0
Lynx Game Development SDK documentation, (c) 2026 the lynxcc authors,
licensed under Creative Commons Attribution 4.0 International.
See doc/licenses.html.
-->

# `lnx bll` — BLL/BS93 object → bootable cartridge ROM Design

Status: **IMPLEMENTED 2026-07-02**. Extends the standalone `lnx` SDK utility
(`tools/lnx/`, see `design/LYNX_LNX_TOOL_DESIGN.md`) with a command that turns a
linked **BLL/BS93 object** into a bootable raw Lynx cartridge ROM. Sources:
`tools/lnx/bllrom.{c,h}` (+ `main.c` `bll` command), the committed generated
`tools/lnx/blloader_gen.{c,h}` and its generator `tools/lnx/gen-loader.sh`
(top-level `make lnx-loader-gen`).

This design is derived from a reference JavaScript implementation of the
conversion (the `atga_romutil.js` / `atga_lnx2lyx.js` "ROM utility" that wraps a
dropped `.o` object with the standard Lynx secondary loader). This document
restates that algorithm as a native C command inside `lnx`, and grounds it in the
SDK's own BLL build path so the two stay consistent.

---

## 1. Why this belongs in `lnx`

The SDK can already **produce** a BLL object today: `cfg/lynx-bll.cfg` plus
`runtime/lynx/bllhdr.s` link a program into the 10-byte-headed BS93 object format
(`__MAIN_START__ = $0400`, 10-byte header, then the program body). What the SDK
has *no* tool for is the last mile: turning that object into a cartridge image a
Lynx (or emulator, or flash cart) will actually boot. That step — prepend the
encrypted secondary loader, write a one-entry directory pointing at the body, and
pad the whole thing to a real cart size — is exactly what the reference JS does,
and historically it was only available as a web page.

This is a **format conversion of an already-linked object**, in the same spirit
as the existing `lnx create` (which wraps a raw body with a 64-byte header). It
is *not* a second linker: `lnx bll` never resolves symbols, relocates, or reads
`cfg/*.cfg`. It consumes the finished BS93 object and emits bytes. That keeps it
inside the "post-build inspector/editor, not a linker" boundary of
`LYNX_SDK_LAYOUT_DESIGN.md` §8 that governs the rest of `lnx`.

The result closes the deferred *".lnx → raw / raw → cart"* corner noted in
`LYNX_LNX_TOOL_DESIGN.md` §7 for the BLL direction specifically.

---

## 2. The BLL/BS93 object format

A BLL object is a **10-byte header followed by the program body**. The SDK emits
it from `runtime/lynx/bllhdr.s` (segment `BLLHDR`, loaded at file offset 0 by
`cfg/lynx-bll.cfg`):

```asm
        .word   $0880                              ; bytes 0-1  (loader hint, ignored on convert)
        .dbyt   __MAIN_START__                     ; bytes 2-3  load address, BIG-endian (hi,lo)
        .dbyt   __BSS_LOAD__ - __MAIN_START__ + 10 ; bytes 4-5  block length, BIG-endian, INCLUDES the 10-byte header
        .byte   $42,$53,$39,$33                    ; bytes 6-9  ASCII "BS93"
```

So the on-disk layout, matching what the reference JS reads:

| Offset | Size | Field | Notes |
| --- | --- | --- | --- |
| 0 | 2 | loader hint | `$80 $08` as written; not used by the converter |
| 2 | 2 | `load_addr` | **big-endian** (high byte first); where the body loads in RAM (`$0400` for SDK builds) |
| 4 | 2 | `block_len` | **big-endian**; length of *header + body* (i.e. body bytes + 10) |
| 6 | 4 | magic | ASCII `"BS93"` (`0x42 0x53 0x39 0x33`) |
| 10 | .. | body | the program image; its length is `block_len - 10` |

Detection: a file is a BLL object iff bytes **6–9** equal `"BS93"`. (Note the
magic sits *after* the size/address fields, not at offset 0 — the converter must
key off bytes 6–9, exactly as the reference does. This is also why a BLL object
must never be mistaken for a 64-byte-headed `.lnx`, whose magic `"LYNX"` is at
offset 0.)

Two derived quantities the converter needs:

- `body_len = block_len - 10` — the number of payload bytes to copy.
- `load_addr = (byte2 << 8) | byte3` — stored into the directory entry
  **little-endian** in the ROM (the loader/CPU want LSB first).

Robustness: reject `block_len < 10` (would make `body_len` negative) and reject a
file shorter than `10 + body_len` (truncated object).

---

## 3. The output ROM

The output is a **raw cartridge image** — no 64-byte `LYNX` header — identical in
shape to the reference tool's `.lyx` download. Structure:

```
+---------------------------------------------------------------+
| 0x000 .. 0x0CA   secondary loader (encrypted boot + micro-loader)   203 bytes
| 0x0CB .. 0x0D2   one-entry Lynx directory (8 bytes)                   |
| 0x0D3 .. 0x0D3+body_len-1   the BS93 body, copied verbatim            |
| ...              zero padding to the chosen cart size                 |
+---------------------------------------------------------------+
```

`0x0D3 = 211`, so the body always starts at offset 211 — the loader length.

### 3.1 The directory entry (offset `0xCB`, 8 bytes)

This is a single Lynx directory entry, laid out exactly like the SDK's own
`runtime/lynx/defdir.s` (which places `__STARTOFDIRECTORY__` at `$00CB`):

| ROM offset | Field | Value written |
| --- | --- | --- |
| `0xCB` | block | `0` (body lives in block 0) |
| `0xCC`–`0xCD` | offset-in-block | `211` little-endian (`0xD3 0x00`) |
| `0xCE` | flags | `0x88` (executable) |
| `0xCF`–`0xD0` | load address | `load_addr` little-endian |
| `0xD1`–`0xD2` | length | `body_len` little-endian |

That the SDK's `defdir.s` directory format (`.byte block; .word offset; .byte
$88; .word addr; .word len`) is **byte-for-byte the same** as the fields the
reference loader expects is the key compatibility fact this design leans on:
the loader embedded in the ROM prefix and the SDK's own loader agree on where the
directory sits and how each entry is shaped.

### 3.2 Cart size, block size, and padding

The loader needs to know the cart's **block size** so it can read a body that
spans block boundaries (`seclynxblock` in `runtime/lynx/bootldr.s` reloads the
per-block byte counter with `$100 - (blocksize >> 8)`). The SDK loader bakes this
as an assembly-time immediate driven by `__BANK0BLOCKSIZE__`, and the resulting
byte is exactly the reference tool's cart-size byte:

| Cart size | block size (`size / 256`) | loader immediate `$100-(blk>>8)` | LNX `page_size_bank0` (§5) |
| --- | --- | --- | --- |
| 128 KiB (`131072`) | 512 (`$0200`) | `$FE` | 512 |
| 256 KiB (`262144`) | 1024 (`$0400`) | `$FC` | 1024 |
| 512 KiB (`524288`) | 2048 (`$0800`) | `$F8` | 2048 |

Every supported cart is 256 blocks; the block size scales with the cart size, and
offset 211 always fits inside block 0 (211 < 512). Because this design **relinks
the SDK loader per block size** (§4) rather than patching a fixed byte offset,
`lnx bll` simply selects the loader variant for the chosen size — there is no
hard-coded `0xC4` patch. The image is zero-padded from `211 + body_len` up to the
exact cart size.

### 3.3 Choosing the size

The body plus the 211-byte prefix must fit: the requirement is
`211 + body_len <= cart_size`, i.e. `body_len <= cart_size - 211`. Behaviour:

- **Default (auto):** pick the smallest of 128/256/512 KiB that satisfies the fit
  check. This mirrors the reference tool, which offers only the sizes that fit.
- **`--size N`:** force 128, 256, or 512 (KiB). Error if the body does not fit the
  forced size.
- If the body does not fit even 512 KiB (`body_len > 524288 - 211 = 524077`),
  error out — the object is too large for a single-bank cart.

---

## 4. The loader prefix — generated from the SDK's own build

Rather than embedding a foreign 211-byte blob, `lnx bll` uses the SDK's **own**
loader (`runtime/lynx/bootldr.s`, "Karri Kaksonen, 2011"), so the bytes are
in-tree, carry the SDK's own license (MPL-2.0, per
`design/LYNX_LICENSE_POLICY_DESIGN.md`), and provably match the loader the SDK
ships in every full ROM. This is viable because the SDK loader is the same family
as the reference's: it reads the 8-byte directory at `$CB`, boots block 0 at
offset 211, and uses the identical `$FE/$FC/$F8` cart-size encoding (§3.2).

The tool needs only the **203-byte loader region** (`0x00`–`0xCA`, i.e. up to
`__STARTOFDIRECTORY__`); it writes the 8-byte directory (§3.1) itself and appends
the body. That region is program-independent — the only thing that varies is the
block-size immediate — so exactly **three variants** are needed, one per block
size (512 / 1024 / 2048).

### 4.1 Generation mechanism

A generator produces the three loader regions from the SDK loader using the
just-built toolchain (`ca65` + `ld65`), overriding `__BANK0BLOCKSIZE__` to
`$0200`, `$0400`, `$0800` in turn:

1. Assemble/link `runtime/lynx/bootldr.s` with a minimal loader-only linker
   config (places only the `BOOTLDR` segment, defines `__BANK0BLOCKSIZE__` and
   `__STARTOFDIRECTORY__ = $CB`, and the ZP symbols the loader references), or
   equivalently link a canonical stub with `cfg/lynx.cfg`.
2. Take the 203-byte `BOOT` region (offsets `0`–`0xCA` of the raw image, i.e. the
   image minus the 64-byte `LYNX` header when produced via `lynx.cfg`).
3. Emit `tools/lnx/blloader_gen.c` (+ `.h`): three
   `static const unsigned char bll_loader_{512,1024,2048}[203]` tables.

**Build ordering.** The top `Makefile` builds `compiler` → `tools` → libraries →
examples, so at the moment `lnx` compiles, the runtime library and any linked ROM
do **not** yet exist — the tool cannot compile against a freshly generated blob
in the same pass without inverting the graph. The design therefore treats
`blloader_gen.c` as a **committed generated artifact**, exactly like the SDK's
`.sha256` goldens: it is checked in, regenerated on demand by a make target
(e.g. `make -C tools lnx-loader-gen`, which runs after the compiler exists), and
guarded against drift by a test (§7). `tools/` then compiles `lnx` against the
committed file with no new ordering dependency. Its provenance is recorded in a
source-file comment and `doc/licenses.html` cross-references `bootldr.s`.

This keeps the loader genuinely SDK-derived and license-clean while avoiding both
a foreign blob and a build-graph inversion.

---

## 5. Command-line interface

Add one command to `lnx` (`main.c` dispatch + a new `bllrom.c/.h` module):

```
lnx bll [options] <object.o>

Convert a BLL/BS93 object into a bootable raw Lynx cartridge ROM.

options:
  -o, --output <file>     write the ROM here (required)
  --size 128|256|512      target cart size in KiB (default: smallest that fits)
  --lnx                   also prepend a 64-byte LYNX header (default: raw image)
  --config <file.json>    header fields for --lnx (see LYNX_LNX_TOOL_DESIGN.md §4)
  --cartname <str>        }
  --manufacturer <str>    } header fields for --lnx; reuse the existing
  --rotation ...          } patch/create field options and precedence
  --audin / --eeprom* ... }
```

Notes:

- `-o` is required (like `create`).
- Default output is the **raw** padded cart image (the reference `.lyx`).
- `--lnx` prepends a 64-byte `LYNX` header built from the documented defaults
  overlaid by `--config`/field flags, reusing the existing `LnxHeaderDefaults` +
  `ApplyOptions` path. When `--lnx` is used and the user did not set `--bank0`,
  the header's `page_size_bank0` defaults to the chosen cart's block size (§3.2:
  512 / 1024 / 2048) so the header agrees with the loader's addressing.
  Emulators strip the 64-byte header and boot the raw cart underneath, so the
  loader's own directory offsets (relative to the raw cart start) remain correct.
- Because `--lnx` composability already exists via `lnx create`, `--lnx` is a
  convenience: `lnx bll obj.o -o game.lyx` followed by
  `lnx create game.lyx -o game.lnx [fields]` is an equivalent two-step path and a
  useful cross-check in testing.

### 5.1 Suggested end-to-end build path

```
cl65 -t lynx -C cfg/lynx-bll.cfg -o game.o game.c   # link a BS93 object
lnx bll game.o -o game.lyx                           # wrap into a bootable ROM
lnx info game.lyx --lnx-created ...                  # (optional) inspect via --lnx
```

(The first line is the existing SDK BLL build; only the second is new.)

---

## 6. Source layout

```
tools/lnx/
├── main.c            # add `bll` command parse + dispatch (CmdBll)
├── bllrom.h/.c       # NEW: BS93 parse, size select, directory, emit ROM
├── blloader_gen.h/.c # NEW: committed generated 3× 203-byte SDK loader regions
├── lnxhdr.h/.c       # unchanged; reused for the optional --lnx header
└── jsoncfg.h/.c      # unchanged; reused for --lnx --config
```

`blloader_gen.c` is generated (§4.1), not hand-edited. A make target regenerates
it from `runtime/lynx/bootldr.s` using the built toolchain; the committed copy is
what `tools/` compiles against.

`bllrom.c` stays plain C99 + libc (the `tools/lnx` constraint,
`LYNX_SDK_LAYOUT_DESIGN.md` §5/§8): no dependency on `compiler/common`, no
third-party JSON, and it reuses `ReadFile`/`WriteFile` already in `main.c`
(promote them to a shared spot or expose via a small internal header).

Proposed `bllrom.h` surface:

```c
#define BLL_MAGIC        "BS93"   /* at object offset 6 */
#define BLL_LOADER_LEN   211      /* 0xD3: loader + directory prefix */
#define BLL_HDR_LEN      10       /* BS93 10-byte object header */

/* Sizes and their 0xC4 cart-size byte. */
typedef enum { BLL_128K = 131072, BLL_256K = 262144, BLL_512K = 524288 } BllSize;

/* Parse the 10-byte BS93 header from `obj` (>= 10 bytes). Returns 0 on success,
** filling *load_addr and *body_len; -1 on bad magic / bad length. */
int  BllParseObject(const unsigned char* obj, size_t obj_size,
                    unsigned* load_addr, size_t* body_len);

/* Pick the smallest cart size that fits body_len (or validate a forced one).
** Returns the BllSize, or 0 if body_len does not fit even 512 KiB. */
BllSize BllChooseSize(size_t body_len, BllSize forced /* 0 = auto */);

/* Build the raw cart image into a freshly malloc'd buffer of exactly `size`
** bytes: the SDK loader region for `size` (blloader_gen), then the 8-byte
** directory built here (block 0, off 211, 0x88, load_addr, body_len), then the
** body, then zero pad. Returns the buffer (caller frees) or NULL on OOM. */
unsigned char* BllBuildRom(const unsigned char* body, size_t body_len,
                           unsigned load_addr, BllSize size);
```

---

## 7. Behaviour guarantees / verification

- **Directory correctness.** Bytes `0xCB`–`0xD2` decode to block 0, offset 211,
  flags `0x88`, the object's load address, and its body length.
- **Boot test.** The produced ROM boots in the in-tree GearLynx emulator
  (`tests/emu/gearlynx`) and runs the program — the real acceptance test, since
  a wrong loader/directory would fail to launch.
- **Loader drift guard.** A test strips the 64-byte header off a freshly built
  example `.lnx` (default block size 1024) and asserts its first 203 bytes equal
  the committed `bll_loader_1024[]`, so the generated loader cannot drift from
  `runtime/lynx/bootldr.s`. The `$FE/$FC/$F8` byte in each variant is checked
  against `$100-(blk>>8)` (§3.2).
- **Cross-check with the reference.** Because the SDK loader shares the reference
  family's directory layout and cart-size encoding, a ROM built by `lnx bll` is
  expected to boot identically to one from the reference tool; the loader *bytes*
  legitimately differ (SDK-signed vs reference-signed encrypted block).
- **Fit / bounds.** `body_len > 524077` errors; a forced `--size` too small
  errors; a truncated or non-`BS93` object errors with a clear message.
- **`--lnx` round-trip.** `lnx bll --lnx` output has a valid 64-byte header
  (`lnx info` reports the configured fields) followed by the identical raw ROM
  that the non-`--lnx` path produces.

Per `CLAUDE.md`, implementation does a full sandbox rebuild (toolchain + lib +
examples) and runs `tests/run.sh`.

---

## 8. Scope and non-goals

In scope: convert a single-block BS93 object (as produced by `cfg/lynx-bll.cfg`)
into a 128/256/512 KiB raw cart ROM, with an optional 64-byte `LYNX` header.

Out of scope (kept intentional, not oversight):

- **Multi-block / multi-directory objects.** The reference and this design write
  one directory entry for one code block at offset 211. Objects that need several
  directory entries (multiple loadable segments, bank 1) are not handled; that is
  the linker's/`defdir.s`'s territory, not a post-build wrap.
- **Bank-1 carts.** Single bank only (`page_size_bank1 = 0`).
- **Linking.** `lnx bll` never resolves symbols or reads `cfg/*.cfg`; it consumes
  a finished object. Producing the object stays `cl65`/`ld65` + `lynx-bll.cfg`.
- **`.lnx` / `.lyx` → raw strip.** The companion direction from the reference JS
  (drop a 64-byte header, pad to size) remains the deferred strip item in
  `LYNX_LNX_TOOL_DESIGN.md` §7; it is unrelated to the BLL wrap and can be a
  separate small command later.

---

## 9. Documentation & memory sync (at implementation time)

Per `CLAUDE.md`, implementing this ships alongside:

- `doc/lnx.html` — new `bll` command section (CLI, the BS93 object format table,
  the ROM layout, cart sizes), plus an SVG of the ROM layout following
  `design/DOC_SVG_STYLE_DESIGN.md`.
- `tools/lnx/main.c` usage text (`Usage`) gains the `bll` command line.
- `tools/Makefile` (+ `lnx.vcxproj`) — the `lnx-loader-gen` regeneration target
  and `blloader_gen.c` in the `lnx` sources; `doc/licenses.html` notes the
  loader's derivation from `runtime/lynx/bootldr.s`.
- `LYNX_LNX_TOOL_DESIGN.md` §3/§7 — note `bll` and flip the BLL corner of the
  deferred list to point here (cross-reference added with this design).
- This file's status flipped to IMPLEMENTED.
- an auto-memory note for the change.
