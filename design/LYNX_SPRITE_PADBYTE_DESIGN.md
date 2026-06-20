# Design: Sprite construction and the Suzy "last-pixel" pad-byte bug

Scope: how hand-built sprites and the runtime TGI text strips are encoded against
Suzy's end-of-line sprite-data bug. This document is the source of truth for the
pad-byte rule. Every claim here was checked on **GearLynx** (drhelius'
accuracy-focused emulator) running the built `.lnx` ROMs headless in the sandbox
(`tools/gearlynx`), reading back the rendered framebuffer pixel by pixel.

> **History.** Two earlier revisions were wrong in opposite directions and are
> superseded by this one:
>
> 1. The first revision read the hardware spec's pad-byte note (ch. 6) as
>    applying only to the *packed* encoding and concluded *literal sprites never
>    need a pad byte*. Its verification step ("run a literal sprite under an
>    accurate emulator") was never actually performed.
> 2. The second revision corrected (1) but then padded **every** literal line
>    with a `$00` byte, assuming pixel *value* 0 is always transparent. It is
>    not: a *normal* sprite makes **pen 0** transparent, and several sprites map
>    pixel value 0 to an opaque pen. Those sprites grew a visible stripe (a white
>    column down the lynxdemo ball, a solid white box after HUD text).
>
> This revision is measured, not reasoned: literal sprites *do* lose their last
> pixel, *and* the pad byte must resolve to **pen 0**, which is not the same as
> "pixel value 0".

## 1. The bug, precisely

Sprite image data is a per-scan-line structure. Each line begins with a 1-byte
**offset** = number of bytes from this offset byte to the next line's offset byte
(offset `0` = end of sprite). Inside a line the pixels are stored in one of two
encodings, selected by the `LITERAL` bit (SPRCTL1 bit 7):

- **Packed** (`LITERAL` clear): a bit-stream of packets, each headed by a 1-bit
  literal/run flag plus a 4-bit count, RLE-style. A header of `00000` also acts
  as an end-of-line marker.
- **Totally literal** (`LITERAL` set): raw pen-index pixels back-to-back, no
  packet headers, no `00000` marker. The line ends when the bytes named by the
  offset run out.

The hardware spec (monlynx.de/lynx/lynx6.html, "Data Packing Format") documents
the bug in terms of the **packed** encoding:

> There is a bug in the hardware that requires that the last meaningful bit of
> the data packet at the end of a scan line does not occur in the last bit of a
> byte (bit 0)... must pad this data packet with a byte of all 0s. Don't forget
> to adjust the offset to include this pad byte. Since this will only happen in
> 1/8 of the scan lines...

The spec describes only one *manifestation*. The underlying cause is Suzy's
sprite-data shift register: when it is asked for the next group of pixel bits and
that request would consume the line's data exactly to its end, it signals
end-of-line and **drops that last group**. GearLynx models this directly (its
`ShiftRegisterGetBits` returns end-of-data on `n >= remaining`, not `n >
remaining`).

Consequences by encoding:

- **Packed**: bites only when the final packet's last meaningful bit lands on bit
  0 of a byte — the spec's "1/8 of lines" case.
- **Literal**: bites on **every line**, because a literal line's pixel data
  always ends exactly on a byte boundary, so the final pixel's bits are always
  the "last group" the shift register refuses. The rightmost source pixel of
  every literal line is dropped.

### Measured proof

`samples/lynxdemo.c` draws an 8-px-wide literal ball at 2× scale, so a correct
render is 16 px wide. Booted in GearLynx and measured from the framebuffer:

| ball encoding | rendered width | meaning |
|---------------|----------------|---------|
| unpadded (`offset = 1 + data`) | **14 px** | rightmost source column dropped (×2) |
| pen-0 pad per line | **16 px** | every real pixel survives, pad invisible |

The 14-px result is the bug; the padded 16-px result is this design.

The pad *value* matters as much as its presence. While diagnosing this, the ball
briefly mapped pixel value 0 to an opaque pen (its rounded corners showed up as a
solid colour rather than transparent). With that encoding a `$00` pad measured
**18 px** — the surviving pad pixel was opaque and added a visible column. That
observation is what established the rule in §2: the pad must resolve to *pen 0*,
not to pixel *value* 0. The ball now maps value 0 to pen 0 (transparent corners),
so its `$00` pad is invisible and it measures 16 px; setbpp's bands remain a live
example of a sprite that gives value 0 an opaque pen (see §2/§3).

## 2. The fix: pad to pen 0, not to value 0

Append one trailing pad byte to each literal line and add 1 to that line's offset
byte. The pad contributes one extra source pixel for the shift register to drop
in place of real imagery.

The pad's surviving pixel(s) must resolve to **pen 0**, which a `TYPE_NORMAL`
sprite leaves transparent. Transparency on the Lynx is decided **after** the pen
palette lookup, on the resulting *pen number*, not on the raw pixel value:

- A *normal* sprite makes **pen 0** transparent (spec ch. 6.1).
- The SCB penpal maps each pixel *value* to a pen: value `2k` → high nibble of
  penpal byte `k`, value `2k+1` → low nibble. (Verified: lynxdemo's ball highlight
  is pixel value 2 and renders as pen 3 = the high nibble of penpal byte 1 = `$30`;
  its body, pixel value 1, takes the low nibble of penpal byte 0.)

So the correct pad byte is **whichever pixel value the sprite's own penpal sends
to pen 0** — commonly `$00`, but not always:

- **Standard sprites** (the usual idiom: value 0 is the transparent background)
  → value 0 already maps to pen 0 → pad byte `$00`.
- **Sprites that repurpose value 0 as an opaque colour** (setbpp's bands recolour
  value 0 per band through the penpal high nibble) → `$00` would paint a visible
  column. Pick a value that this penpal maps to pen 0 (its penpal byte 1 is `$00`,
  so pixel value 2 → pen 0): pad byte `$22`.
- **The 8×8 system font is active-low**: `build8x8` puts the draw pen in the high
  nibble of the pen byte, so pixel value 0 is the (opaque) ink and pixel value 1
  is pen 0. Its transparent pad is therefore `$FF` (all value-1 pixels), not
  `$00` — a `$00` pad paints a solid box after the string.

Why this is safe everywhere we measured:

- On GearLynx / hardware it restores the otherwise-dropped real pixel.
- The surviving pad pixel maps to pen 0 → transparent → never painted.
- For stretched sprites (setbpp's bands at 40×) the pad is the *last* source
  pixel, so it is the one the engine drops; even when it is not dropped it is
  pen 0 and invisible.

## 3. What the code does

Every hand-built sprite in the tree uses the literal encoding (`LITERAL | REHV`).
Each now carries a per-line pad byte (offset incremented to match), with the pad
*value* chosen so it resolves to pen 0 for that sprite:

| File | Sprites | Depth | value 0 maps to | Pad byte |
|------|---------|-------|-----------------|----------|
| `samples/lynxdemo.c` | `ball_img` | 4bpp | pen 0 (transparent corners) | `$00` |
| `samples/setbpp.c` | `band_img` | 4bpp | per-band pen (opaque) | `$22` |
| `samples/breakout.c` | `brick_img`, `paddle_img`, `ball_img` | 4bpp | pen 0 (transparent) | `$00` |
| `samples/invaders.c` | `inv_a/inv_b/inv_boom`, `ship_img`, bullets, bomb, bunker, ufo | 4bpp | pen 0 | `$00` |
| `samples/raycaster.c` | `solid_img`, `guard_img`, `guard_img2`, `gun_img`, `flash_img` | 4bpp | pen 0 (`PEN_NONE`) | `$00` |
| `samples/sybil.c` | `syb0..syb2(+l)`, `en_a/en_b`, `coin_img`, `blk_img` | 4bpp | pen 0 (identity penpal) | `$00` |
| `libsrc/lynx/tgi/tgi-text.s` | runtime 8×8 glyph strip (`build8x8`) | 1bpp | draw pen (active-low) | `$FF` |
| `libsrc/lynx/tgi/tgi-text5x5.s` | runtime 5×5 glyph strip (`build5x5`) | 1bpp | — | exempt (§4) |

The 8×8 text builder emits the pad at runtime: its per-row offset is `1 + len +
1`, the header loop writes `$FF` into each row's trailing pad position, and the
`text_bitmap` reservation is `8*(1+20+1)+1`.

## 4. The two documented exemptions

1. **The compact 5×5 / proportional caps fonts.** They pack glyphs at a 6-px
   pitch (5 ink bits in bits 7..3, plus a 1-px gap) into `W = ceil(N*6/8)` bytes.
   The final bit of every row is therefore always the inter-glyph gap column or
   zero byte-padding — never ink. The pixel the hardware drops is always
   transparent, so no pad byte is required. `build5x5` is left without a pad on
   purpose; the design-only `buildvar` proportional builder gets the same
   treatment when implemented.

2. **3bpp literal leftover bits (latent, not currently triggered).** The spec
   notes for literal lines: "The odd bits that may be left over at the end of the
   last byte will be painted." At 3bpp, pixels don't tile bytes evenly
   (8 / 3 = 2 px + 2 stray bits), so a literal line whose pixel count isn't a
   multiple of 8 pixels would paint 1–2 garbage pixels at the line end — a
   separate hazard from the last-pixel drop. Nothing in the tree uses `BPP_3`
   literal today; `_suzy.h` carries a caution near `BPP_3`. For 3bpp, pad each
   literal line to a whole-byte pixel count or use the packed encoding.

## 5. Forward guidance

- **Literal lines**: always append one pad byte and set the offset byte to
  `1 + dataBytes + 1`. The pad *value* must be one this sprite's penpal maps to
  **pen 0** (for the common value-0-transparent idiom that is `$00`; document the
  chosen value at the call site whenever it is not `$00`). The only sprites that
  may skip the pad are those whose rightmost source pixel is provably pen 0 on
  every line (the gap-padded fonts in §4).
- **Packed lines** (when packed authoring arrives): if the last meaningful bit of
  a line's bit-stream falls on bit 0 of a byte, append a `$00` pad byte and add 1
  to that line's offset. Independent of depth. The right long-term answer is an
  offline packer (`sprpck`-style) that emits the pad automatically.

## 6. Verification

Performed on GearLynx (`tools/gearlynx`, headless, framebuffer read-back), not on
paper:

- **lynxdemo**: ball measured 14 px unpadded → 16 px with the `$00` pad, rounded
  corners transparent (and 18 px under the discarded value-0-opaque encoding with
  a `$00` pad — the data point behind the pen-0 rule). The HUD string
  "HELLO, LYNX!" loses its trailing solid box once the text builder pads with
  `$FF` instead of `$00`.
- **setbpp**: the 4-px band sprite scaled 40× spans the full 160-px width with the
  `$22` pad (it was clipped short without it), and the four bands now show the
  pen 0..3 ramp (the body is pixel value 0, recoloured per band).
- **breakout / invaders / sybil**: full-width bricks, ships and platforms with no
  trailing-edge clipping and no stray pad pixels.
- Full rebuild (toolchain + lib + all samples) stays green.
