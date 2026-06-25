<!--
SPDX-License-Identifier: CC-BY-4.0
Lynx Game Development SDK documentation, (c) 2026 the lynxcc authors,
licensed under Creative Commons Attribution 4.0 International.
See doc/licenses.html.
-->

# Lynx TGI: adding the 5×5 compact font + runtime font switching

Status: design (2026-06-14). Adds a second bitmap font to the static TGI text
path with its own **narrow-pitch, new builder**, a **transparent background**,
and **foreground in the current pen colour**, plus a mechanism to switch between
it and the existing 8×8 font at runtime. Programs that never select the compact
font do not link its builder or its data.

## 1. Source image analysis

`img_font5x5.bmp` is a 325×5, 2-colour paletted bitmap: one horizontal strip of
**65 fixed-size 5×5 glyphs** (325 / 5 = 65). White (`#FFFFFF`) is the
foreground; red (`#AA0000`) is the background.

Glyphs are in ASCII order from space, i.e. `ascii = 32 + index`, covering
**ASCII 32–96** (`space` … `` ` ``). Confirmed by decoding: indices 16–25 are
`0`–`9`, 33–58 are `A`–`Z`. Layout is `space ! " # $ % & ' ( ) * + , - . / 0-9
: ; < = > ? @ A-Z [ \ ] ^ _ ``` — the printable upper-ASCII block, **caps
only, no lowercase**.

## 2. Why a new builder (not the 8×8 path)

The existing `tgi-text.s` is locked to the 8×8 cell: it copies exactly 8 bytes
per glyph, each glyph occupying one **byte-aligned** column of an 8-px-wide
literal sprite, advances the cursor by `strlen*8`, and reports height `8*scale`.
That byte-alignment is the whole reason it is cheap — and the whole reason it
cannot do sub-8 spacing.

The compact font is 5 px wide, so its natural pitch is **6 px** (5 ink + 1 gap).
At 6 px, glyph `i` starts at bit `i*6`, which is **not** byte-aligned, so glyphs
must be bit-shifted into a packed strip. That is a different inner loop — a new
builder, as requested. The 8×8 builder is left **byte-for-byte unchanged** so
existing programs carry zero regression risk; the new builder lives beside it.

Pitch is a single assemble-time constant `FONT5X5_PITCH = 6`, trivially tunable.
True proportional spacing is a natural extension of this same bit-packed builder
(replace the constant pitch with a per-glyph advance read from a width table);
it is noted in §7 but not built in the first cut.

## 3. Transparency and pen colour

The Lynx gives transparency for free on **normal sprites**: pen index 0 is not
drawn. The existing code proves this — when the text background pen is 0 it
deliberately switches to a *background* sprite type to force pen 0 to paint a
solid box. The compact builder wants the opposite, so it always uses the normal
type and routes the background to pen 0.

For a 1-bpp sprite the pen buffer is one byte: high nibble = colour for pixel
value 0, low nibble = colour for pixel value 1. The compact builder sets:

```
pen byte = tgi_drawindex            ; high nibble 0, low nibble = current pen
sprite type = $04                   ; normal -> pen 0 is transparent
```

So pixel value 0 → pen 0 → **transparent**, pixel value 1 → `tgi_drawindex` →
**current pen colour**. `tgi_bgindex` is ignored entirely for this font. Foreground
follows whatever `tgi_setcolor` last set, exactly as asked.

Note this flips the bit convention relative to the 8×8 font: in the compact font
**bit value 1 = foreground**. That is why the font data is stored fg = 1 (§5),
which also lets the builder OR glyphs into a zero-initialised strip (zero =
transparent) with no inversion.

**The 8×8 font is now transparent too.** `build8x8` previously special-cased a
background pen of 0 by switching to a *background* sprite type, forcing pen 0 to
paint a solid black box. That special case is removed: `build8x8` always uses the
normal sprite type ($04), so with the default `tgi_bgindex` of 0 the background
(bit value 1) maps to pen 0 and is transparent. A non-zero `tgi_setbgcolor` still
produces an opaque coloured box, since the pen byte stays
`(tgi_drawindex << 4) | tgi_bgindex` (the 8×8 font keeps bit 0 = foreground).
Caveat shared with the compact font: drawing in pen 0 itself yields invisible
text on a normal sprite.

## 4. The new builder

A literal 1-bpp sprite, same row framing as the 8×8 strip (each row = one
offset byte + the pixel bytes; a trailing 0 offset ends the sprite), but with a
variable byte width and 5 rows instead of 8.

For an `N`-character string (capped at the existing 20):

```
W      = ceil(N * PITCH / 8)        ; strip byte-width, max ceil(120/8)=15
strip  = 5 rows of [offset=W+1][W pixel bytes], then a 0 terminator
                                    ; buffer = 5*(1+15)+1 = 81 bytes (.bss)
```

Build:

1. Zero the `W` pixel bytes of all 5 rows (0 = background = transparent).
2. Write each row's leading offset byte = `W+1`.
3. For each char `i`, `bit = i*PITCH`, `byteidx = bit>>3`, `shift = bit & 7`:
   * `t = foldsplice(ch)` (fold `a`–`z`→`A`–`Z`, then drop the freed slots — see §5); `gp = tgi_font5x5 + t*5`
   * for each of 5 rows `r`, take `src = gp[r]` (5 ink bits in bits 7..3):
     `strip[row r][byteidx]   |= src >> shift`
     `strip[row r][byteidx+1] |= src << (8-shift)`   (carry into next byte;
     `src`'s low 3 bits are 0 so the shift never loses ink)
4. Set sprite type `$04` and pen byte `tgi_drawindex` (§3).
5. Draw once, sprite height 5, scaled by `text_sx/text_sy` like today.
6. Advance the cursor by the string width (§6).

The whole strip is one sprite scaled once, so glyph spacing scales with the text
— the single-sprite / single-scale model of the original is preserved.

## 5. The compact font module

New file `libraries/graphics/tgi-font5x5.s` exporting `tgi_font5x5`: **5 bytes per
glyph**, 5 ink bits left-aligned in bits 7..3, **bit 1 = foreground**.

Because the source is caps-only, lower-case `a`–`z` would be byte-identical
copies of `A`–`Z`. Rather than store them twice, the builder **folds** `a`–`z`
onto `A`–`Z` (`AND #$DF`) and **splices** the freed slots out of the table, so
the index is no longer a plain `(ch-32)`. Stored layout (**70 glyphs**):

* index `0..64` = ASCII `32..96` (space … backtick) — the converted strip
  glyphs plus the filled punctuation (`{ | } ~` live at 123–126, see below).
* index `65..68` = ASCII `123..126` (`{ | } ~`, designed to match).
* index `69` = ASCII `127` (DEL, blank).

The builder maps a character to its index with: fold `a`–`z`, `t = ch-32`,
then `if t >= 65: t -= 26` (the only codes left above 64 after folding are
`123..126`). See §6 for the exact code.

70 × 5 = **350 bytes** (was 480 for a full 96-glyph table; 768 for the 8×8
font). Verified samples:

```asm
.byte $00, $00, $00, $00, $00   ; 32 ' '
.byte $70, $98, $A8, $C8, $70   ; 48 '0'
.byte $20, $50, $88, $F8, $88   ; 65 'A'
.byte $F0, $88, $F0, $88, $F0   ; 66 'B'
.byte $88, $D8, $A8, $88, $88   ; 77 'M'
```

The generator script (BMP → bytes, with lowercase folding) should be committed
so the table is reproducible.

## 6. Switching mechanism

Switching must select a *builder*, not just a data pointer, because the two
fonts use different inner loops, pitches, and heights. To keep conditional
linking (don't pull the compact code/data unless used), dispatch through an
indirect builder pointer plus two metric bytes rather than a branch:

```asm
; tgi-text.s (.data)
tgi_buildptr:   .addr   build8x8        ; active builder; default = system 8x8
tgi_pitch:      .byte   8               ; cursor advance per char
tgi_fontheight: .byte   8               ; rows, for tgi_gettextheight
```

* `_tgi_outtext` keeps the shared prologue (save string ptr, copy cursor to
  `text_x/text_y`, compute capped `STRLEN`) then `jmp (tgi_buildptr)`.
* `build8x8` is the current builder body (unchanged); `build5x5` is §4. Both end
  in the shared draw-and-advance epilogue.
* `tgi_gettextwidth` becomes `strlen * tgi_pitch * scale >> 8` (reads the byte
  instead of a literal 8); `tgi_gettextheight` is `tgi_fontheight * scale >> 8`.
  Both are now font-agnostic and force-link neither builder.

Because the default `tgi_buildptr` is `build8x8`, today's behaviour is
unchanged and only the 8×8 path links until the program asks for the compact
font. `build5x5`/`tgi_font5x5` are referenced solely by `tgi_setfont`'s compact
branch, so they link only when that path is taken.

Public API (`include/tgi.h`, `asminc/tgi-kernel.inc`):

```c
#define TGI_FONT_BITMAP   0     /* existing system 8x8           */
#define TGI_FONT_COMPACT  1     /* new transparent 5x5, 6px pitch */
void __fastcall__ tgi_setfont (unsigned char font);     /* tgi-setfont.s */
```

`tgi_setfont(TGI_FONT_BITMAP)`  → `buildptr=build8x8, pitch=8, height=8`.
`tgi_setfont(TGI_FONT_COMPACT)` → `buildptr=build5x5, pitch=6, height=5`.

Optionally the already-ignored `font` argument of `tgi_settextstyle` can be
forwarded to `tgi_setfont` so existing call sites gain switching for free; this
changes a currently-ignored argument's behaviour, so it is offered as a
secondary convenience and would require the `lynx/tgi.h` comment to be updated.

## 7. Files touched

| File | Change |
|------|--------|
| `libraries/graphics/tgi-text.s` | extract shared prologue/epilogue; add `tgi_buildptr`/`tgi_pitch`/`tgi_fontheight`; `build8x8` = current body (logic unchanged); `gettextwidth`/`gettextheight` read the metric bytes. |
| `libraries/graphics/tgi-text5x5.s` | **new**: `build5x5` (§4) — packed strip, transparent normal sprite, pen = `tgi_drawindex`, height 5. |
| `libraries/graphics/tgi-font5x5.s` | **new**: `tgi_font5x5`, 96 glyphs × 5 bytes, fg = bit 1. |
| `libraries/graphics/tgi-setfont.s` | **new**: set `buildptr`/`pitch`/`fontheight` from the font id. |
| `include/tgi.h` | add `TGI_FONT_COMPACT` + `tgi_setfont` prototype. |
| `asminc/tgi-kernel.inc` | add `TGI_FONT_COMPACT = 1`. |

No change to `tgi-font.s` (8×8 data) or to the sprite-draw core.

## 8. Verification plan

1. **Host round-trip** (done): BMP → packed bytes → ASCII art reproduces every
   source glyph; `ascii = 32 + index` confirmed; `A/B/M/0` verified.
2. **Build/link**: `tgi-font5x5.s` + `tgi-text5x5.s` assemble; a sample using
   only `TGI_FONT_BITMAP` links neither (check the map file); a sample calling
   `tgi_setfont(TGI_FONT_COMPACT)` pulls them in.
3. **Regression**: an existing 8×8 text program is byte-identical (default
   `buildptr`/`pitch`/`height`).
4. **Emulator/hardware**: print the full 32–96 range in the compact font over a
   non-black background to confirm the background shows through (transparency),
   that the foreground tracks `tgi_setcolor`, that the 6-px pitch reads tightly,
   and that `tgi_settextscale` scales both glyphs and spacing; toggle fonts
   mid-screen with `tgi_setfont` to confirm switching and that `gettextwidth`
   advance matches each font's pitch.

## 9. Future option (not in scope)

Proportional spacing: add a 96-byte advance table and replace the constant
`PITCH` in the §4 bit position and in `gettextwidth` with the per-glyph width.
The bit-packed builder already supports arbitrary bit offsets, so this is an
incremental change rather than a third builder.
