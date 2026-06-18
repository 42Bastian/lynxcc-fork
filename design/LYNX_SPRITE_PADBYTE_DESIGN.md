# Design: Sprite construction and the Suzy "pad-byte" bug

Scope: review of how hand-built sprites are encoded in the samples and in the
TGI text driver, against the sprite-data packing bug documented in the Lynx
hardware spec ch. 6 (monlynx.de/lynx/lynx6.html, "Data Packing Format").
**Design only** — no code is changed here; this defines what should change and
what deliberately should not.

## 1. The bug, precisely

Sprite image data is a per-scan-line structure. Each line begins with a 1-byte
**offset** = number of bytes from this offset byte to the next line's offset
byte (offset `0` = end of sprite). Inside a line the pixels are stored in one of
two encodings, selected by the `LITERAL` bit (SPRCTL1 bit 7):

- **Packed** (`LITERAL` clear): a bit-stream of packets, each headed by a
  1-bit literal/run flag plus a 4-bit count, RLE-style. A header of `00000`
  also acts as an end-of-line marker.
- **Totally literal** (`LITERAL` set): raw pen-index pixels back-to-back, no
  packet headers, no `00000` marker. The line ends when the bytes named by the
  offset run out.

The spec's bug applies to the **packed** encoding only:

> There is a bug in the hardware that requires that the last meaningful bit of
> the data packet at the end of a scan line does not occur in the last bit of a
> byte (bit 0)... must pad this data packet with a byte of all 0s. Don't forget
> to adjust the offset to include this pad byte. Since this will only happen in
> 1/8 of the scan lines...

Two tells confirm it is packed-only: the phrase "the last meaningful bit of the
**data packet**" (literal lines have no packets), and "**1/8** of the scan
lines" — a probability that only makes sense when a variable-length bit-stream
sometimes happens to end on bit 0. A literal line always ends on a byte
boundary, so if the bug applied to literal it would fire on 8/8 of lines, not
1/8.

The spec then describes literal lines as terminating purely by byte count
("converted to pixels until it runs out of bytes in that line"), with no
bit-position-sensitive end detection — so the pad-byte workaround has nothing to
act on. Conclusion: **literal sprites do not need pad bytes.**

## 2. What the code actually does

Every hand-built sprite in the tree uses the literal encoding (`LITERAL | REHV`
in SPRCTL1), with the leading offset byte equal to `1 + dataBytes` per line and
a `0` terminator:

| File | Sprites | Encoding | Depth |
|------|---------|----------|-------|
| `samples/lynxdemo.c` | `ball_img` | literal | 4bpp |
| `samples/breakout.c` | `brick_img`, `paddle_img`, `ball_img` | literal | 4bpp |
| `samples/invaders.c` | `inv_a/inv_b/inv_boom`, `ship_img`, bullets, bomb, bunker, ufo | literal | 4bpp |
| `samples/raycaster.c` | `solid_img`, `guard_img`, `guard_img2` | literal | 4bpp |
| `samples/setbpp.c` | ball | literal | 4bpp |
| `libsrc/lynx/tgi/tgi-text.s` | runtime glyph strip (`text_sprite`, ctl `$00,$90`) | literal | 1bpp |

All depths in use are 1bpp and 4bpp, both of which divide 8 evenly, so each
whole data byte is a whole number of pixels and there are never leftover bits.

**Finding: none of these are subject to the pad-byte bug. No sprite-data changes
are required.** This is consistent with the provenance of the text driver, which
is derived from Karri Kaksonen's hardware-tested cc65 Lynx TGI driver and has
always built literal glyph strips without pad bytes.

## 3. The one stale spot: the text driver's "fill-byte" experiment

`tgi-text.s` is the only place that still carries doubt about this. It reserves
room for a per-row fill byte and leaves a comment that the bug status is
unknown:

```
; 8 rows with (one offset-byte plus 20 character bytes plus one fill-byte)
; plus one 0-offset-byte.
; (As an experiment, the fill-byte isn't being generated.
;  It might not be needed to work around a Suzy bug.)
text_bitmap:    .res    8*(1+20+1)+1
```

The build loop already commits to "no fill byte": the extra `iny` that would
have widened the offset is commented out, the `lda #$FF / sta text_bitmap-1,x`
fill write is commented out, and each row's offset byte is written as
`STRLEN + 1` (offset byte + glyph bytes only). So the generated data is correct
literal data; only the reservation and the comment lag behind.

Design changes (cosmetic + 8 bytes of BSS, no behaviour change):

1. Resolve the comment from "experiment / might not be needed" to a definite
   statement: literal sprites are exempt from the packed-format pad-byte bug
   (spec ch. 6), so no fill byte is emitted. Cross-reference
   `LYNX_SPRITE_PADBYTE_DESIGN.md` §1.
2. Shrink the reservation to match what is generated:
   `text_bitmap: .res 8*(1+20)+1` — reclaiming the 8 never-written fill bytes.
   This is the only buffer-size dependency; confirm the loop terminator
   (`stz text_bitmap,x` after the 8th row) still lands inside the buffer with
   the tightened size (it does: last index = 8*(1+20) = 168, terminator at 168,
   buffer length 169).
3. Delete the two commented-out fill-byte lines so future readers don't
   resurrect the doubt.

Keeping the byte is also acceptable as a pure safety margin, but then the
comment must stop calling it an open question — pick one.

## 4. Latent hazard to document (not currently triggered): 3bpp literal

The spec notes for literal lines: "The odd bits that may be left over at the end
of the last byte will be painted." At 3bpp, pixels don't tile bytes evenly
(8 bits / 3 = 2 pixels + 2 stray bits), so a literal line whose pixel count
isn't a multiple of 8 pixels would paint 1–2 garbage pixels at the line end.
Nothing in the tree uses `BPP_3` literal today, but `tgi_setbpp` and the SCB
structs expose 1/2/3/4bpp. Design recommendation: add a one-line caution to the
header near `BPP_3` and to the sprite section of `LYNX_TGI_DESIGN.md` — for
3bpp, either pad each literal line to a whole-byte pixel count or use the packed
encoding.

## 5. Forward guidance: when packed sprites do arrive

The pad-byte rule is dormant only because nothing here is packed yet. To keep it
from biting whoever first hand-authors or imports a packed sprite, the design
adds a short, authoritative reference rather than leaving it tribal knowledge:

1. A new subsection in `LYNX_TGI_DESIGN.md` §5 ("Hardware perniciousness
   rules"), stating the rule and its scope:
   - Packed lines: if the last meaningful bit of a line's bit-stream falls on
     bit 0 of a byte, append a `$00` pad byte **and** add 1 to that line's
     offset byte. Independent of depth.
   - Literal lines: no pad byte ever; just ensure the offset byte = `1 + data
     bytes` and, for 3bpp, a whole-byte pixel count.
2. A one-line pointer comment in `_suzy.h` next to the `LITERAL` define.
3. If packed authoring becomes common, the right long-term fix is to lean on an
   offline packer (`sprpck`-style) that emits the pad byte automatically, rather
   than hand-encoding — call this out as a follow-up, not part of this change.

## 6. Summary of proposed changes

| # | Change | File(s) | Risk |
|---|--------|---------|------|
| 1 | Clarify comment, shrink reservation, drop dead fill-byte lines | `libsrc/lynx/tgi/tgi-text.s` | Low (cosmetic + 8B BSS) |
| 2 | Caution note for 3bpp literal leftover bits | `include/_suzy.h`, `LYNX_TGI_DESIGN.md` | None (doc) |
| 3 | Packed-vs-literal pad-byte rule in design doc + header pointer | `LYNX_TGI_DESIGN.md` §5, `include/_suzy.h` | None (doc) |
| — | Sample sprite data (`*.c`) | — | **No change** — all literal, all exempt |

## 7. Verification (when implemented)

- Build all samples + a text-drawing program; byte-diff the generated
  `text_bitmap` offsets are unchanged by the reservation shrink (only trailing
  unused bytes differ).
- Run one literal-text and one 4bpp-sprite sample under the emulator and confirm
  no trailing-edge artifacts — this also closes the "emulator run pending" item
  already tracked for these samples.
- Optional: author a throwaway packed sprite that deliberately ends on bit 0,
  with and without the pad byte, to confirm the rule on the emulator before
  documenting it as load-bearing.
