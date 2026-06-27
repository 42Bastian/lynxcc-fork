<!--
SPDX-License-Identifier: CC-BY-4.0
Lynx Game Development SDK documentation, (c) 2026 the lynxcc authors,
licensed under Creative Commons Attribution 4.0 International.
See doc/licenses.html.
-->

# Lynx sound engine (`snd`) — design

Design for the **lynxcc** sound engine: a single, compiled-stream music player for
Mikey's four audio channels, plus a small set of direct-register helpers for sound
effects. It is the result of merging the two earlier sound modules — the binary
"rsound" player (`lynx_snd*`) and the ABC-notation player (`abcmusic`) — into one
engine.

- Runtime origin: binary event-stream player by Karri Kaksonen and Bjoern Spruck
  (11.12.2012). The `_IF/_ELSE/_ENDIF` structured-assembly macros are Spruck's
  (from Chipper).
- Authoring origin: the ABC-notation language by Karri Kaksonen (02.02.2019),
  retained as the **source** language compiled offline by `abccc`.
- Target: cc65 / ca65, Atari Lynx (Mikey audio hardware).

## Summary of the merge

The two modules are unified by keeping the stronger half of each:

- **Runtime = the binary player.** Tunes play from a compiled byte-coded event
  stream interpreted under a dedicated sound-timer interrupt — no ASCII parsing on
  target. This keeps the looping volume/frequency/wave envelopes, the 64-instrument
  bank, pattern call/return, counted loops, stereo/attenuation, and multi-channel
  sync.
- **Authoring = ABC text, compiled offline.** Human-readable ABC tune strings stay
  as the source format, but are turned into the binary stream by an external tool
  (`abccc`) at build time. The on-target ABC parser is **removed**.
- **Timbre gap closed.** The integrate waveform and the 9-bit LFSR tap that the old
  binary player could not program are added as opt-in stream opcodes, so ABC's `I`
  and `X` commands now round-trip.
- **SFX helpers kept.** `abcmusic`'s direct register pokes survive under the
  `mikey_snd_*` prefix for one-call sound effects that bypass the stream.
- **Backward compatible.** Every existing binary stream still plays byte-identically
  (see §6.4); new features are opt-in opcodes in free opcode space.

Naming: the engine's public functions move from the old `lynx_snd_*` prefix to a
plain `snd_*` prefix (we already target the Lynx exclusively, so the `lynx_` tag is
redundant); see §8 for the full rename map.

---

## 1. Public API

### 1.1 Engine (`snd_*`)

Declared in `lynx/lynx.h`:

```c
void           snd_init (void);
/* Initialise the engine: program the sound timer + install the interrupt. */

void __fastcall__ snd_play (unsigned char channel, const unsigned char *stream);
/* Start a compiled tune stream on a channel (0-3); a busy default channel
   yields to a free one. */

void           snd_stop (void);
/* Stop all channels. */

void __fastcall__ snd_stop_channel (unsigned char channel);
/* Stop one channel. */

void           snd_pause (void);
void           snd_continue (void);
/* Suspend / resume every channel (e.g. game pause). */

unsigned char  snd_active (void);
/* Non-zero while any channel is still playing (bitmask of channels). */
```

Usage: call `snd_init()` once at startup, then `snd_play(chan, stream)` with a
stream produced by `abccc` (§7). Playback then advances on its own under the sound
interrupt; the program is free between calls.

### 1.2 Direct hardware helpers (`mikey_snd_*`)

These poke a single Mikey channel register and are independent of the stream
engine — useful for sound effects or hand-built instruments. They are a thin
hardware layer, hence the `mikey_` prefix rather than `snd_`. The
channel-to-register math is `reg_base = base + (chan << 3)` (each Mikey channel
occupies 8 bytes):

```c
void __fastcall__ mikey_snd_octave    (unsigned char chan, unsigned char val); // 0..6
void __fastcall__ mikey_snd_pitch     (unsigned char chan, unsigned char val); // 0..255 (BACKUP)
void __fastcall__ mikey_snd_taps      (unsigned char chan, unsigned int  val); // 0..511 (LFSR taps)
void __fastcall__ mikey_snd_integrate (unsigned char chan, unsigned char val); // 0..1
void __fastcall__ mikey_snd_volume    (unsigned char chan, unsigned char val); // 0..127
```

| Call | Register | Effect |
|------|----------|--------|
| `mikey_snd_volume`    | `$FD20+8c` VOLUME  | signed output volume |
| `mikey_snd_pitch`     | `$FD24+8c` BACKUP  | timer reload = pitch within the octave |
| `mikey_snd_octave`    | `$FD25+8c` CONTROL | clock-divider bits 0-2 = octave band, **and** the timer-enable bits ($18 = reload+count) so the channel's poly counter is actually clocked (the same $18 the stream engine ORs in) |
| `mikey_snd_integrate` | `$FD25+8c` CONTROL | bit 5 = integrate (waveform shape) |
| `mikey_snd_taps`      | `$FD21+8c` FEEDBACK + CONTROL bit 7 | LFSR feedback tap selection: the low 8 taps in FEEDBACK, the 9th in CONTROL bit 7 (timbre/noise). The CTLB `$FD27` nibble holds shift-register state, not tap selection, so the helper leaves it alone |

These are the same five routines `abcmusic` exposed (`abcoctave`/`abcpitch`/
`abctaps`/`abcintegrate`/`abcvolume`), renamed. `mikey_snd_octave` additionally
sets the channel-enable bits so a hand-built voice actually sounds: a channel
with the enable bits clear, or with no feedback taps, only emits a DC level
(silence). A minimal audible voice is therefore `taps` + `octave` + `pitch` +
`volume`.

---

## 2. Synthesis model

Each of Mikey's four channels is an 8-byte register block driven from a per-channel
software state. The engine provides:

- **Pitch** from a 128-entry chromatic table (`SndReload` + `SndPrescaler`): a
  single 0-127 index selects the timer reload (semitone) and clock divider
  (octave) across ~8 octaves.
- **Instruments**: up to 64 predefined instruments, each a (shift-lo, shift-hi,
  feedback, volume, max-volume) set programmed by the `SetInstr` opcode.
- **Three independent, looping envelopes per channel** — volume, frequency, and
  waveform/shift-register — each a list of `[count, increment]` segments with a
  loop point, updated every interrupt. These give tremolo, vibrato/pitch slides,
  and timbre sweeps.
- **Integrate / taps**: the CONTROL integrate bit and the 9-bit LFSR feedback,
  programmable per channel (new in this engine, see §6.4).

### 2.1 AHD shorthand (compiler-side only)

`abcmusic`'s simple attack/hold/decay (AHD) per-note envelope is preserved as a
**source convenience**, not a second runtime engine: `abccc` lowers a tune's
`R`/`H`/`K`/`V` settings into an ordinary three-segment **volume envelope**
(attack: `+R` until target; hold: `0` for `H`; decay: `-K`). The runtime therefore
needs no AHD-specific code — an AHD instrument is just a volume envelope like any
other. Authors who want richer shapes can define full envelopes directly.

---

## 3. Timing

Playback is driven by a **dedicated Mikey timer**, not the vertical blank. `snd_init`
programs timer-based interrupt (`STIMCTLA`/`STIMBKUP`, ~240 Hz by default) and a
reentrancy semaphore guards the handler. This gives finer tempo and envelope
resolution than a 60 Hz VBL tick, at the cost of one Mikey timer. The player IRQ
frequency can be changed mid-stream by the `PlayerFreq` opcode. Durations are
counted in interrupt frames; tempo is a compile-time multiplier folded into
durations by `abccc`.

(The old `abcmusic` engine ran on the VBL interrupt; that path is retired with it.)

---

## 4. Source language: ABC tune syntax

ABC text is the **authoring** format consumed by `abccc` (§7); it is not parsed on
target. A tune is a NUL-terminated ASCII string.

### 4.1 Notes and accidentals

| Token | Meaning |
|-------|---------|
| `C D E F G A B` | natural notes, middle octave |
| `c d e f g a b` | same notes one octave higher (lowercase) |
| `^C ^F ...`     | sharp (raise one semitone) |
| `_D _E ...`     | flat (lower one semitone) |
| `C,`            | one octave **lower** (trailing comma) |
| `c'`            | one octave **higher** (trailing apostrophe) |
| `z`             | rest / silence |

Notes and rests accept an optional **duration** number: `C` = 1 unit, `C2`, `C3`,
…, and `z`, `z2`. Real duration in frames = `number × tempo` (see `T`).

### 4.2 Inline commands (each takes a numeric argument)

| Cmd | Name | Effect | Default |
|-----|------|--------|---------|
| `V` | Volume        | channel target volume (0..127) | 127 |
| `R` | Ramp / attack | per-frame attack increment | 4 |
| `H` | Hold          | frames of full-volume sustain | 4 |
| `K` | Kill / decay  | per-frame decay decrement | 4 |
| `T` | Tempo         | frames per duration unit | 6 |
| `O` | Octave base   | added to each note's octave | 0 |
| `I` | Integrate     | waveform: 0 = square, 1 = integrate/triangle | — |
| `X` | XOR taps      | LFSR feedback taps (timbre / noise colour) | — |

`V/R/H/K` feed the AHD lowering of §2.1; `I`/`X` compile to the integrate/taps
opcodes of §6.4; `T`/`O` are resolved entirely at compile time (folded into
durations and pitch indices).

### 4.3 Structure

| Token | Meaning |
|-------|---------|
| `\|:`  | start repeat section; optional count (`\|:3`); count 0 ⇒ infinite |
| `:`   | end repeat section; loops back to the matching `\|:` |
| `' '` | spaces ignored (free formatting) |
| `\0`  | end of tune |

`abccc` compiles repeats to the engine's `Loop`/`Do` opcodes; nested repeats and
shared phrases map to `CallPattern`/`RetToSong`.

---

## 5. Compiled stream — overview

`snd_play` is handed a pointer to a byte-coded event stream. The interpreter reads a
lead byte each step:

- **bit 7 clear** → a **note**. Legacy form is 2 bytes `[pitch][duration]`; the
  compact form (1 byte `[pitch]`, duration from the current default) is enabled by
  the `MODE` opcode (§6.4).
- **bit 7 set** → an **opcode** (`$80 + index`), operands follow.
- **`$00`** → end of stream; the channel goes idle.

Each handler returns how many bytes to advance and whether to keep processing in the
same frame, so several config events can run on one note boundary while notes
themselves wait out their duration.

---

## 6. Stream format reference

### 6.1 Note encoding

| Form | Bytes | When |
|------|-------|------|
| legacy note | 2 | `[pitch 0-127][duration]` — always valid |
| compact note | 1 | `[pitch 0-127]` — duration from `SETDUR`, only while `MODE` compact bit set |

`pitch` indexes the chromatic `SndReload`/`SndPrescaler` tables. `abccc` maps each
ABC note (letter + accidental + octave) to the matching table index, so the
engine's table is the canonical tuning.

### 6.2 Opcode map

Existing opcodes (unchanged, `$80`–`$92`):

| Op | Name | Bytes | Purpose |
|----|------|-------|---------|
| `$80` | Loop              | 2 | begin counted loop `[op][count]` |
| `$81` | Do                | 1 | loop back if count remains |
| `$82` | Pause             | 2 | silence for N frames |
| `$83` | NoteOff           | 1 | release current note |
| `$84` | SetInstr          | 6 | `[op][shiftLo][shiftHi][feedback][vol][maxVol]` |
| `$85` | NewNote2          | 4 | explicit `[op][reload][prescale][length]` note |
| `$86` | CallPattern       | 3 | call shared phrase `[op][addrLo][addrHi]` |
| `$87` | RetToSong         | 1 | return from phrase |
| `$88` | DefEnvVol         | 4 | define volume envelope `[op][env#][ptrLo][ptrHi]` |
| `$89` | SetEnvVol         | 2 | select volume envelope |
| `$8A` | DefEnvFrq         | 4 | define frequency envelope |
| `$8B` | SetEnvFrq         | 2 | select frequency envelope |
| `$8C` | DefEnvWave        | 4 | define waveform envelope |
| `$8D` | SetEnvWave        | 2 | select waveform envelope |
| `$8E` | SetStereo         | 2 | write `MSTEREO` |
| `$8F` | SetAttenuationOn  | 2 | write attenuation-enable (`$FD44`) |
| `$90` | SetChnAttenuation | 2 | per-channel attenuation (`$FD40+c`) |
| `$91` | PlayerFreq        | 3 | retune the player IRQ (`STIMCTLA`/`STIMBKUP`) |
| `$92` | ReturnAll         | 1 | multi-channel sync marker |

New opcodes (this engine, free `$93+` space):

| Op | Name | Bytes | Purpose |
|----|------|-------|---------|
| `$93` | Mode      | 2 | `[op][flags]` — per-channel parser modes (bit 0 = compact notes) |
| `$94` | SetDur    | 2 | `[op][dur]` — default note duration for compact notes |
| `$95` | SetInteg  | 2 | `[op][value]` — per-channel integrate (and bit-7 tap) bits |

`$00` ends the stream. `$96`–`$FF` remain free for future opt-in additions.

### 6.3 Per-note size

| format | bytes/note | note |
|--------|-----------|------|
| legacy note | 2 | `[pitch][duration]` |
| `NewNote2` (explicit reload) | 4 | for pitches off the chromatic table |
| **compact note** (this engine) | **1** | `[pitch]`, duration from `SETDUR` |

Default-duration compaction takes a typical melody's note stream down roughly a
third (≈ `f·N − 2D` bytes saved for N notes, fraction `f` at the prevailing
duration, `D` duration changes); see §6.4.

### 6.4 Backward-compatible extensions

The stream format already ships, so every addition is opt-in and the legacy decode
is the default. The enabler is free opcode space (`$93+`); no existing stream
contains those bytes, so an upgraded player replays old streams bit-identically.

**Compatibility rule:**

1. Never reinterpret an existing byte — the bit7-clear note stays a 2-byte
   `[pitch][duration]` note by default.
2. Express each feature as a new `$93+` opcode and grow the dispatch table.
3. Default all new per-channel state to 0, so legacy behaviour is the power-on
   behaviour; clear it in `snd_init`, reset it in `snd_play`.
4. Bump the stream/player format-version byte (§9.3) when opcodes are added.

**Default-duration notes — mode switch.** A 1-byte note wants the bit7-clear space,
which is already the first byte of a 2-byte note, so the compact form is gated:
`Mode` (`$93`) bit 0 makes subsequent bit7-clear bytes 1-byte notes that take their
duration from `SetDur` (`$94`). Old streams emit neither opcode, stay in 2-byte
mode, and decode unchanged; new streams emit one `Mode 1` (+ `SetDur`) near the top
and pay 1 byte per common note.

**Integrate / tap — opt-in state.** The legacy player writes CONTROL as
`prescale | $18` and never sets bit 5 (integrate) or bit 7 (upper feedback tap), so
old streams always play the square wave. `SetInteg` (`$95`) stores a per-channel bit
that `SndSetValues` simply `ora`s into `$FD25` alongside the existing `| $18`.
Because the byte defaults to 0, existing streams write a bit-identical control value
(square preserved); new streams switch a channel to the integrated wave (ABC `I1`)
or set the 9th tap (high ABC `X`). This closes the timbre gap that previously
blocked a faithful ABC round-trip.

---

## 7. `abccc` — the tune compiler

A **separate, self-contained command-line tool**, maintained **outside** the cc65 /
**lynxcc** build. It turns ABC tune text into a compiled stream before the normal
build runs. Keeping it external keeps the target tree free of the stateful
tokenizer, lets it be written in a convenient host language (C or Python), and makes
its output easy to unit-test against the stream spec.

Responsibilities:

- Parse ABC text (§4) — notes, accidentals, octaves, durations, inline commands,
  repeats.
- Resolve each note to a chromatic **table index** and emit a compact 1-byte note
  under `Mode`/`SetDur`; fall back to `NewNote2` only for off-table pitches.
- Fold `T` (tempo) and `O` (octave base) into durations and pitch indices.
- Lower `V/R/H/K` (AHD) into a volume envelope (`DefEnvVol`/`SetEnvVol`, §2.1).
- Emit `SetInteg` for `I` and program feedback/taps for `X`.
- Compile `|: … :|` to `Loop`/`Do`, and shared phrases to `CallPattern`/`RetToSong`.
- Validate ranges and repeat/phrase structure; warn on overflow.

**Tool shape (suggested):**

```
abccc  [options]  input.abc ...            # "abc cross-compiler"
  -o FILE        output file
  -f s|h|bin     output format: ca65 asm (.s), C header (.h), or raw binary
  -l LABEL       symbol/label name for the emitted stream
  --no-compact   disable default-duration compaction (emit 2-byte notes)
  --validate     range/structure checks only, no output
```

**Contract:** the emitted bytes are the shared spec between `abccc` and the player
(§5/§6). The two are versioned together via the format-version byte (§9.3).

### 7.1 Worked example

Author `assets/theme.abc` (plain ABC text):

```
T36 V100 R8 H12 K4
|: CDEF GABc :|
z4
```

Compile as a pre-build step:

```
abccc -f s -l theme_music -o assets/theme.s assets/theme.abc
```

`abccc` emits a ca65 source (illustrative bytes):

```asm
; Generated by abccc - do not edit by hand. Source: theme.abc
    .export _theme_music
    .segment "RODATA"
_theme_music:
    .byte $93, $01        ; Mode: compact notes
    .byte $84, ...        ; SetInstr (AHD V100/R8/H12/K4 lowered to a vol envelope)
    .byte $89, $01        ; SetEnvVol 1
    .byte $80, $00        ; Loop (count 0 = repeat)
    .byte $94, 36         ; SetDur 36 frames (1 unit x T36) — emitted lazily, just
                          ;   before the first note whose duration it governs
    .byte   $0C           ;   C  (compact note: pitch index, default dur)
    .byte   $0E           ;   D
    ; ... E F G A B c ...
    .byte $81             ; Do
    .byte $82, 144        ; Pause 144 frames  (z4 x T36)
    .byte $00             ; END
```

Game C code uses the engine directly — no new entry point, `snd_play` takes the
compiled stream:

```c
#include <lynx/lynx.h>
#include "theme.h"          /* extern const unsigned char theme_music[]; */

void main(void) {
    snd_init();
    snd_play(0, theme_music);
    for (;;) { /* IRQ advances the stream on its own */ }
}
```

Makefile wiring (`abccc` is just another rule):

```makefile
ABCCC := abccc

assets/%.s: assets/%.abc
	$(ABCCC) -f s -l $(notdir $*)_music -o $@ $<

OBJS += main.o assets/theme.o

game.lnx: $(OBJS)
	cl65 -o $@ $(OBJS)
```

A `-f bin` variant emits a raw stream for `.incbin` or for loading into a RAM
buffer; the on-cart byte layout is identical either way.

---

## 8. Migration from `lynx_snd*` / `abcmusic`

### 8.1 Engine rename (`lynx_snd_*` → `snd_*`)

| Old | New |
|-----|-----|
| `lynx_snd_init`         | `snd_init` |
| `lynx_snd_play`         | `snd_play` |
| `lynx_snd_stop`         | `snd_stop` |
| `lynx_snd_stop_channel` | `snd_stop_channel` |
| `lynx_snd_pause`        | `snd_pause` |
| `lynx_snd_continue`     | `snd_continue` |
| `lynx_snd_active`       | `snd_active` |

The exported assembly labels (`_lynx_snd_*`) and the `.interruptor` change to match.
`lynx/lynx.h` declares the new names. No call-site behaviour changes.

### 8.2 SFX helpers (`abc*` → `mikey_snd_*`)

| Old (`abcmusic`) | New |
|------------------|-----|
| `abcoctave`    | `mikey_snd_octave` |
| `abcpitch`     | `mikey_snd_pitch` |
| `abctaps`      | `mikey_snd_taps` |
| `abcintegrate` | `mikey_snd_integrate` |
| `abcvolume`    | `mikey_snd_volume` |

### 8.3 Removed

- `abcmusic`'s on-target ABC parser and its `abcplay`/`abcstop`/`abcactive` API
  (authoring is now compiled-only; `snd_play`/`snd_stop`/`snd_active` replace them).
- The `abcmusic` VBL interrupt path (the engine uses its own timer, §3).
- `abcmusic`'s `_delays` one-octave table (pitch now uses the engine's chromatic
  table, §6.1).

### 8.4 Work items

- `libraries/audio/lynx-snd.s`: rename exports; add `Mode`/`SetDur`/`SetInteg`
  opcodes + the per-channel mode/default-duration/integrate state; extend
  `SndCmdsLo/Hi`; `ora` the integrate bit in `SndSetValues`.
- New `libraries/audio/mikey-snd.s` (+ header) carrying the five direct helpers.
- Delete `abcmusic.s` / `ABCMusic.h`.
- New external tool `abccc`.
- Docs in sync (project rule): `include/lynx/lynx.h`, `doc/funcref.html`,
  `doc/lynx.html` §8.4, this design doc.
- New dedicated `doc/sound.html` page (§10) — engine guide, `abccc` composing
  tutorial, worked examples, SVG diagrams — plus its index card, nav entry on every
  doc page, and Makefile wiring.
- A worked example under `examples/mikey/` and a GearLynx golden.

### 8.5 Compatibility

Existing **binary** streams keep playing unchanged (§6.4). Source-level
compatibility for `abcmusic` C callers is **not** preserved — those call sites move
to `snd_play` with a compiled stream, or to `mikey_snd_*` for direct SFX.

---

## 9. `abcrom` — tune test-ROM utility

A separate command-line utility (like `abccc`, **outside** the cc65 codebase) that
turns a tune into a runnable `.lnx` for auditioning in an emulator.

### 9.1 Approach: template patcher (zero toolchain at test time)

`abcrom` does **not** invoke cc65. It ships a pre-assembled **template `.lnx`** that
already contains the `snd` engine plus a tiny boot harness, with a fixed region
reserved for the tune stream. To make a test ROM it (1) compiles the tune to a stream
(via `abccc`, or accepts a `.bin`), (2) **patches those bytes into the reserved
region** of a copy of the template, (3) writes the result. No compile, no link — so a
tune reaches a playable `.lnx` in milliseconds with no cc65 installed. Because the
region is fixed-size, patching never changes the file length, so the `.lnx` header
stays valid.

### 9.2 The template ROM

Built **once** with the normal toolchain (a `make` target in the player project, not
part of `abcrom`). It contains the engine object, a boot harness that on reset calls
`snd_init()` then `snd_play()` for each non-empty region and loops, and one or more
**reserved tune regions** in `RODATA` fronted by a magic header so the patcher can
locate them without hard-coded offsets. The player replays a stream until the `END`
opcode (`$00`), so an empty region is a single `$00` byte and that channel stays
silent.

### 9.3 Reserved-region layout

```
offset  size  field
  0      4     magic  "ABCR"
  4      1     format version  (must match abccc / player stream version)
  5      1     channel index   (0..3)
  6      2     capacity (bytes of payload that follow)   LE
  8      2     used     (bytes written by abcrom)        LE
 10      N     payload (event stream, must end with END within `capacity`)
```

`abcrom` finds a region by its `"ABCR"` magic + channel index, checks the **format
version** matches (refuses a stale template otherwise), checks the stream + `END`
fits `capacity`, then writes `used` and the payload.

### 9.4 CLI

```
abcrom  [opts]  tune.abc
  -o FILE          output .lnx (default: <tune>.lnx)
  -t FILE          template .lnx (default: bundled template)
  --channels MAP   place tunes on channels, e.g. 0:bass.abc,1:lead.abc
  --name STR       cart-name field in the .lnx header
  --run [EMU]      launch emulator after patching
  --bin            input is already a compiled stream, skip abccc
```

### 9.5 Tradeoffs

- **Win:** no toolchain at test time; instant turnaround; runs anywhere.
- **Main fragility — template/version coupling.** The template embeds a specific
  engine build and stream-format version; whenever the engine or stream format
  changes, the template must be **rebuilt once** with the toolchain. The `format
  version` byte makes a mismatch a hard error rather than silent corruption.
- **Fixed capacity.** Tunes larger than a region's `capacity` are rejected; bump the
  reserved size and rebuild the template.
- **Emulator target only.** The patched `.lnx` carries the standard 64-byte header
  and runs in Handy / Mednafen / Felix. Real-hardware carts need the encrypted
  micro-loader — a separate path, out of scope for quick testing.

---

## 10. Documentation: the dedicated Sound page

A standalone `doc/sound.html` page must be authored as part of this work. Today
sound is only a short subsection (`doc/lynx.html` §8.4) plus the function-reference
entries; the merged engine needs a full guide that both explains how playback works
and teaches how to compose tunes with `abccc`. The page follows the rest of the SDK
documentation: light/dark theme, the shared nav and Licenses links, a card on
`doc/index.html`, and Makefile wiring like the other pages.

### 10.1 Content outline

1. **Overview** — Mikey's four audio channels, the compiled-stream model, the
   timer-driven IRQ, and where the `mikey_snd_*` direct helpers fit.
2. **How playback works** — `snd_play` hands the engine a stream; the IRQ walks it
   (lead-byte rule: bit7 clear = note, bit7 set = opcode, `$00` = end), advances
   durations in frames, runs the envelopes, and writes the channel registers.
3. **Composing with `abccc`** — the ABC source language (§4) end to end, the build
   flow (`.abc` → `abccc` → `.s` → link), the Makefile rule, and how `T/O/V/R/H/K/I/X`
   map onto the compiled stream (compact notes, AHD-as-volume-envelope, integrate).
4. **Worked examples** (§10.2) — several, increasing in complexity.
5. **Auditioning with `abcrom`** — the zero-toolchain test-ROM loop (§9).
6. **Reference** — links to `funcref.html` (`snd_*`, `mikey_snd_*`) and this design.

### 10.2 Worked examples (multiple, increasing complexity)

Each example shows the `.abc` source, the `abccc` invocation, the resulting
annotated bytes, and the C that plays it:

1. **A single beep** via `mikey_snd_*` only — no stream, to show the direct layer.
2. **A one-voice scale/melody** — compact notes + an AHD instrument (`V/R/H/K`).
3. **A looped two-voice tune** — `|: … :|` repeats plus a second channel on
   `snd_play(1, …)`.
4. **An expressive instrument** — a frequency envelope (vibrato) and a volume
   envelope (tremolo) on a held note.
5. **A sound effect** — `taps`/`integrate` for noise/percussion timbre.

### 10.3 SVG diagrams

Diagrams follow `design/DOC_SVG_STYLE_DESIGN.md` (viewBox 720, theme CSS variables
only, mono/sans font conventions, `<figure>`/`<figcaption>` wrapper, legible in both
light and dark themes), matching the graphics/font/sprite pages so the Sound page
reads consistently. The page should include at least:

- **Signal chain** — `tune.abc` → `abccc` → event stream in `RODATA` → `snd` IRQ →
  Mikey channel registers → speaker.
- **Channel register map** — the 8-byte Mikey channel block (`$FD20`–`$FD27`):
  VOLUME, FEEDBACK, CONTROL, BACKUP, shift registers, with the `+8c` per-channel
  stride called out.
- **Event-stream anatomy** — a byte run split into lead bytes, showing a compact
  1-byte note, a legacy 2-byte note, and a `Mode`/`SetDur`/`SetInstr` header.
- **Opcode dispatch** — lead byte → bit-7 test → note path vs the `SndCmds`
  jump table.
- **AHD envelope** — the attack/hold/decay shape annotated with `R`/`H`/`K`/`V`
  (the clean SVG counterpart of §2.1).
- **Looping envelope** — a `[count, increment]` segment list with the loop-back
  arrow, illustrating vibrato/tremolo.

---

## 11. Quirks and limitations

1. **Durations are byte-wide.** A note/pause caps at 255 frames; longer notes are
   split into tied notes by `abccc`.
2. **64 instruments / envelope slots.** The instrument and envelope tables are
   fixed-size; `abccc` should error if a tune exceeds them.
3. **One Mikey timer consumed** for the player IRQ (§3).
4. **Compact-note mode is per channel.** A channel must (re)assert `Mode` after a
   context that could have reset it; `abccc` emits it at stream start.
5. **`abcrom` template must track the format version** (§9.5).

---

## 12. Quick reference

```
init:      snd_init();
play:      snd_play(0, theme_music);     // theme_music = abccc output
stop one:  snd_stop_channel(c);
stop all:  snd_stop();
pause:     snd_pause();  / resume: snd_continue();
busy?:     if (snd_active()) ...
SFX:       mikey_snd_volume(c, v); mikey_snd_pitch(c, p); ...

author:    edit tune.abc  ->  abccc -f s -l name_music -o name.s tune.abc
audition:  abcrom --run tune.abc
```
