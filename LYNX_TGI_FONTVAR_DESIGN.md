# Lynx TGI: adding the proportional (variable-width) font

Status: design (2026-06-14). Adds a third bitmap font to the static TGI text
path: an **all-caps, variable-width pixel font** recovered from `img_help.bmp`.
It reuses the bit-packed strip builder introduced for the 5×5 compact font, so
the only genuinely new machinery is a **per-glyph advance table** and a
**proportional width query**. Programs that never select it link neither its
builder nor its data. This is the realisation of the future option noted in
`LYNX_TGI_FONT5X5_DESIGN.md` §9.

## 1. Source image analysis

`img_help.bmp` is a 160×102 paletted bitmap — a **native full-screen Lynx
frame** (160×102 is the Lynx display), so one image pixel equals one display
pixel and the font appears at its true authored size. It is the help/intro
screen of an "EggSavier" game: white body text, one yellow word (`EGGSAVIER`),
on a blue background, with green grass and a bunny sprite at the bottom.

Decoding the text pixels (ink = white **or** yellow; everything else =
background) gives an unambiguous picture of the font:

* **Caps only.** Every glyph is an upper-case letter, digit shape, or
  punctuation mark. No lower-case forms appear anywhere on the screen.
* **Cap height 5 px, line pitch 6 px.** Row-ink histograms show text bands of
  exactly 5 inked rows separated by a single blank row (`y 4–8`, `10–14`,
  `16–20`, …). A few punctuation marks (`,`) drop a single pixel into the 6th
  row, i.e. one pixel of descender.
* **Proportional, 1–5 px wide, 1 px gap.** Column segmentation of each text
  band splits cleanly at 1-px inter-glyph gaps. Line 1 segments into exactly 32
  runs and reads `THE EASTER BUNNY IS IN TROUBLE ! THE EGG` (= 32 letters), so
  **each run is one glyph**. Measured ink widths:

  | width | glyphs |
  |-------|--------|
  | 1 px  | `I ! ' . ,` |
  | 2 px  | `C F L` |
  | 3 px  | `A B D E G H J O P R S T U V Y` (most letters) |
  | 4 px  | `N` |
  | 5 px  | `M` |

  This is what makes the font *variable width*: an `I` occupies 1 px + 1 gap
  while an `M` occupies 5 px + 1 gap. A fixed pitch (as the 8×8 and 5×5 fonts
  use) cannot reproduce the look.

### Coverage and gaps

The screen exercises 24 distinct glyphs — `A B C D E F G H I J L M N O P R S T
U V Y` plus `! ' . ,`. It contains **no** `K Q W X Z`, **no digits**, and no
other punctuation. Those glyphs therefore cannot be lifted from the image and
must be authored to match. They are flagged `[D]` (designed) versus `[X]`
(extracted, authoritative) in the data table in §7.

## 2. Relationship to the existing font infrastructure

The compact-font work already paid for the hard part. From
`LYNX_TGI_FONT5X5_DESIGN.md` and the current `tgi-text.s` / `tgi-text5x5.s`:

* `tgi_outtext` runs a shared prologue, then `jmp (tgi_buildptr)` to the active
  builder; every builder ends in the shared `draw_and_advance` epilogue.
* `tgi_setfont` swaps `tgi_buildptr` plus the metric bytes `tgi_pitch` /
  `tgi_fontheight`, and force-links a builder/font only on the branch that
  selects it — so conditional linking is preserved.
* `build5x5` already packs glyphs into a **bit-shifted literal 1-bpp sprite**
  at an arbitrary bit position (`bit = i*PITCH`, `byteidx = bit>>3`,
  `shift = bit&7`), with a transparent background (normal sprite, pen 0) and the
  foreground in the current pen. The font is stored **fg = bit value 1**, ink
  left-aligned in the high bits, so rows OR straight into a zero-filled strip.

The variable-width font is the **same storage convention and the same builder
shape**. Only two things change:

1. The constant `PITCH` becomes a **per-glyph advance** read from a table.
2. The width query stops being `strlen × pitch` and becomes a **sum of
   advances**.

No third inner loop, no change to the sprite-draw core, no change to the 8×8 or
5×5 paths.

## 3. Data format

Two parallel tables (`tgi_fontvar` bitmaps and `tgi_fontadv` advances) share
one index. Since this font is caps-only, lower-case `a`–`z` are byte-identical
to `A`–`Z`, so — exactly as the 5×5 font now does — the builder **folds**
`a`–`z` onto `A`–`Z` and **splices** the freed slots out instead of storing the
duplicates. The index is therefore not a plain `(ch-32)`:

```
t = ch
if 'a' <= t <= 'z':  t &= 0xDF      ; fold to upper-case
t -= 32
if t >= 65:          t -= 26        ; only { | } ~ (123-126) remain above 64
```

Stored layout (**70 entries** per table):

* index `0..64` = ASCII `32..96` (space … backtick)
* index `65..68` = ASCII `123..126` (`{ | } ~`)
* index `69` = ASCII `127` (DEL, blank)

### 3.1 `tgi_fontvar` — glyph bitmaps (5 bytes/glyph)

5 rows per glyph, one byte per row, **ink left-aligned starting at bit 7**,
**bit value 1 = foreground** (identical to the 5×5 convention). A 5-px glyph
(`M`, `W`) fills bits 7..3; narrower glyphs leave the low bits clear. Because
ink never reaches the low 3 bits, the builder's carry-into-next-byte shift
(`src << (8−shift)`) can never drop a pixel — the same safety property the 5×5
builder relies on.

`70 glyphs × 5 bytes = 350 bytes`.

### 3.2 `tgi_fontadv` — advance table (1 byte/glyph)

`advance = ink_width + 1` (the 1-px inter-glyph gap), i.e. the amount the
cursor and the pack position move per character. Range 2 (`I`) … 6 (`M`/`W`).
Space (`32`) is given advance 4. Glyphs with no bitmap default to advance 4.

`70 bytes`. Total font cost **420 bytes** of `.rodata` (was 576 before the
lower-case fold).

A 20-char line maxes at `20 × 6 = 120 px < 256`, so a string's total advance
always fits in a single byte — important for the width query (§5).

## 4. The new builder `buildvar`

A near-clone of `build5x5` (`tgi-textvar.s`), differing only in how the per-
character bit position is derived. `build5x5` computes `bit = i*6`; `buildvar`
keeps a **running accumulator** seeded from the advance table.

For an `N`-char string (capped at the existing 20):

```
; foldsplice(ch) -> t : fold a-z onto A-Z then drop the freed slots (see §3)
; pass 1: total advance -> strip byte width
total = 0
for i in 0..N-1:  total += tgi_fontadv[foldsplice(s[i])]
W     = (total + 7) >> 3                 ; strip byte-width (<= 15)
strip = 5 rows of [offset=W+1][W pixel bytes], then a 0 terminator
                                         ; 5*(1+15)+1 = 81 bytes -> reuse text_bitmap
; pass 2: pack
bitpos = 0
for i in 0..N-1:
    t       = foldsplice(s[i])
    byteidx = bitpos >> 3
    shift   = bitpos & 7
    gp      = tgi_fontvar + t*5
    for r in 0..4:
        strip[row r][byteidx]   |= gp[r] >> shift
        strip[row r][byteidx+1] |= gp[r] << (8-shift)
    bitpos += tgi_fontadv[t]
; sprite type $04, pen byte = tgi_drawindex, height 5  (as build5x5)
draw_and_advance
```

The two passes share one subroutine — *sum the advances of the capped string* —
which is exactly what the width query needs (§5), so factor it into a helper
(`str_advance`) called by both `buildvar` and `tgi_gettextwidth`.

Everything else — zeroing the strip, writing each row's `W+1` offset byte, the
5-row shift/OR, the transparent normal sprite with pen byte `tgi_drawindex`,
the single scaled draw — is `build5x5` verbatim. The whole string is still one
sprite scaled once, so glyph spacing scales with the text.

## 5. Proportional width query

`tgi_gettextwidth` currently returns `strlen × tgi_pitch × scale >> 8`. That is
wrong for a proportional font, and it is also the value `draw_and_advance` uses
to step the cursor — so it must become advance-aware **without** force-linking
the variable font into fixed-pitch programs.

Add one indirect datum beside the existing metric bytes:

```asm
; tgi-text.s (.data)
tgi_advtab:     .addr   0       ; 0 => fixed pitch; else -> advance table
```

`tgi_gettextwidth` branches on it:

* `tgi_advtab == 0` (8×8, 5×5): unchanged fast path,
  `strlen × tgi_pitch × scale >> 8`.
* `tgi_advtab != 0` (variable): walk the capped string, sum
  `tgi_advtab[foldsplice(ch)]` into a byte `total` (the shared `str_advance`
  helper, which applies the same fold+splice as the builder), then
  `total × scale >> 8` on Suzy's multiplier exactly as today.

Because the default `tgi_advtab` is 0 and only `tgi_setfont`'s variable branch
ever stores `tgi_fontadv` into it, the advance table and `buildvar` link **only**
when a program selects the variable font. `tgi_gettextheight` is unchanged
(`tgi_fontheight × scale >> 8`, with height 5).

## 6. Switching mechanism / public API

Extend the existing scheme with one font id.

```c
/* include/tgi.h */
#define TGI_FONT_BITMAP    0    /* system 8x8 (default)            */
#define TGI_FONT_COMPACT   1    /* transparent 5x5, 6px fixed      */
#define TGI_FONT_VARIABLE  2    /* proportional caps, 1..5px + gap */
void __fastcall__ tgi_setfont (unsigned char font);
```

```asm
; tgi-setfont.s, new branch
@var:   lda #<buildvar  : sta tgi_buildptr
        lda #>buildvar  : sta tgi_buildptr+1
        lda #<tgi_fontadv : sta tgi_advtab     ; enables proportional width
        lda #>tgi_fontadv : sta tgi_advtab+1
        lda #6          : sta tgi_pitch        ; fallback / unused when advtab set
        lda #5          : sta tgi_fontheight
        rts
```

The `BITMAP` and `COMPACT` branches must additionally `stz tgi_advtab` /
`stz tgi_advtab+1` so switching *back* from the variable font restores the
fixed-pitch width path. Add `TGI_FONT_VARIABLE = 2` to
`asminc/tgi-kernel.inc`.

## 7. Font data (extracted + designed)

`[X]` = extracted from `img_help.bmp` (authoritative). `[D]` = designed to
match the house style (absent from the source — verify on hardware).
Lower-case `a`–`z` are **not stored**: the builder folds them onto `A`–`Z`
(§3), so the table holds 70 entries (ASCII 32–96, then 123–126, then a blank
DEL). Excerpt of the 70-entry table emitted by the generator (§8):

```asm
tgi_fontvar:
        .byte $00, $00, $00, $00, $00   ; 32 ' '  [D]
        .byte $80, $80, $80, $00, $80   ; 33 '!'  [X]  w1
        .byte $80, $80, $00, $00, $00   ; 39 '\'' [X]  w1
        .byte $00, $00, $00, $00, $80   ; 44 ','  [X]  w1 (1px descender)
        .byte $00, $00, $00, $00, $80   ; 46 '.'  [X]  w1
        .byte $E0, $A0, $A0, $A0, $E0   ; 48 '0'  [D]  w3
        ; 49-57 '1'..'9' [D]
        .byte $40, $A0, $A0, $E0, $A0   ; 65 'A'  [X]  w3
        .byte $C0, $A0, $C0, $A0, $C0   ; 66 'B'  [X]  w3
        .byte $C0, $80, $80, $80, $C0   ; 67 'C'  [X]  w2
        .byte $E0, $80, $C0, $80, $E0   ; 69 'E'  [X]  w3
        .byte $80, $80, $80, $80, $80   ; 73 'I'  [X]  w1
        .byte $A0, $A0, $C0, $A0, $A0   ; 75 'K'  [D]  w3
        .byte $88, $D8, $A8, $88, $88   ; 77 'M'  [X]  w5
        .byte $90, $D0, $B0, $90, $90   ; 78 'N'  [X]  w4
        .byte $E0, $A0, $A0, $C0, $E0   ; 81 'Q'  [D]  w3
        .byte $88, $88, $A8, $D8, $88   ; 87 'W'  [D]  w5
        .byte $A0, $A0, $40, $A0, $A0   ; 88 'X'  [D]  w3
        .byte $E0, $20, $40, $80, $E0   ; 90 'Z'  [D]  w3

tgi_fontadv:    ; ink width + 1, same fold+spliced index as tgi_fontvar
        .byte 4,2,4,4,4,4,4,2,4,4,4,4,2,2,2,4
        .byte 4,4,4,4,4,4,4,4,4,4,2,4,4,4,4,4
        .byte 4,4,4,3,4,4,3,4,4,2,4,4,3,6,5,4   ; @ A B C D E F G H I J K L M N O
        .byte 4,4,4,4,4,4,4,6,4,4,4,4,4,4,4,4   ; P Q R S T U V W X Y Z ...
```

(Advances: `C F L` = 3, `I`/punctuation = 2, `M`/`W` = 6, `N` = 5, the rest 4.)

## 8. Generator

A committed host script (`tools/genfontvar.py`, mirroring the 5×5 generator)
reproduces both tables from `img_help.bmp`:

1. classify ink = white|yellow, background = everything else;
2. find the 5-row text bands by row-ink histogram;
3. segment each band at 1-px gaps; map runs to the known on-screen text to
   label glyphs and capture the most common bitmap per character;
4. encode 5 rows left-aligned (`bit = ink_width-aligned`, fg = 1), advance =
   width + 1;
5. splice in the `[D]` designs for `K Q W X Z`, digits and extra punctuation;
   fold `a–z` onto `A–Z`;
6. emit `tgi-fontvar.s` and an ASCII-art proof sheet for visual check.

Keeping it in-tree means the `[D]` glyphs can be revised and the table
regenerated deterministically.

## 9. Files touched

| File | Change |
|------|--------|
| `libsrc/lynx/tgi/tgi-text.s` | add `tgi_advtab` (`.addr 0`); `tgi_gettextwidth` branches to the advance-sum path when `tgi_advtab≠0`; factor out the `str_advance` helper. 8×8 path otherwise byte-identical. |
| `libsrc/lynx/tgi/tgi-textvar.s` | **new**: `buildvar` — two-pass bit-packed strip with per-glyph advance (§4); shares the `build5x5` body shape and the shared epilogue. |
| `libsrc/lynx/tgi/tgi-fontvar.s` | **new**: `tgi_fontvar` (96×5) + `tgi_fontadv` (96) (§7). |
| `libsrc/lynx/tgi/tgi-setfont.s` | add the `TGI_FONT_VARIABLE` branch; `BITMAP`/`COMPACT` branches zero `tgi_advtab`. |
| `include/tgi.h` | add `TGI_FONT_VARIABLE 2`. |
| `asminc/tgi-kernel.inc` | add `TGI_FONT_VARIABLE = 2`. |
| `tools/genfontvar.py` | **new**: regenerates the tables from `img_help.bmp`. |

No change to `tgi-font.s` (8×8), `tgi-font5x5.s`, `tgi-text5x5.s`, or the
sprite-draw core.

## 10. Verification plan

1. **Host round-trip** (done): BMP → tables → ASCII proof sheet reproduces all
   24 extracted glyphs and the proportional widths; `[D]` glyphs render legibly
   alongside them.
2. **Build/link**: `tgi-fontvar.s` + `tgi-textvar.s` assemble; a sample using
   only `TGI_FONT_BITMAP`/`COMPACT` links **neither** (check the `.map`); a
   sample calling `tgi_setfont(TGI_FONT_VARIABLE)` pulls in `buildvar`,
   `tgi_fontvar`, `tgi_fontadv`.
3. **Regression**: an existing 8×8 (and 5×5) text program is byte-identical;
   `tgi_advtab` defaults to 0 so the width fast path is unchanged.
4. **Width correctness**: `tgi_gettextwidth("MIMI")` = adv(M)+adv(I)+adv(M)+adv(I)
   = 6+2+6+2 = 16; `tgi_gettextwidth("III")` = 6; confirm `draw_and_advance`
   leaves the cursor flush against the last glyph + gap.
5. **Emulator/hardware**: print the full A–Z / 0–9 set over a non-black
   background to confirm transparency, that the foreground tracks
   `tgi_setcolor`, that proportional spacing reads correctly (`I` tight, `M`
   wide), that `tgi_settextscale` scales glyphs *and* spacing together, and that
   `tgi_setfont` toggles cleanly among all three fonts mid-screen with the
   cursor advancing by each font's metric. Reproduce the original `img_help`
   intro text and compare pixel-for-pixel against the source frame.

## 11. Notes / open questions

* **Cell height stays 5.** The single-pixel `,` descender is folded onto the
  bottom row (as the 5×5 font does for `,` `;`), so height remains 5 and
  `buildvar` reuses the 5-row loop unchanged. If true descenders are wanted
  later, widen the cell to 6 rows — a localized change to the builder's row
  count and `tgi_fontheight`.
* **`[D]` glyphs are provisional.** `K Q W X Z` and the digits are not in the
  source art; the shapes here match the 3-px caps rhythm but the original
  designer's versions are unknown. They live in the generator so they are easy
  to refine once seen on hardware.
* **Lower-case folding** matches the 5×5 font's behaviour; if a future source
  supplies real lower-case forms, only the generator and the 97–122 rows change.
