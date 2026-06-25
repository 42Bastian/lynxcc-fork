<!--
SPDX-License-Identifier: CC-BY-4.0
Lynx Game Development SDK documentation, (c) 2026 the lynxcc authors,
licensed under Creative Commons Attribution 4.0 International.
See doc/licenses.html.
-->

# ABCMusic for the Atari Lynx

Reference documentation for `abcmusic.s` / `ABCMusic.h`, plus a design for moving
tune parsing from run time to compile time.

- Origin: ABC-notation player by Karri Kaksonen (02.02.2019). The `_IF/_ELSE/_ENDIF`
  structured-assembly macro package is by Bjoern Spruck (from Chipper).
- Target: cc65 / ca65, Atari Lynx (Mikey audio hardware).

---

## 1. What it does

ABCMusic plays up to four simultaneous voices from short, human-readable
ABC-style note strings. Each voice maps to one Lynx Mikey audio channel.

The key idea is that the **Lynx hardware sustains the tone on its own** — once a
channel's pitch (BACKUP/timer) and control bits are programmed, it keeps
oscillating with no CPU involvement. The CPU only wakes up on the vertical-blank
(VBL) interrupt to:

1. count down the current note's duration, and
2. shape the per-note volume envelope (attack / hold / decay).

When a note's duration expires, the VBL handler parses the next token(s) from the
tune string and programs the hardware for the next note.

---

## 2. Public API (`ABCMusic.h`)

```c
void  __cdecl__   abcstop(void);                         // init + silence all
void  __cdecl__   abcplay(unsigned char channel, char *tune);
unsigned char     abcactive[4];                          // read-only: voice busy flags

// Low-level direct hardware control (bypass the parser)
void  __fastcall__ abcoctave   (unsigned char chan, unsigned char val); // 0..6
void  __fastcall__ abcpitch    (unsigned char chan, unsigned char val); // 0..255 (BACKUP)
void  __fastcall__ abctaps     (unsigned char chan, unsigned int  val); // 0..511 (LFSR taps)
void  __fastcall__ abcintegrate(unsigned char chan, unsigned char val); // 0..1
void  __fastcall__ abcvolume   (unsigned char chan, unsigned char val); // 0..127
```

Usage rules:

- **Call `abcstop()` once at init** before any other call. It clears state,
  installs the "empty tune", and sets stereo panning (`MSTEREO`/`$FD50` = both
  sides). The library also self-initialises lazily (`init_done` flag) on the first
  `abcplay`, but explicit init is the documented contract.
- `abcplay(chan, tune)` starts `tune` on channel `chan` (0–3). Starting a new tune
  on a busy channel simply replaces what was playing.
- `abcactive[chan]` is non-zero while a tune is playing; it drops to 0 when the
  tune ends (null terminator reached) or the channel is stopped.
- The IRQ tie-in is automatic: `_abcmusic_irq` is registered with `.interruptor`,
  so the cc65 interrupt dispatcher calls it. It checks the VBL bit in `INTSET`
  (`$FD81`) and, on a VBL, runs the per-channel update.

### Low-level helpers

These poke a single channel register and are useful for sound effects or custom
instruments outside the note parser. The channel-to-register math is
`reg_base = base + (chan << 3)` (each Mikey channel occupies 8 bytes):

| Call | Register | Effect |
|------|----------|--------|
| `abcvolume`    | `$FD20+8c` VOLUME   | signed output volume |
| `abcpitch`     | `$FD24+8c` BACKUP   | timer reload = pitch within the octave |
| `abcoctave`    | `$FD25+8c` CONTROL  | clock-divider bits 0–2 = octave band |
| `abcintegrate` | `$FD25+8c` CONTROL  | bit 5 = integrate (waveform shape) |
| `abctaps`      | `$FD21+8c` FEEDBACK + CONTROL bit 7 + `$FD27` | LFSR feedback tap selection (timbre/noise) |

---

## 3. Tune syntax

A tune is a NUL-terminated ASCII string. Tokens:

### Notes and accidentals

| Token | Meaning |
|-------|---------|
| `C D E F G A B` | natural notes, middle octave |
| `c d e f g a b` | same notes one octave higher (lowercase) |
| `^C ^F ...`     | sharp (raise one semitone) |
| `_D _E ...`     | flat (lower one semitone) |
| `C,`            | one octave **lower** (trailing comma) |
| `c'`            | one octave **higher** (trailing apostrophe) |
| `z`             | rest / silence |

Notes and rests accept an optional **duration** number immediately after them:
`C` = 1 unit, `C2`, `C3`, `C4`, … and `z`, `z2`, `z3` for rests. Real duration in
VBL frames = `number × tempo` (see `T` below; default tempo 6).

### Inline commands (each takes a numeric argument)

| Cmd | Name | State set | Default |
|-----|------|-----------|---------|
| `V` | Volume        | channel target volume `abc_channel_volume` (0..127) | 127 |
| `R` | Ramp / attack | per-frame attack increment `abc_instrument_incr` | 4 |
| `H` | Hold          | frames of full-volume sustain `abc_instrument_maxlen` | 4 |
| `K` | Kill / decay  | per-frame decay decrement `abc_instrument_decr` | 4 |
| `T` | Tempo         | frames per duration unit `abc_tempo` | 6 |
| `O` | Octave base   | added to each note's octave `abc_oct` | 0 |
| `I` | Integrate     | waveform: 0 = square, 1 = integrate/triangle | — |
| `X` | XOR taps      | LFSR feedback taps (timbre / noise colour) | — |

### Structure

| Token | Meaning |
|-------|---------|
| `\|:`  | start repeat section; optional count after it (`\|:3`). Count 0 ⇒ treated as 255. |
| `:`   | end repeat section; loops back to the matching `\|:` |
| `' '` | spaces are ignored (free formatting) |
| `\0`  | end of tune (clears `abcactive`) |

Repeat semantics: a count of **255 means infinite loop**; any other count plays
the section `count` times. Only **one repeat level per channel** is supported
(no nesting), and the repeat offset is stored as a single byte (see Limitations).

`R` (attack) interacts with the start volume: if `R` is non-zero the note begins
at silence and ramps up; if `R` is zero the note starts immediately at the channel
volume.

---

## 4. Envelope model

Per-note volume is a simple AHD (attack/hold/decay) shape evaluated once per VBL
frame in `envelope`:

```
volume
  ^
  |        ____________            <- channel_volume (V)
  |       /            \
  |      / (R)      (H) \  (K)
  |     /                \____
  +----+------------------+--------> frames
        attack    hold     decay
```

- `note_playing[c]` counts the remaining frames; `abc_note_length[c]` is the
  original total (`number × tempo`).
- `elapsed = abc_note_length - note_playing`.
- **Attack:** while `current_volume < channel_volume`, add `abc_instrument_incr`
  (`R`) each frame, clamped to `channel_volume`.
- **Hold:** once `elapsed >= abc_instrument_maxlen` (`H`) the note begins decaying.
- **Decay:** subtract `abc_instrument_decr` (`K`) each frame, clamped at 0.
- A note length of **255 plays "forever"** (duration is not decremented).
- When `note_playing` hits 0 the channel volume is set to 0 and the next token is
  fetched on the following frame.

---

## 5. Pitch generation

Pitch is split into two parts: a **timer reload (BACKUP, `$FD24`)** for the
chromatic step within an octave, and a **clock divider (CONTROL bits 0–2,
`$FD25`)** for the octave band.

`_delays` (RODATA) holds the 12-semitone reload table, one octave wide, 14 bytes
to allow direct indexing of every sharp slot:

```
C=255  ^C=240  D=227  ^D=214  E=202  ^E=191  F=191  ^F=180
G=170  ^G=161  A=152  ^A=143  B=135  ^B=127
```

The ratio between adjacent entries is ≈ 2^(−1/12) (255 → 127 over the octave =
one octave down in frequency for a larger reload value). Higher reload ⇒ slower
timer ⇒ lower pitch.

The `pitch` routine converts a note letter into an index into `_delays`
(`cur_note`) and an octave delta (`cur_octave`):

- Letters `A..G` (uppercase) → base octave; `a..g` (lowercase) → one octave up
  (`dec cur_octave`, i.e. a smaller divider).
- `^` adds one to the index (sharp), `_` subtracts one (flat).
- The A/B-before-C ordering is rotated to the C-based table by the
  `+14 / −5` arithmetic, giving these natural indices: A=10, B=12, C=0, D=2,
  E=4, F=6, G=8 (verified against `_delays`).
- `,` and `'` post-modifiers add/subtract an octave.

The final octave fed to `setoctave` is `abc_oct[c]` (`O` command) + the note's own
octave delta. `setoctave` writes the divider into CONTROL bits 0–2 and always sets
`$18` (bit 3 = reload enable, bit 4 = count enable).

### CONTROL register (`$FD25+8c`) bit usage in this code

| Bits | Used for |
|------|----------|
| 0–2  | clock divider = octave (`setoctave`) |
| 3    | reload enable (always set via `\|$18`) |
| 4    | count enable (always set via `\|$18`) |
| 5    | integrate (`I` / `abcintegrate`) |
| 7    | upper LFSR feedback tap (`X` / `abctaps`) |

`taps`/`taps40` and the `X` command program the polynomial LFSR feedback across
FEEDBACK (`$FD21`), CONTROL bit 7, and the `$FD27` "other" register — this is what
gives different timbres and noise effects (the 9-bit tap value is why `abctaps`
takes an `unsigned int`).

---

## 6. Runtime flow

```
VBL IRQ ─► _abcmusic_irq ─► (VBL bit set?) ─► abcupdate
                                               │
              for channel c in 0..3:           │
                if !abcactive[c]: skip         │
                if note_playing[c]==0:         │  note finished
                    parse_abc  ───────────────►│  read next token(s),
                                               │  program hw, set duration
                else:                          │
                    envelope  ────────────────►│  shape volume this frame
```

`parse_abc` is the tokenizer: it reads characters via `abc_peek_char` /
`abc_read_char` (which dereference the per-channel zero-page score pointers
`_abc_score_ptr0..3` indexed by `Y`), dispatches on the character, reads numbers
with `abc_read_number` (decimal, ×10 accumulation), and for a note computes pitch,
octave, start volume, and duration before storing the updated byte offset back to
`abc_music_ptr[c]`.

### State arrays (indexed by channel 0–3)

| Symbol | Purpose |
|--------|---------|
| `_abc_score_ptr0..3` (ZP) | base pointer to each channel's tune string |
| `abc_music_ptr[c]`        | **byte offset** into the tune (current read position) |
| `_abcactive[c]`           | voice busy flag |
| `abc_repeat_offs[c]`      | tune offset of the active `\|:` |
| `abc_repeat_cnt[c]`       | remaining repeat count (255 = infinite) |
| `abc_tempo[c]`            | frames per duration unit (`T`) |
| `abc_channel_volume[c]`   | target volume (`V`) |
| `abc_instrument_incr[c]`  | attack step (`R`) |
| `abc_instrument_maxlen[c]`| hold length (`H`) |
| `abc_instrument_decr[c]`  | decay step (`K`) |
| `abc_note_length[c]`      | total frames of current note |
| `note_playing[c]`         | remaining frames of current note |
| `sound_channel_current_volume[c]` | live envelope volume |
| `abc_oct[c]`              | octave base (`O`) |

---

## 7. Quirks, bugs and limitations

These matter for both correct use and for the compile-time redesign below.

1. **256-byte tunes.** `abc_music_ptr[c]` and `abc_repeat_offs[c]` are single
   bytes, so a tune (and the position of a repeat) must live within the first 256
   bytes of the string. Longer tunes silently wrap.
2. **One repeat level, no nesting.** A second `|:` overwrites the stored offset/
   count for that channel.
3. **Per-note parsing cost in the IRQ.** Each note boundary runs the full ASCII
   tokenizer (character classification, decimal multiply for `abc_read_number`,
   pitch index arithmetic, octave loop) inside the VBL handler. This is the main
   cost the redesign targets.
4. **`_IFCS`/`_IFGE` are identical** (both emit `bcc` to the else label) and
   `_IFCC` emits `bcs`. Fine for unsigned `>=` / `<` comparisons but the `_IFCS`
   naming is misleading — verify before reuse.
5. **Lazy init.** `abcplay` performs an `abcstop` on first use, but `abcstop` is
   still the documented init entry point; relying on lazy init alone leaves
   stereo panning unset until the first play.
6. **`O` octave base is unbounded.** `abc_oct + cur_octave` is written straight to
   the divider with no clamp; large values just keep adding to CONTROL bits 0–2.
7. **No note re-trigger guard.** The commented-out `parse_small` block suggests an
   abandoned attempt to clamp hold vs. note length; currently `H` longer than the
   note simply never reaches decay.

---

## 8. Design: compile-time tune compilation

### 8.1 Goal

Eliminate runtime ASCII parsing. Today, every note boundary re-derives, inside the
VBL IRQ, values that are **fully determined by the tune text** and never change at
run time: the pitch reload byte, the octave/control nibble, the start volume, and
the note duration in frames. All of these can be computed once, ahead of time, and
stored as a compact binary **event stream** that the runtime simply replays.

Result: the IRQ "fetch next note" path collapses from a multi-branch tokenizer
with a decimal-multiply loop into a short opcode dispatch that mostly just pokes
registers.

### 8.2 Compiled event-stream format

Replace the ASCII tune with a byte-coded stream. Suggested encoding (one byte
opcode in the high bits, operands following):

```
NOTE   : [op|flags][reload][ctl][dur_lo][dur_hi]   ; play a note
REST   : [op][dur_lo][dur_hi]                       ; silence for N frames
SETV   : [op][value]      ; channel volume
SETR   : [op][value]      ; attack step
SETH   : [op][value]      ; hold length
SETK   : [op][value]      ; decay step
SETI   : [op][value]      ; integrate flag
SETX   : [op][lo][hi]     ; LFSR taps (pre-shifted into hw layout)
RPTB   : [op][count]      ; begin repeat (count, 255 = infinite)
RPTE   : [op]             ; end repeat
END    : [op]             ; end of tune
```

Key decisions:

- **Pre-resolve pitch + octave.** The compiler tracks the running `O` base and the
  note's own modifiers (`^ _ , '` and upper/lowercase) and emits the final
  `_delays` reload byte and the final CONTROL value (octave divider + `$18`
  + integrate bit). The runtime no longer needs the `_delays` table, the `pitch`
  routine, or `setoctave` math for note playback.
- **Pre-resolve duration.** Because `T` is known at compile time, emit the final
  frame count (`number × tempo`) directly as a 16-bit value. This removes
  `abc_read_number` and the duration multiply loop from the IRQ. 16-bit duration
  also lifts the implicit 255-frame note ceiling cleanly (the "forever" case can
  be a reserved value, e.g. `0xFFFF`, or a NOTE flag bit).
- **Pre-resolve start volume.** Emit a flag bit in the NOTE opcode: "start at
  silence (ramp)" vs "start at channel volume", computed from whether `R` is zero,
  exactly as `parse_abc` does today.
- **Config events are rare** (`V/R/H/K/I/X`), so encoding them as separate small
  events costs little and keeps NOTE compact.
- **16-bit stream offset.** Widen `abc_music_ptr[c]` and the repeat offset to 16
  bits, removing the 256-byte limit. (If a per-channel zero-page pointer pair is
  used instead of an offset, indexing also gets faster.)
- **Repeats** keep the same one-level model unless nesting is wanted; if so, a
  small per-channel stack of (offset,count) pairs can be added — but that is
  optional and orthogonal to the parse-elimination goal.

Approximate size: a plain note today is 1–3 ASCII bytes; as an event it's ~5
bytes. The stream is somewhat larger than the source text but removes all parsing
code paths and tables from the hot path. If size matters, a denser variable-length
encoding (e.g. pack reload+ctl, use 1-byte duration when it fits) recovers most of
the difference.

### 8.3 The compiler — a standalone external utility

**Decision:** the compiler is a **separate, self-contained command-line tool**,
developed and maintained **outside the cc65 codebase**. It is not a ca65 macro
package, not a cc65 source file, and not built as part of the cc65 / **lynxcc** build.
It is an independent utility that a project invokes (manually or from its own
Makefile) to turn ABC tune text into a compiled event stream before the normal
cc65 build runs.

Rationale for keeping it external:

- The cc65 tree stays untouched — no new source files, no new build dependency,
  nothing for the library to carry. The runtime side only gains the small
  `next_event` replay path (8.4); the parsing logic moves entirely off-target.
- Full host language (C, or Python for a reference implementation) for the
  stateful tokenizer, instead of awkward assembly-time macro string handling.
- Easy to unit-test the output against the documented runtime semantics, and to
  add build-time validation (range checks, repeat/phrase matching, length
  warnings) without touching the player.
- Naturally supports >256-byte tunes and the denser encodings in 8.6.

**Tool shape (suggested):**

```
abccc  [options]  input.abc ...            # "abc cross-compiler"
  -o FILE        output file
  -f s|h|bin     output format: ca65 asm (.s), C header (.h), or raw binary
  -l LABEL       symbol/label name for the emitted stream (asm/header)
  --dense        enable nibble-packed + default-duration + phrase-call encoding
                 (8.6); default is the simple byte-aligned format (8.2)
  --validate     range/structure checks only, no output
```

- **Input:** one or more ABC tune text files (the same syntax as section 3). The
  input format is the tool's own concern; a simple convention is one tune per file,
  or a small wrapper that names multiple tunes.
- **Output:** by default a generated `.s`/`.h` with the event-stream `.byte`
  tables (and a public label) that the project assembles and links normally; a raw
  `.bin` option is available for tunes loaded as data/assets.
- **Contract:** the emitted bytes must be bit-identical to what an equivalent
  runtime parse would produce, so the on-target `next_event` replay (8.4) and the
  field layouts (8.2 / 8.6) are the shared specification between this tool and the
  player. Those two must be versioned together.

Because the tool is external, it can be written in whatever is most convenient on
the build host and distributed independently of cc65; the only coupling to the
target is the documented stream format.

The runtime ASCII parser (sections 3–6) may optionally be retained behind a build
flag for quick experimentation or live-entered tunes, but the compiled-stream path
produced by this external tool is the intended default.

### 8.4 Runtime changes implied

- New entry `abcplaybin(chan, stream)` (or repurpose `abcplay`) taking a compiled
  stream pointer; `abcactive`, `abcstop`, the envelope engine, and all low-level
  helpers are unchanged.
- Replace `parse_abc` with `next_event`: read opcode → small jump table →
  for NOTE poke BACKUP (`reload`), write CONTROL (`ctl`), set
  `note_playing`/`abc_note_length` from the 16-bit duration, set start volume from
  the flag. For config events, store the operand into the matching state byte.
- `envelope`, `volume`, and the VBL tie-in stay as-is — only the "fetch next note"
  half of `abcupdate` changes.
- Delete from the hot path (can stay for the optional text parser): `pitch`,
  `_delays` indexing for playback, `abc_read_number`, the duration multiply loop,
  and character-class branches.

### 8.5 Estimated payoff

The per-note IRQ work drops from "classify chars + decimal multiply + pitch index
math + octave loop" to "load opcode, dispatch, 3–4 stores". On a note boundary
that is a large reduction in VBL CPU time; between boundaries (the common frame)
only `envelope` runs, unchanged. The freed cycles and the removal of the 256-byte
limit are the main wins; the cost is a modest increase in tune data size and one
build-time tool.

### 8.6 Stream density (reducing the byte count)

The byte-aligned format in 8.2 is ~5 bytes per note, larger than the ASCII source.
This is recoverable, but there is a hard tradeoff to respect: the whole point of
the redesign is to **minimise VBL-IRQ CPU**, and the 65C02 has no barrel shifter,
so any field that straddles a byte boundary costs a shift/mask loop to unpack —
which re-introduces the per-note cost we just removed. So the rule is: **pack to
nibble/byte fields, never to a free-running bitstream.**

Strict "bit-stuffing" (HDLC-style insertion of bits to avoid reserved patterns)
is the wrong tool — it *adds* bits. What helps is **bit-packing** of low-entropy
fields plus **structural** compression.

**Per-event entropy.** A NOTE's information content is small:

| Field | Range | Bits |
|-------|-------|------|
| semitone index (into `_delays`) | 0–13 | 4 |
| octave (pre-resolved, absolute) | 0–7  | 3 |
| integrate | 0–1 | 1 |
| start-volume flag (ramp vs full) | 0–1 | 1 |
| duration | variable | (handled structurally, below) |

That is ~9 bits of fixed fields, versus 24 bits (3 bytes) for `reload+ctl+flags`
byte-aligned. Store the **semitone index**, not the reload byte, and recover the
reload at playback with `lda _delays,y` — that indexed load is cheap (it is the
*parsing* we removed, not the table lookup).

**Nibble-packed note (cheap to decode).** Pack the fixed fields into one byte:

```
note byte:  [oct:3][int:1][semitone:4]
            decode: A=byte; semitone = A & $0F          ; one AND
                    octave/int = A >> 4  (4x LSR or table)
```

Integrate and start-volume change rarely, so they are better carried as current
channel state set by separate `SET*` events than spent per note — freeing the top
bits and letting the common note shrink further.

**Structural wins (better ratio than bit-packing, and nearly free at runtime):**

1. **Default-duration / running-status notes.** Most consecutive notes share a
   duration. Add a `SETDUR n` event and a "note uses current default duration"
   form, so the common note carries *no* duration operand. This is usually the
   single biggest reduction — ABC melodies are full of equal-length runs.
2. **Phrase calls.** Repeated motifs beyond what `|:…:|` captures can be factored
   out: a `CALL offset` / `RET` opcode pair plays a shared phrase once in the
   stream. Runtime cost is just push/pop of a stream pointer — far cheaper than
   any unpacking, and a strong ratio on melodic content. (Reuses the per-channel
   pointer stack already needed for nested repeats.)
3. **Opcode in the high nibble.** Use the top nibble of the lead byte as opcode and
   the low nibble as a small inline operand (octave, short count). Notes that need
   a full operand spill into one extra byte; everything else stays single-byte.

**Resulting common-case layout (1 byte/note):**

```
0nnn nnnn   bit7=0  -> NOTE with current default duration
            bits6-4 = octave (0..7)
            bits3-0 = semitone index (0..13)   ; integrate+startvol from state
1ooo ....   bit7=1  -> opcode (NOTE+duration, REST, SETV/R/H/K/I/X,
                       SETDUR, CALL, RET, RPTB/RPTE, END), operands follow
```

This gets the dominant event — a note at the prevailing duration — down to a
single byte (from 5), decoded with one load, one mask and one shift, while
explicit-duration notes, config changes and structure pay only when they occur.

**When to stop.** Going below nibble alignment (true cross-byte bitstream) can
squeeze out a little more but the unpack loop eats into the IRQ-CPU budget that
was the original goal. Only pursue it if ROM size is the hard binding constraint;
otherwise the nibble-packed + default-duration + phrase-call scheme is the sweet
spot.

### 8.7 Worked example: integrating `abccc` with game source

End-to-end flow from a tune file to a running game. Two integration styles are
shown: compiling the tune into a **linked symbol** (common), and emitting a **raw
binary asset**.

**1. Author the tune** — `assets/theme.abc`, plain ABC text (section 3 syntax):

```
T6 V100 R8 H12 K4
|: CDEF GABc :|
z4
```

**2. Compile it** as a pre-build step:

```
abccc -f s -l theme_music -o assets/theme.s assets/theme.abc
```

**3. `abccc` emits a ca65 source file** `assets/theme.s` (illustrative bytes):

```asm
; Generated by abccc - do not edit by hand. Source: theme.abc
    .export _theme_music
    .segment "RODATA"
_theme_music:
    .byte $C2, 100        ; SETV 100
    .byte $C3, 8          ; SETR 8
    .byte $C4, 12         ; SETH 12
    .byte $C5, 4          ; SETK 4
    .byte $A0             ; RPTB (infinite)
    .byte $02, $0C        ; NOTE oct2 C  (default dur)
    .byte $02, $0E        ; NOTE oct2 D
    ; ... E F G A B c ...
    .byte $A1             ; RPTE
    .byte $C1, $04, $00   ; REST 4 frames
    .byte $00             ; END
```

…and a matching header `assets/theme.h`:

```c
extern const unsigned char theme_music[];
```

The `_` prefix on the asm label is the cc65 C-name convention, so C sees
`theme_music`.

**4. Game C code** uses the runtime replay entry (`abcplaybin`, §8.4):

```c
#include <lynx/lynx.h>
#include "ABCMusic.h"
#include "theme.h"

void main(void) {
    abcstop();                    // init once
    abcplaybin(0, theme_music);   // start the compiled stream on channel 0
    for (;;) {
        // game loop; the VBL IRQ plays/advances the stream on its own
    }
}
```

`abcplaybin` only stores the stream pointer for the channel; the existing VBL
handler does playback, exactly as `abcplay` did with text. One new prototype is
added to `ABCMusic.h`:

```c
void __cdecl__ abcplaybin(unsigned char channel, const unsigned char *stream);
```

**5. Makefile wiring** — `abccc` lives outside cc65, so it is just another rule:

```makefile
ABCCC := abccc

# any assets/<name>.abc -> assets/<name>.s with label <name>_music
assets/%.s: assets/%.abc
	$(ABCCC) -f s -l $(notdir $*)_music -o $@ $<

OBJS += main.o assets/theme.o

game.lnx: $(OBJS)
	cl65 -o $@ $(OBJS)
```

The generated `theme.s` assembles and links like any other source; the tune ends
up in `RODATA` in the cartridge image.

**Raw-asset variant.** To avoid linking the tune as a symbol (e.g. streaming many
tunes from a data file), emit a binary and pull it in directly:

```
abccc -f bin -o assets/theme.bin assets/theme.abc
```

```asm
_theme_music: .incbin "assets/theme.bin"
```

…or load `theme.bin` into a RAM buffer at runtime and pass that buffer to
`abcplaybin`. Either way the on-cart format is identical — the only contract
between `abccc` and the player is the byte layout in §8.2 / §8.6.

---

## 9. Quick reference

```
init:        abcstop();
play:        abcplay(0, "T6 V100 R8 H10 K4 |: CDEF GABc :| z4");
stop one:    abcplay(c, "");      // or play an empty/terminated string
stop all:    abcstop();
busy?:       if (abcactive[c]) ...
```

---

## 10. `abcrom` — tune test-ROM utility

A separate command-line utility (like `abccc`, **outside** the cc65 codebase) that
turns a tune into a runnable `.lnx` for auditioning in an emulator.

### 10.1 Approach: template patcher (zero toolchain at test time)

`abcrom` does **not** invoke cc65. It ships a single, pre-assembled **template
`.lnx`** that already contains the abcmusic player plus a tiny boot harness, with a
fixed region reserved for the tune stream. To make a test ROM, `abcrom`:

1. compiles the tune to an event stream (calls `abccc`, or accepts an
   already-compiled `.bin`),
2. **patches those bytes into the reserved region** of a copy of the template,
3. writes out the result.

No compile, no link — just byte patching — so a tune goes to a playable `.lnx` in
milliseconds, and the utility runs anywhere with no cc65 installed.

Because the reserved region is **fixed-size**, patching never changes the file
length, so the `.lnx` header stays valid (only the cosmetic cart-name field is
optionally rewritten).

### 10.2 The template ROM

Built **once** with the normal toolchain (a `make` target in the player project,
not part of `abcrom` itself). It contains:

- the abcmusic player object (`abcmusic.o`),
- a minimal boot harness: on reset it calls `abcstop()`, then `abcplaybin()` for
  each non-empty tune region, then loops (the VBL IRQ does the playback),
- one or more **reserved tune regions** in `RODATA`, each fronted by a magic
  header so the patcher can locate them without hard-coding file offsets.

The harness references each region by its linker symbol, so it reads whatever bytes
`abcrom` later writes there. The player replays a stream until the `END` opcode
(`$00`), so the harness needs no separate length — an empty region is just a single
`END` byte and that channel goes inactive immediately.

### 10.3 Reserved-region layout

Each region begins with a small marker so `abcrom` can find it by **scanning for
the magic** (robust against template rebuilds that shift offsets):

```
offset  size  field
  0      4     magic  "ABCR"
  4      1     format version  (must match abccc / player stream version)
  5      1     channel index   (0..3)
  6      2     capacity (bytes of payload that follow)   LE
  8      2     used     (bytes written by abcrom; 0 in a fresh template) LE
 10      N     payload (event stream, must end with END within `capacity`)
```

`abcrom` locates a region by its `"ABCR"` magic + channel index, checks the
**format version** matches (refuses a stale template otherwise), checks the stream
plus its `END` fits in `capacity`, then writes `used` and the payload. Default
`capacity` of ~4 KB per region is far more than any tune needs.

### 10.4 What `abcrom` does

```
1. compile tune.abc -> stream bytes        (abccc, or read tune.bin)
2. copy template.lnx -> out.lnx
3. scan out.lnx for "ABCR" + channel marker
4. verify format version; verify len(stream)+END <= capacity
5. write `used` and patch stream into payload
6. (optional) rewrite cart-name field in the .lnx header
7. (optional) launch emulator on out.lnx
```

### 10.5 CLI

```
abcrom  [opts]  tune.abc
  -o FILE          output .lnx (default: <tune>.lnx)
  -t FILE          template .lnx (default: bundled template)
  --channels MAP   place tunes on channels, e.g. 0:bass.abc,1:lead.abc
  --name STR       cart-name field in the .lnx header
  --run [EMU]      launch emulator after patching
  --bin            input is already a compiled stream, skip abccc
```

Typical quick test: `abcrom --run theme.abc`.

### 10.6 Multi-channel

The template reserves up to four regions (`"ABCR"` markers, channel index 0–3).
`--channels 0:bass.abc,1:lead.abc` patches each named tune into its region; unused
regions keep their single `END` byte and stay silent. The harness calls
`abcplaybin` for every region whose payload does not start with `END`.

### 10.7 Tradeoffs and caveats

- **Speed/portability win:** no toolchain at test time; instant turnaround.
- **Main fragility — template/version coupling.** The template embeds a specific
  player build and stream-format version. Whenever the player or the stream format
  (§8.2 / §8.6) changes, the template must be **rebuilt once** with the toolchain.
  The `format version` byte makes a mismatch a hard error rather than silent
  corruption.
- **Fixed capacity.** Tunes larger than a region's `capacity` are rejected; bump
  the reserved size and rebuild the template if ever needed.
- **Emulator target only.** The patched `.lnx` carries the standard 64-byte Handy
  header and runs in Handy / Mednafen / Felix. Real-hardware carts need the
  encrypted micro-loader — a separate, slower path, intentionally out of scope for
  quick testing.
