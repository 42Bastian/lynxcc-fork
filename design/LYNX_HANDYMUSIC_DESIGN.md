<!--
SPDX-License-Identifier: CC-BY-4.0
Lynx Game Development SDK documentation, (c) 2026 the lynxcc authors,
licensed under Creative Commons Attribution 4.0 International.
See doc/licenses.html.
-->

# HandyMusic Integration Design (`hmcc` tool + `lynx-handymusic` library)

Status: **PARTIALLY IMPLEMENTED.** The **host tool track (§3) is done** — `hmcc`
lives in `tools/hmcc/`, builds to `bin/hmcc` from the shared `PROGS` list in
`tools/Makefile`, and is documented at `doc/hmcc.html`. The **runtime library
track (§4) is still planned** — the ca65 driver port, `lib/lynx-handymusic.lib`,
`include/lynx/handymusic.h`, the ported example, and the driver reference page do
not exist yet. This document remains the source of truth for how Osman Celimli's
*HandyMusic 1.40cx+* driver and its `HMCC` script compiler are brought into
**lynxcc** as (1) a host build tool and (2) a linkable audio library games can
pull in. Section-level status is called out in §3, §6 and §8 below.

Upstream package: `HandyMusic_v1-40cx+.zip` — driver source (`HandyMusic/*.asm`,
lyxass/BLL syntax), `HMCC/` (the Win32 script compiler), the *HandyMusic
Programmer's Manual* (25pp PDF), and `SASS/` demo scripts. Author's grant, from
the manual: *"HandyMusic is completely free to use and modify in your own
projects."*

---

## 1. What HandyMusic is, and why it does not "just link"

HandyMusic is a script-driven, macro-instrument sound driver: instruments and
sound effects are byte-code envelopes (waveform/volume/frequency scripts with
loops and note-off sections), music is four parallel track scripts referencing
those instruments, and single-channel 8 kHz PCM samples are streamed off the
cart. It is heavier and more capable than the existing **lynxcc** engines
(`libraries/audio/lynx-snd.s` note engine and the `sfx` catalogue) — it is an
*alternative* BGM+SFX engine, not a replacement, and should be packaged so a
game opts into one or the other, never both at once.

Four properties of the upstream code mean it cannot be dropped into the tree as-is:

1. **Wrong assembler.** The driver is written for Bastian Schick's **lyxass**
   (BLL kit): `ORG $A000`, `include`/`path "c:\lynx\HandyMusic"`, `::` global
   labels, `ds`/`dc.b`, `LDA#imm`, `echo`, and `%H`-style symbol interpolation.
   **lynxcc** assembles with **ca65**. Every source file needs a syntax port.

2. **Hard-coded absolute layout.** The manual and source fix the driver at
   `$A000–$A6AF` and music data at `$B000–$BFFF`, and the compiled `.mus`/`.sfx`
   blobs contain *absolute* address tables baked to a chosen base. **lynxcc**
   links relocatable segments against a cc65 memory config (stack pinned at
   `$C038`, reclaimable startup, etc.). Absolute ORGs and pre-baked pointer
   tables fight relocatable linking directly (see §4).

3. **BLL runtime dependencies.** The PCM path and `HandyMusic_LoadPlayBGM` call
   BLL filesystem/IRQ primitives that do not exist in **lynxcc**: `LoadFile`,
   `LoadDir`, `SelectBlock`, `ReadOver`, the `entry+…` directory struct,
   `irq_vecs`, `_IOdat`, and the `FileNum_MusicBase`/`FileNum_SampleBase`
   constants. **lynxcc** has its own cart API (`openn`/`lseek`/`lynx-cart.s`)
   and IRQ handling (`set_irq`). These must be remapped or the feature gated off.

4. **Windows-only tool.** `HMCC` is Win32 C (`windows.h`, `CreateFile`/
   `WriteFile`, `_CrtSetDbgFlag`). It will not compile in the Linux sandbox or on
   the shared `tools/` build. It needs the same portability treatment `abcrom`
   received (pure file-to-file, no `system()`/`windows.h`).

The work therefore splits cleanly into a **host tool track** (§3) and a
**runtime library track** (§4), plus data-flow, docs, and licensing (§5–§7).

## 2. Tool vs. library — where each piece lands

| Upstream piece | Nature | lynxcc home | Model |
|---|---|---|---|
| `HMCC/*.c` | Win32 host compiler | `tools/hmcc/` | **First-party portable port** (like `abccc`/`abcrom`), *not* `tools/extern/` |
| `HandyMusic/*.asm` | lyxass 6502 driver | `libraries/audio/handymusic/` → `lib/lynx-handymusic.lib` | **ca65 port** |
| `include/lynx/handymusic.h` | (new) | C-callable API surface | new header |
| `SASS/*` demos | script samples | `examples/mikey/handymusic/` | ported sample |
| Manual PDF | reference | `doc/handymusic.html` | themed doc page |

Why `HMCC` is **not** a `tools/extern/` subtree: the vendored-tools policy
(`LYNX_EXTERN_TOOLS_DESIGN.md`) requires a **live upstream git repo kept
byte-identical**. HandyMusic ships as a zip with no upstream repo, and the tool
*must be modified* to become portable. That is exactly the case the extern
policy excludes ("external code is never modified"). So `hmcc` follows the
`abccc`/`abcrom` precedent instead: a **lynxcc-authored** tool that credits
Osman Celimli and records the "free to use and modify" grant, living under
`tools/hmcc/` and built via `PROGS` in `tools/Makefile`.

## 3. Host tool track — `tools/hmcc`  *(IMPLEMENTED)*

### 3.1 Port `HMCC` to portable C  *(done)*

The Win32 I/O and debug scaffolding were rewritten and the parser logic kept, as
planned. As shipped in `tools/hmcc/`:

- `CreateFile`/`WriteFile`/`CloseHandle` are replaced with `fopen`/`fwrite`/`fclose`.
- `windows.h`, `crtdbg.h`, and the `_CrtSetDbgFlag` block are dropped entirely.
- The translation units that carry the real logic are kept unchanged in spirit:
  `hmcc.c` (CLI), `instfx.c`/`instfx.h` (instrument + SFX script assembler),
  `music.c`/`music.h` (track assembler), `notes.c`, and `common.c`/`common.h`.
- The CLI contract from `hmcc.c` is preserved so scripts and docs stay valid:
  - `hmcc $dest_addr sfx_file.txt` → `sfx_file.sfx` (+ `sfx_file.equ`)
  - `hmcc $dest_addr out.mus inst.txt trk1.txt [trk2..4]` → `out.mus`
- Built with the strict first-party host `CFLAGS` (unlike the relaxed flags used
  for vendored extern tools).

`tools/Makefile` carries `hmcc` directly on the `PROGS` line
(`PROGS = lnx abccc abcrom hmcc`), mirroring the `abccc`/`lnx` entries with no
`EXTERN_*` machinery. One addition not in the original sketch: `notes.c` uses
`pow()` from libm, so the Makefile sets a per-program `hmcc_LDLIBS = -lm` to keep
that dependency local to `hmcc` (the other `PROGS` link nothing extra). Output is
the shared `bin/hmcc`.

### 3.2 The base-address problem the tool encodes

`HMCC` takes `$dest_addr` and bakes **absolute** SFX/instrument address tables
into the output (`baseSFXAddress = baseAddress + 16`, `InstFX_MakeEquates`,
`InstFX_BuildBin`). The `.mus` header is 16 bytes (`MUS_HDR_LENGTH`) of absolute
track and instrument-table pointers. This means the produced data is only valid
if it is loaded at exactly `$dest_addr`.

**Decision: fixed-region model only.** Reserve a `HANDYMUSIC_DATA` region in the
Lynx cfg at a known address, pass that address as `$dest_addr`, and `.incbin` the
blob. This matches the original BLL design 1:1 and keeps `hmcc` a faithful,
minimally-changed port (I/O and debug scaffolding only). A relocatable
object-output mode for `hmcc` (symbolic tables resolved by ld65) is explicitly
**out of scope** — it would be a large, invasive change to code we are porting
unmodified in spirit, and the fixed region is sufficient for the Lynx's memory
budget.

## 4. Runtime library track — `lynx-handymusic`  *(PLANNED — not yet implemented)*

### 4.1 ca65 syntax port

Port the five driver files to ca65, preserving the public entry points the
manual documents (the `::` globals become `.export`ed symbols):

- Init/flow: `HandyMusic_Init`, `HandyMusic_Main`, `HandyMusic_Pause`,
  `HandyMusic_UnPause`, `HandyMusic_StopAll`.
- Music: `HandyMusic_PlayMusic`, `HandyMusic_StopMusic`, `HandyMusic_LoadPlayBGM`.
- SFX: `HandyMusic_PlaySFX`, `HandyMusic_StopSoundEffect`.
- PCM: `PlayPCMSample`, `PCMSample_IRQ`.

Mechanical transforms: `ORG` → named segments; `include`/`path` → `.include`;
`::` → label + `.export`; `ds N` → `.res N`; `dc.b` → `.byte`; `LDA#x` →
`lda #x`; delete `echo`/`%H` debug lines; convert local `.label` dot-labels to
ca65 cheap-locals (`@label`). The CPU is already 65SC02 across the tree, so
`STZ`/`PHX`/`BRA` need no guards.

### 4.2 Memory placement — ZP and segments

HandyMusic's `HM_ZP.asm` declares a large block of per-channel state as
zero-page (`ds` under an implicit ZP origin in BLL). **lynxcc** has a bounded ZP
budget and a `__zeropage__` mechanism. Plan:

- Keep only the genuinely hot, indexed-through pointers in ZP
  (`HandyMusic_Channel_DecodePointer`, `HandyMusic_Music_DecodePointer`, the
  enable/active/BGM flags).
- Move the bulk arrays (`HandyMusic_Channel_*` frequency/volume/loop tables,
  `HandyMusic_Music_*`) into a dedicated `HANDYMUSIC_BSS` segment in RAM.
- Audit the driver for `,X`/`,Y` addressing that *requires* ZP (zero-page
  indexed) vs. absolute-indexed that works anywhere — the frequency envelope
  code indexes these arrays heavily and must be checked instruction-by-
  instruction, because moving a ZP array to absolute changes both correctness
  (addressing mode) and cycle cost. This audit is the main correctness risk and
  gets its own verification pass (§8).

Driver code (~1.7 KB) goes in a `HANDYMUSIC` code segment; the design keeps it
in the reclaimable/normal RAM area chosen by the cfg, not a fixed `$A000`.

### 4.3 Music/SFX data region and the cfg

Provide a `lynx-handymusic.cfg` (or a documented add-on segment) that reserves
the music-data region whose base is handed to `hmcc`. The game `.incbin`s the
`hmcc` output there and points the six `HandyMusic_SFX_AddressTable*` pre-init
variables and the `.mus` header at it. This preserves the "data is absolute to
its base" contract without patching the tool.

### 4.4 PCM playback — RAM-sourced, not cart-streamed

The PCM streamer and `LoadPlayBGM` are the only parts touching BLL internals.

**Decision: replace cart streaming with RAM-sourced PCM.** The upstream design
streams samples byte-by-byte off the cart precisely to keep RAM use tiny, but
long PCM on the Lynx is a bad trade regardless (cart bandwidth, IRQ overhead), so
it is not a goal here. Instead:

- **Reband `PlayPCMSample`/`PCMSample_IRQ` to read from a RAM buffer.** Keep the
  same timer-3 8 kHz IRQ that writes `$FD22` (direct volume) and the same
  channel-0 capture via `HandyMusic_Channel_NoWriteBack`, but replace the
  `LoadDir`/`SelectBlock`/`ReadOver`/`ReadByte` cart walk and the `entry+…`
  directory struct with a plain pointer + length over a caller-supplied RAM
  buffer. The IRQ becomes "fetch next byte from RAM pointer, advance, stop at
  end" — simpler and with no cart dependency. Install it via `set_irq` on timer
  3 rather than writing `irq_vecs` directly.
- **Samples are short, resident data.** The game provides the sample as a normal
  linked/`.incbin`ed byte array in RAM (or a decoded buffer); `PlayPCMSample`
  takes its address+length. This keeps the "small footprint" spirit for the
  short one-shots PCM is actually good for on the Lynx (drum hits, stingers) and
  drops the streaming machinery entirely.
- **`HandyMusic_Disable_Samples`** is retained as a runtime mute for the PCM
  path, but PCM is a supported, linkable feature — not gated off.

**`LoadPlayBGM`** depends on `LoadFile` + `FileNum_MusicBase` (cart file load).
Replace it with a "music data already resident in the reserved region, just
`PlayMusic`" flow — the game arranges the `.mus` data in the `HANDYMUSIC_DATA`
region (§4.3) and calls `HandyMusic_PlayMusic` directly. The cart-loading variant
is dropped along with the streaming path.

### 4.5 C-callable surface (`include/lynx/handymusic.h`)

Wrap the asm entry points as C functions where the calling convention allows.
The number-in-`A` calls (`PlaySFX`, `PlayMusic`) map to a single `unsigned char`
argument. `PlayPCMSample` changes shape from the upstream "sample number in A" to
a RAM-sourced call taking a buffer pointer and length (§4.4), e.g.
`handymusic_play_pcm(const unsigned char *buf, unsigned int len)`.

**Decision: the library installs its own VBlank hook.** `HandyMusic_Main` must
run once per VBlank; rather than requiring the game to call it from its own
handler, the library installs its own hook at `HandyMusic_Init` time (and removes
it on a teardown call). This keeps the game code minimal — init, then just
`play`/`stop`/`pause` — and matches how the driver was always meant to be tied to
the 60 Hz tick. Document the hook so a game with its own IRQ scheme knows what is
installed. Header carries the doc comments per the repo's "docs track code" rule.

## 5. Data flow, end to end

```
  .txt scripts (instruments, sfx, 4 tracks)      SASS-format text
        │  make rule invoking bin/hmcc
        ▼
  bin/hmcc $base out.mus inst.txt trk1..4   →   out.mus  (+ .sfx / .equ)
        │  ca65 .incbin at the reserved data region
        ▼
  game.o  ── ld65 ──►  linked with lynx-handymusic.lib  ──►  game.lnx
                                    ▲
                       ca65-ported driver + C header
```

A `make`-time rule compiles the demo scripts the same way sprites are packed
today (the `sprpck`/`sp65` precedent), so the example rebuilds from text sources.
PCM samples are ordinary resident byte arrays linked into RAM and handed to
`PlayPCMSample` by address+length (§4.4) — no separate stream file.

## 6. Documentation & build wiring (per `CLAUDE.md` "docs track code")

- **`doc/hmcc.html` — DONE.** The `hmcc` tool got its own page under the Tools
  section (matching the `abccc`/`abcrom`/`lnx` tool pages) rather than being a
  sub-section of a single driver reference: the two compile modes, the
  instrument/SFX and music script languages with full command tables, tuning and
  note frequencies, the output files and absolute base-address model, and the CLI
  and build flow. Three theme-aware SVG diagrams (build data-flow, instrument
  script structure, `.mus` binary layout) per `DOC_SVG_STYLE_DESIGN.md`. It is
  registered in the shared nav (`TOPBAR_HTML` in `doc.js`, alphabetically between
  `da65` and `ld65`) and the `index.html` Tools grid, and the doc search index was
  regenerated (`make -C doc doc-search-index`).
- **`doc/handymusic.html` — PLANNED (library track).** A driver Reference page
  distilled from the manual (memory usage, channel structure, subroutine
  reference for the ca65 driver and `handymusic.h` API) is still to be written
  when §4 lands. Follow `DOC_SVG_STYLE_DESIGN.md` for its diagrams (channel-update
  flow, envelope byte-code layout). The script-format material already lives on
  `doc/hmcc.html` and need not be duplicated.
- **`doc/licenses.html` §4.5 — DONE.** Records HandyMusic's origin, Osman
  Celimli's authorship, the verbatim "free to use and modify" grant, and the
  derived `tools/hmcc/` files (see §7).
- Add the future `HandyMusic_*`/`handymusic.h` entries to the function reference,
  re-sorted alphabetically per the funcref convention, when the library lands.
- Design doc committed as `design/LYNX_HANDYMUSIC_DESIGN.md` (this file).
- Header/`asminc` doc comments on every exported symbol (library track).

## 7. Licensing

HandyMusic's grant ("completely free to use and modify") is permissive but is
**not** a standard SPDX licence, so it gets its own component bucket rather than
being folded into the MPL-2.0 fork (consistent with the per-component policy in
`LYNX_LICENSE_POLICY_DESIGN.md`):

- The ca65 port is a **derived work** of Osman Celimli's driver — it must retain
  his authorship/credit and the original grant text, and is best tagged as such
  (author's grant, not MPL), the same way derived tgi/ser files were reverted to
  their original notices rather than MPL.
- `hmcc` is a **derived port** of Celimli's `HMCC` — same treatment: credit +
  original grant, first-party build glue authored by lynxcc.
- Add a `doc/licenses.html` entry recording HandyMusic's origin, author, the
  verbatim grant, and which files are derived. Regenerate the search index.
- Unlike the extern tools, there is no upstream repo to pin; the provenance note
  records the package name/version (`HandyMusic_v1-40cx+`) instead.

**Decision: accept the grant verbatim.** The "completely free to use and modify"
wording is taken as-is; no upstream SPDX licence will be sought as a precondition
for shipping. The `doc/licenses.html` entry reproduces the grant text verbatim
and attributes the original work to Osman Celimli, and the derived files carry
that same author's-grant notice (not MPL).

## 8. Phasing & verification

Suggested order, each phase independently verifiable and each ending in a full
sandbox rebuild (toolchain + lib + examples) per project convention:

1. **`hmcc` portable port. — DONE.** `bin/hmcc` builds from `PROGS`; the tool is
   documented at `doc/hmcc.html` and licensed per §7 / `licenses.html` §4.5.
   Regression-checking its output against the shipped `SASS/DemoMusic`/`DemoSFX`
   reference binaries byte-for-byte (to prove the port is faithful) remains a
   worthwhile follow-up once the demo scripts are brought into the tree.
2. **Driver ca65 port, music+SFX. — PENDING.** Bring up a minimal example that plays the
   demo song and a couple of SFX; verify on the headless GearLynx emulator that
   Mikey audio registers actually change and the output is *audible*, not just
   boot-and-golden (per the "audio must be hearable" rule).
3. **Data region + cfg + C header + example** wired into the examples build
   (fixed-region model, §3.2/§4.3).
4. **RAM-sourced PCM (§4.4).** Reband `PlayPCMSample`/`PCMSample_IRQ` to a RAM
   buffer, install the timer-3 IRQ via `set_irq`, and add a short one-shot sample
   to the example; verify the PCM one-shot is audible and channel 0 hands back to
   the music engine afterwards.
5. **Docs** (`handymusic.html`, funcref, nav, search index).

The §4.2 ZP-vs-absolute addressing audit is the highest-risk item and warrants a
dedicated verification pass (ideally a subagent diffing addressing modes against
the lyxass original) before phase 2 is considered done.

## 9. Resolved decisions

1. **Library packaging** — its own `lib/lynx-handymusic.lib`, opt-in and parallel
   to `lynx-audio`. It is a large, mutually-exclusive alternative engine, so it
   gets a separate library rather than folding into the existing audio lib.
2. **PCM** — supported, but **RAM-sourced, not cart-streamed** (§4.4). Long PCM
   streams are a non-goal on the Lynx; short resident one-shots played from a RAM
   buffer via the existing timer-3 IRQ are the supported model.
3. **`HandyMusic_Main` wiring** — the **library installs its own VBlank hook**
   (§4.5); the game does not call `Main` itself.
4. **Licence** — the "free to use and modify" grant is **accepted verbatim**
   (§7); no upstream SPDX licence is sought as a precondition.
5. **`hmcc` output** — **fixed-region model only** (§3.2); relocatable object
   output is out of scope.

First deliverable: `hmcc` tool + music/SFX library + RAM PCM + one example + doc
page, per the phasing in §8. **Delivered so far: the `hmcc` tool and its
`doc/hmcc.html` page (phase 1);** the library, RAM PCM and example remain
outstanding.
