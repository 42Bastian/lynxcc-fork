<!--
SPDX-License-Identifier: CC-BY-4.0
Lynx Game Development SDK documentation, (c) 2026 the lynxcc authors,
licensed under Creative Commons Attribution 4.0 International.
See doc/licenses.html.
-->

# Design: a dedicated `doc/sprites.html` Reference page

**Status: proposed (design only).** This document is the source-of-truth outline
for a new `doc/sprites.html` Reference page. Nothing is built yet. When the page
is written it must track the header it documents (`include/lynx/_suzy.h`) under
the `CLAUDE.md` docs-track-code rule, the way `design/LYNX_GFX_DOC_PAGE_DESIGN.md`
is the source of truth for `graphics.html`.

The reference material this page is grounded in is 42Bastian Schick's Lynx
hardware notes, chapter 6 "Sprite/Collision"
(<https://www.monlynx.de/lynx/lynx6.html>). The page re-expresses that hardware
description **through the concrete C types the SDK actually ships** — the SCB
structs, penpal structs, and `SPRCTL0` / `SPRCTL1` / `SPRSYS` / `SPRGO` defines
in `include/lynx/_suzy.h` — rather than re-narrating the raw register spec.

## 1. Problem

The Suzy sprite engine is the single most important thing to understand when
writing a Lynx game — *all* drawing goes through it (`graphics.html` §1 already
states there is no pixel-plot primitive; a shape is a sprite). Yet the SDK has no
page that explains how a sprite is actually structured in memory:

- `include/lynx/_suzy.h` declares **eight** SCB struct variants
  (`SCB_REHVST_PAL`, `SCB_REHVST`, `SCB_REHVS_PAL`, `SCB_REHVS`, `SCB_REHV_PAL`,
  `SCB_REHV`, `SCB_RENONE_PAL`, `SCB_RENONE`) and **four** penpal structs
  (`PENPAL_4` … `PENPAL_1`), plus the `SPRCTL0` / `SPRCTL1` bit-field defines
  (`BPP_*`, `HFLIP`, `VFLIP`, `TYPE_*`, `LITERAL`/`PACKED`, `REHV*`/`RENONE`,
  `REUSEPAL`, `SKIP`, `DRAWUP`, `DRAWLEFT`). None of this is documented for the
  user anywhere. A programmer reading the header cannot tell *why there are eight
  SCB types*, when to pick which, or how the struct field list relates to the
  reload bits in `SPRCTL1`.
- `doc/collisions.html` already exists and owns the **collision** half of chapter
  6: the collision buffer, collision numbers, the depository, `SPRCOLL`, the
  per-sprite / per-system `NO_COLLIDE` opt-outs, and `COLLOFF`. It deliberately
  treats sprite *painting* as assumed background. So the collision side has a
  home; the sprite-structure side does not.
- `doc/graphics.html` §3.8 ("Sprites, collision, and going deeper") is explicitly
  a short pointer section — it links out to "the sprite pad-byte rule" and the
  packtest example but has nowhere to point for the SCB structure itself.
- `doc/sp65.html` documents the packed-vs-literal *data* format (with a figure)
  and `design/LYNX_SPRITE_PADBYTE_DESIGN.md` documents the last-pixel pad-byte
  bug, but neither explains the SCB that points at that data.

So the layer between "here is a blob of packed pixel data" (sp65) and "collisions
are detected against the collision buffer" (collisions.html) — namely *the sprite
control block, its linking, its type byte, and its pen palette* — is undocumented.
That layer is exactly `_suzy.h`, and it is what this page owns.

This mirrors the recent `comlynx.html` and `graphics.html` splits, where a
central Suzy/Mikey topic was promoted into its own Reference page with proper
hardware description and SVG diagrams.

## 2. Goal

A single Reference page, `doc/sprites.html`, titled **"Sprites"**
(`<title>` = `lynxcc - Sprites`), that documents the Lynx hardware sprite engine
as `_suzy.h` exposes it:

1. The hardware sprite model — Suzy paints, in real time, from packed RAM data
   into the frame buffer; unlimited vertical size, ≤254 source bytes wide,
   1–4 bpp, hardware scaling, flip, clip, and per-sprite skip.
2. **The SCB** — the fixed 5-byte head, the reloadable position/size/stretch/tilt
   block, and the pen palette, mapped field-by-field onto the `_suzy.h` structs.
3. **SCB linking** — the `next` pointer, painter's order, `0` termination, and
   many-SCBs-to-one-data-block sharing.
4. **The eight SCB struct variants and the reload bits** — how `RENONE` / `REHV`
   / `REHVS` / `REHVST` and `REUSEPAL` in `SPRCTL1` decide which trailing fields
   the hardware reloads, and how each `_suzy.h` struct is the exact C shape of one
   of those choices.
5. **Sprite types** — the eight `TYPE_*` values in `SPRCTL0`, their pen-0 / pen-E
   / pen-F transparency-and-collision semantics, and the shadow-inverter hardware
   bug.
6. **The pen palette ("penpal")** — pen-index → pen-number nibble mapping,
   the `2^bpp`-nibble length rule that gives `PENPAL_4`…`PENPAL_1` their sizes,
   and how `REUSEPAL` lets consecutive sprites share a palette.
7. **Pixel & data format** — 2 pixels/byte MSB-first, packed vs `LITERAL`, and a
   pointer to the pad-byte rule and sp65.
8. **Sizing, stretch, tilt, flip, quadrants** — `hsize`/`vsize`, the `stretch`
   and `tilt` adders, `HFLIP`/`VFLIP`/`DRAWUP`/`DRAWLEFT`, the reference point and
   starting quadrant, and the `HSIZEOFF`/`VSIZEOFF` bump-balancing offsets.
9. **Driving the engine** — the `struct __suzy` registers and the initialise →
   build SCBs → `SPRGO` sequence, framed as "what `gfx_sprite` does for you".

The page follows `design/DOC_STRUCTURE_DESIGN.md` (shared `<site-nav>` /
`<site-foot>` chrome) and `design/DOC_SVG_STYLE_DESIGN.md` for every diagram.

**Scope boundary with `collisions.html`.** Sprites.html describes *what the third
SCB byte is* (`sprcoll`) and the collision-relevant columns of the sprite-type
table, then links to `collisions.html` for the buffer, depository, numbering
scheme, and `COLLOFF`. It does not re-document those. Conversely the sprite-type
opt-out material in `collisions.html` §3 links back to this page's type table.

## 3. Page outline

Section numbering is the doc-set convention (`sect-N`, `sect-N-M` ids for the
search index and quick-jump palette).

### 3.1 The hardware sprite model (§1)

State the model as facts, from chapter 6 §6.1:

- Suzy paints pixel data into the frame buffer; the CPU never plots pixels. The
  "sprite" abstraction is software's view of a hardware painter.
- Sprites are **sized in real time** as they paint (reduce or enlarge, H and V
  independently) and their source data is stored **compacted** in RAM.
- Limits: unlimited vertical size; horizontal source ≤ **254 bytes** (≈ 508
  unscaled pixels); **1, 2, 3, or 4 bpp** of pen-index. Off-screen pixels are
  clipped by hardware. Sprites can be H/V flipped about their reference point and
  individually **skipped**.
- Neither the SCB nor sprite data may live in Mikey ROM; both are pointer-reached
  and may sit anywhere else in RAM.
- One-paragraph orientation: in the SDK you rarely start the engine by hand —
  `gfx_sprite` (see `graphics.html`) builds the register writes and pulls `SPRGO`.
  This page explains the data structure `gfx_sprite` consumes, and what you are
  building when you author an SCB directly (as `spritetest.c`, `packtest.c`,
  `spriteslice.c`, `spritesheet.c`, `collision.c` do).

### 3.2 The sprite control block (§2)

The core. Present the SCB as three regions, keyed to `_suzy.h`:

- **Fixed 5-byte head** (always present, always reloaded):
  `sprctl0` ($FC80 semantics), `sprctl1` ($FC81), `sprcoll` ($FC82), `next`
  (16-bit pointer to next SCB), `data` (16-bit pointer to the sprite data block).
- **Reloadable body** (present only per the `SPRCTL1` reload bits, §3.4):
  `hpos`, `vpos` (signed screen position), `hsize`, `vsize` (unsigned 8.8),
  `stretch`, `tilt` (16-bit adders).
- **Pen palette** — `penpal[]`, a *separately* reloadable block (§3.6).

Show the byte order in a diagram (Diagram A) and give the exact `_suzy.h`
declaration alongside, e.g. `SCB_REHVST_PAL` as the "all fields present" shape:

```c
typedef struct SCB_REHVST_PAL {   /* SCB with all attributes */
  unsigned char sprctl0;          /* SPRCTL0: bpp, flip, type            */
  unsigned char sprctl1;          /* SPRCTL1: pack, reload, reuse, skip  */
  unsigned char sprcoll;          /* collision number + don't-collide    */
  char *next;                     /* -> next SCB, 0 = last               */
  unsigned char *data;            /* -> packed sprite data               */
  signed int hpos, vpos;          /* screen position                     */
  unsigned int hsize, vsize;      /* 8.8 size                            */
  unsigned int stretch, tilt;     /* per-scanline adders                 */
  unsigned char penpal[8];        /* pen-index palette (4bpp = 8 bytes)  */
} SCB_REHVST_PAL;
```

Note the optional software-only trailers the hardware ignores (id byte, Z depth,
previous-SCB back-pointer, and the collision-depository byte whose offset is
`COLLOFF` — that byte's placement is documented on `collisions.html`).

### 3.3 SCB linking and painter's order (§2.1)

- SCBs form a singly linked list via `next`; the engine follows it until `next`
  is **0** (last SCB). Diagram B: three SCB boxes chained, last `next = 0`, two of
  them pointing at the *same* `data` block to show sharing.
- **Painter's order**: earlier SCBs paint first and are overwritten by later ones
  — so the list is back-to-front in Z. This is also the order that makes collision
  numbers meaningful (link to `collisions.html`).
- One SCB is required per on-screen occurrence; many SCBs may share one data block
  (e.g. a tiled wall). `spritesheet.c` / `spriteslice.c` are the worked references.
- `SKIP` (`SPRCTL1` bit) mostly skips a sprite but the engine still reads its first
  5 bytes, so `next` must stay valid — the list is not broken by a skip.

### 3.4 The eight struct variants and the reload bits (§2.2)

The key insight that makes `_suzy.h` legible. The `SPRCTL1` reload field selects
which trailing fields the hardware pulls from this SCB; anything not reloaded
keeps the value left in Suzy's registers by the previous sprite. `REUSEPAL`
independently drops the pen palette. Each `_suzy.h` struct is the exact C shape of
one legal combination:

| `SPRCTL1` bits            | Reloaded body fields          | penpal | `_suzy.h` struct |
|---------------------------|-------------------------------|--------|------------------|
| `REHVST` (`0x30`)         | hpos,vpos,hsize,vsize,stretch,tilt | yes | `SCB_REHVST_PAL` |
| `REHVST` + `REUSEPAL`     | hpos,vpos,hsize,vsize,stretch,tilt | no  | `SCB_REHVST`     |
| `REHVS` (`0x20`)          | hpos,vpos,hsize,vsize,stretch      | yes | `SCB_REHVS_PAL`  |
| `REHVS` + `REUSEPAL`      | hpos,vpos,hsize,vsize,stretch      | no  | `SCB_REHVS`      |
| `REHV` (`0x10`)           | hpos,vpos,hsize,vsize               | yes | `SCB_REHV_PAL`   |
| `REHV` + `REUSEPAL`       | hpos,vpos,hsize,vsize               | no  | `SCB_REHV`       |
| `RENONE` (`0x00`)         | hpos,vpos only                     | yes | `SCB_RENONE_PAL` |
| `RENONE` + `REUSEPAL`     | hpos,vpos only                     | no  | `SCB_RENONE`     |

- Every variant always carries the 5-byte head and `hpos`/`vpos`; the ladder is
  purely about how much of the size/stretch/tilt tail and the palette travels with
  *this* SCB. Shorter SCBs paint faster and cost less RAM; use them for a run of
  same-size sprites after a full one has loaded the registers.
- `REUSEPAL` = "reuse the previous sprite's pen palette", i.e. pick the non-`_PAL`
  struct. Practical rule: the first sprite of a group carries a `_PAL` struct;
  followers that share its palette use the matching non-`_PAL` struct with
  `REUSEPAL` set. Warn that reusing a palette that was never loaded (e.g. the very
  first sprite) is undefined.
- `ALGO3` (`0x40`) and the draw-direction bits `DRAWUP`/`DRAWLEFT` also live in
  `SPRCTL1`; cover the draw bits under §3.8 (flip/quadrant) and note `ALGO3` as a
  legacy packing-algorithm select left at 0 for the standard format.
- Diagram C: the reload ladder as four nested brackets over the SCB byte layout,
  with the `_PAL` / non-`_PAL` split shown as the palette block detaching.

### 3.5 Sprite types (§3)

The eight `TYPE_*` values in the low bits of `SPRCTL0`, from chapter 6 §6.2.
Present the SDK name, the value, and the pen semantics:

| `SPRCTL0` name    | val  | Pen behaviour (summary)                               |
|-------------------|------|-------------------------------------------------------|
| `TYPE_NORMAL`     | 0x04 | pen 0 transparent + non-collideable; 1–F opaque       |
| `TYPE_BOUNDARY`   | 0x03 | normal, but pen F transparent yet still collideable   |
| `TYPE_SHADOW`     | 0x07 | normal, but pen E non-collideable yet still opaque     |
| `TYPE_BSHADOW`    | 0x02 | boundary + shadow combined                            |
| `TYPE_BACKGROUND` | 0x00 | overwrites video+collision; pen 0 & F opaque; no read  |
| `TYPE_BACKNONCOLL`| 0x01 | background, no collision-buffer activity at all       |
| `TYPE_NONCOLL`    | 0x05 | never touches collision buffer; pen F non-collideable  |
| `TYPE_XOR`        | 0x06 | normal, but video data XORed with sprite data          |

- Reproduce the hardware truth table (F-opaque / E-collideable / 0-opaque /
  allow-collision-detect / allow-buffer-access / XOR) as Diagram/Table E, and
  carry the **shadow-inverter bug** note verbatim in spirit: the missing inverter
  forces shadow on for XOR and background sprites, so pen E is non-collideable
  there — important when using a background sprite to clear the collision buffer
  (pen E won't clear its cell) — cross-linking `collisions.html`.
- Note the timing hints from the spec (background ≈ ¼ faster since no read;
  background-non-collide ≈ ⅓ faster; non-collideable ≈ ¼ faster; XOR up to ¼
  slower) as qualitative guidance, not measured cycles.
- `sprcoll` (byte 3) is introduced here only as far as "holds the 4-bit collision
  number plus a don't-collide bit"; everything else defers to `collisions.html`.

### 3.6 The pen palette (§4)

Chapter 6 §6.4/§6.5, expressed via the `PENPAL_*` structs:

- Each sprite carries its own **pen-index → pen-number** map: a pixel's pen index
  (0..2^bpp−1, from the packed data) indexes the palette to yield a 4-bit **pen
  number** (0..F), which then indexes the display palette (documented on
  `graphics.html`). Two indirections: sprite penpal, then the 16-entry display
  palette.
- **Length rule**: the palette is `2^bpp` nibbles, packed 2 nibbles/byte, so
  `1bpp → 1 byte`, `2bpp → 2 bytes`, `3bpp → 4 bytes`, `4bpp → 8 bytes`. This is
  exactly `PENPAL_1` (1), `PENPAL_2` (2), `PENPAL_3` (4), `PENPAL_4` (8) in
  `_suzy.h`. The generic SCB structs declare `penpal[8]`; when hand-building a
  low-bpp sprite you only populate/emit the first `2^(bpp-1)` bytes, and the SCB
  is that many bytes shorter.
- Nibble order (§6.5): pen index 0's pen number is the **upper** nibble of the
  first penpal byte. Show this in Diagram D beside the pixel-packing diagram.
- The palette is a **separately reloadable block** from the SCB body — this is why
  `REUSEPAL` can drop it independently (§3.4). Example: `packtest.c`'s "identity
  penpal `{0x01,0x23,…,0xEF}`" (pen index k → pen k) and `spritetest.c`'s
  transparent-value-0 penpal.

### 3.7 Pixel and data format (§5)

- Frame-buffer packing (§6.5): 2 pixels per byte, the pixel in the **high** nibble
  paints left (on an un-flipped, right-drawing sprite); bytes ascend left→right.
- Sprite *source* data is separately packed (offsets + packed/literal runs). This
  page states the distinction and the `LITERAL` vs `PACKED` (`SPRCTL1`) choice,
  then links the detail out: the packed-vs-literal figure and encoding on
  `sp65.html`, and the **last-pixel pad-byte bug** on
  `design/LYNX_SPRITE_PADBYTE_DESIGN.md` / the `sprctl1` doc comment in `_suzy.h`.
  Do not re-derive the packing format here (sp65 owns it); do restate the
  pad-byte's user-visible rule because it is a sprite-authoring gotcha:
  literal lines always need a trailing pad byte whose pixels resolve to pen 0.
- `packtest.c` is the reference that packed == literal output at all four bpp.

### 3.8 Sizing, stretch, tilt, flip, quadrants (§6)

- `hsize`/`vsize` are 8.8 fixed-point scale (256 = 1×). Hardware scales during
  paint; H and V independent.
- `stretch` (`SPRCTL1` reload-gated) is added to `hsize` each scanline — sub-unit
  to many-unit (1/256 unit … 128 units), wraps past 128 units into a size
  reduction. Given its own bullet from the vertical-stretch mode.
- **Vertical stretch** gets its own bullet and a dedicated figure (Diagram G):
  `VSTRETCH` (`SPRSYS` bit `0x10`) feeds the *same* `stretch` value into `vsize`
  too, so the sprite grows in both axes each scanline (uniform zoom, not a
  flat-topped trapezoid). It is a **global mode bit**, not an SCB field — stays set
  until cleared, affects every sprite while on; the new vsize takes effect at the
  next source-line fetch. There is no separate vertical adder.
- `tilt` is added (via the tilt accumulator, integer part) to `hpos` each
  scanline — shears the sprite; positive tilts right; combine with stretch for
  arbitrary quads.
- Flip and direction: `DRAWUP`/`DRAWLEFT` (`SPRCTL1`) set the starting draw
  quadrant. The reference point is fixed at compaction time; the **starting quadrant
  is runtime-changeable** and shifts position when changed. Diagram F: reference
  point at centre, the four quadrants, flip axes.
- **§6.1 Flipping about the reference point** is a dedicated subsection (`sect-6-1`)
  covering `HFLIP`/`VFLIP` (`SPRCTL0` `0x20`/`0x10`): each mirrors the paint
  direction left↔right / up↔down about the reference point (not the centre or a
  screen axis), so `hpos`/`vpos` are untouched; both together = 180° rotation; flip
  is independent of size/stretch/tilt and of the draw quadrant. Diagram H: the same
  asymmetric `F` glyph in the four flip states about a fixed reference point.
- `HSIZEOFF`/`VSIZEOFF` (`hsizeoff`/`vsizeoff` in `struct __suzy`, set at init to
  the "magic" `$007F` on the right/down directions, `0` left/up) balance the
  one-pixel visual bump at the reference point of a scaled multi-quadrant sprite;
  they are common to all sprites — restore them if you change them.
- `spriteslice.c` (per-scanline hsize) and `raycaster.c` (scaled wall slices) are
  the worked references.
- **§6.2 Panning the display window: hoff and voff** is a dedicated subsection
  (`sect-6-2`) covering `hoff`/`voff` (`$FC04`/`$FC06` in `struct __suzy`): `hpos`/`vpos`
  are display-*world* coordinates, and `hoff`/`voff` fix the top-left corner of the
  160×102 display window within that world, so a sprite draws at buffer pixel
  (`hpos−hoff`, `vpos−voff`) and off-window pixels clip. `gfx_init` sets both to 0
  (making `hpos`/`vpos` plain screen coordinates); their purpose is scrolling —
  because the offsets are subtracted from every sprite, changing them pans the whole
  scene at once with no per-SCB edits. 16-bit/global/next-render latch. Diagram I:
  display world holding sprites, the offset window rectangle, the (hpos−hoff,vpos−voff)
  mapping, and a panned second window position.

### 3.9 Driving the engine (§7)

Grounded in the `struct __suzy` register block in `_suzy.h`:

- One-time init (chapter 6 §6.8): `sprsys` control bits, `sprinit`, `hoff`/`voff`
  (screen edge offsets), `colbase` (collision buffer base — see collisions.html),
  `colloff`, `hsizeoff`/`vsizeoff`, and `suzybusen = 1` to give Suzy the bus.
- Per-frame start: `vidadr`/`sprbase` (video build buffer), `scbnext` = first SCB
  address, then `sprgo = SPRITE_GO` (`0x01`, or `EVER_ON|SPRITE_GO` = `0x05`),
  ack sleep, and let Suzy take the bus.
- `sprsys` read side (`SPRITEWORKING`, `SPRITETOSTOP`, `UNSAFE_ACCESS`,
  `VSTRETCHING`, `LEFTHANDED`, `MATHWORKING`/`MATHWARNING`/`MATHCARRY` …) — how to
  tell the engine is running / has stopped, plus the read-back of the `VSTRETCH` and
  `LEFTHAND` mode bits (and the math-status bits). Write bits `SPRITESTOP` /
  `CLR_UNSAFE` / `NO_COLLIDE` / `VSTRETCH` / `LEFTHAND`. The engine only restarts
  from the beginning; it cannot resume mid-list.
- `LEFTHAND` (`SPRSYS` `0x08`) selects the console's drawing handedness: it reverses
  Suzy's video-buffer fill direction so the image is correct in the flipped
  orientation, and pairs with the `JOYSTICK` up/down + left/right axis swap. Global
  console-orientation setting written once at init (leave as `gfx_init` sets it
  unless implementing screen flip); `LEFTHANDED` reads it back.
- Frame this whole section as "what `gfx_sprite` and `gfx_init` do for you"
  (link `graphics.html`), so the reader knows they normally don't hand-write these
  writes, but can when they need a custom sprite loop.
- Note the `SPRSYS` `NO_COLLIDE`, `SIGNMATH`/`ACCUMULATE` (the last two are Suzy
  hardware-math bits, not sprites — point at `suzymath.h`/funcref, do not expand).

### 3.10 Diagrams

All per `design/DOC_SVG_STYLE_DESIGN.md`: `viewBox="0 0 720 H"`, theme CSS
variables only, mono for bytes/registers, sans for labels, each in
`<figure>`/`<figcaption>`.

- **Diagram A — SCB byte layout.** The fixed 5-byte head, the reloadable body, and
  the detachable penpal block, with byte offsets and the `_suzy.h` field names.
- **Diagram B — SCB linked list.** Three SCBs chained by `next`, last `next = 0`,
  two sharing one `data` block; painter's-order arrow (back→front).
- **Diagram C — reload ladder.** Nested brackets (`RENONE` ⊂ `REHV` ⊂ `REHVS` ⊂
  `REHVST`) over the body, plus the `_PAL` / `REUSEPAL` palette-detach split;
  labels tie each bracket to its `_suzy.h` struct.
- **Diagram D — pen index → pen number & pixel packing.** Left: two-pixels-per-byte
  MSB-first with high-nibble-paints-left; right: pen index into the penpal nibbles
  (index 0 = upper nibble of byte 0) → 4-bit pen number → display palette.
- **Table/Diagram E — sprite-type truth table.** The eight `TYPE_*` rows × the
  hardware columns, with the shadow-inverter-bug footnote.
- **Diagram F — reference point, quadrants, flip.** Reference pixel, four paint
  quadrants, `HFLIP`/`VFLIP` axes, `DRAWUP`/`DRAWLEFT` starting-quadrant note.
- **Diagram G — vertical stretch.** Two panels: `stretch` alone (flat-topped
  trapezoid, fixed height) vs `stretch` + `VSTRETCH` (grows in width and height,
  uniform zoom), each over the dashed 1× base sprite.
- **Diagram H — flip states.** The same asymmetric `F` glyph in four cells (normal,
  `HFLIP`, `VFLIP`, `HFLIP|VFLIP`) mirrored about a fixed reference point, with the
  mirror axes dashed through the reference dot.
- **(Reuse, don't redraw)** the packed-vs-literal data figure — link
  `sp65.html`'s existing one rather than duplicating it.

## 4. Content sourcing (facts and where they come from)

| Fact | Source in tree |
|------|----------------|
| SCB field order, 8 struct variants, penpal structs | `include/lynx/_suzy.h` (structs) |
| `SPRCTL0` bits: `BPP_*`, `HFLIP`, `VFLIP`, `TYPE_*` | `_suzy.h` `$FC80` defines |
| `SPRCTL1` bits: `LITERAL`/`PACKED`, `ALGO3`, `REHV*`/`RENONE`, `REUSEPAL`, `SKIP`, `DRAWUP`, `DRAWLEFT` | `_suzy.h` `$FC81` defines |
| `SPRGO` (`SPRITE_GO`, `EVER_ON`), `SPRSYS` read/write bits | `_suzy.h` `$FC91`/`$FC92` defines |
| Register block (hoff/voff, sprbase, colbase, scbnext, colloff, hsizeoff/vsizeoff, sprinit, suzybusen, sprgo, sprsys) | `_suzy.h` `struct __suzy` |
| `2^bpp`-nibble penpal length ↔ `PENPAL_1..4` sizes | derived from `_suzy.h` struct sizes + chapter 6 §6.4 |
| Reload-bit ↔ struct-variant mapping table (§3.4) | derived from `_suzy.h` struct fields + `SPRCTL1` reload bits |
| Sprite model, limits, types truth table, shadow bug, init sequence, painter's order, packing/pad-byte, quadrants, stretch/tilt/size-offset | chapter 6 <https://www.monlynx.de/lynx/lynx6.html> |
| Pad-byte user rule | `design/LYNX_SPRITE_PADBYTE_DESIGN.md`, `_suzy.h` `sprctl1` comment |
| Packed-vs-literal data encoding (linked out) | `doc/sp65.html`, `design/LYNX_SPRITE_SHEET_DESIGN.md` |
| Collision buffer / depository / numbers / `COLLOFF` / opt-outs (linked out) | `doc/collisions.html` |
| Worked SCBs | `examples/suzy/{spritetest,packtest,spriteslice,spritesheet,collision}.c`, `examples/games/raycaster.c` |

The only *derived* content is the §3.4 reload-ladder table and the penpal-length
rule; both are mechanical consequences of the `_suzy.h` declarations and the
reload-bit semantics, and must be re-checked against `_suzy.h` if those structs or
defines ever change.

## 5. Integration with the existing doc set

1. **Nav (`doc/doc.js`, `TOPBAR_HTML`).** Add a Reference-dropdown entry next to
   Graphics and Collision detection:
   `<a href="sprites.html"><span>Sprites</span><span class="tdesc">SCBs, types, penpal</span></a>`.
   Per `DOC_STRUCTURE_DESIGN.md` the active-page highlight derives from the
   filename automatically.
2. **Search index (`doc/doc.js` section-index array + `gen-search-index.py`).**
   Register each `sect-N` / `sect-N-M` id (SCB, linking, reload variants, types,
   penpal, data format, sizing, driving the engine) following the existing
   `["page.html","sect-id","Title"]` rows.
3. **Cross-links out.** Sprites.html links to `collisions.html` (collision half),
   `graphics.html` (`gfx_sprite`, display palette, `gfx_init`), `sp65.html`
   (packing / sprite-sheet build), `funcref.html` (`gfx_sprite`, Suzy math), and
   `design/LYNX_SPRITE_PADBYTE_DESIGN.md`.
4. **Cross-links in.** Point `graphics.html` §3.8 and `collisions.html`
   (its "sprite painting is assumed" intro + §3 type opt-outs) *at* this page;
   add a "see Sprites" pointer from `sp65.html` (which builds the data the SCB
   references) and from `samples.html` next to the suzy examples.
5. **`index.html`.** Add a Sprites card/link alongside the other Reference pages
   if the landing page lists them.
6. **License header.** `sprites.html` carries the same `CC-BY-4.0` SPDX comment as
   the other doc pages (per `LYNX_LICENSE_POLICY_DESIGN.md`).

## 6. Docs-track-code obligations

This is a new documentation surface, so the standing `CLAUDE.md` rule extends to
it: any future change to an SCB struct, a `SPRCTL0`/`SPRCTL1`/`SPRSYS`/`SPRGO`
define, a `PENPAL_*` struct, or the `struct __suzy` register layout in `_suzy.h`
must update `sprites.html` in the same pass, alongside the `_suzy.h` doc comments,
`funcref.html` where relevant, and any affected diagram. Record `sprites.html` in
this design doc's touch list when those symbols are edited. Because the §3.4 table
and §3.6 length rule are *derived* from `_suzy.h`, they are the most fragile parts
and must be re-verified on any `_suzy.h` change.

## 7. Explicitly out of scope

- Writing `sprites.html` itself (this is design only).
- Any code or API change — the page documents `_suzy.h` exactly as it is.
- The collision buffer, depository, collision numbering, `COLLOFF`, and the
  `NO_COLLIDE` opt-outs — those stay on `collisions.html`; sprites.html only names
  `sprcoll` and links across.
- The packed/literal *data* encoding detail and the sprite-sheet build pipeline —
  those stay on `sp65.html`; sprites.html links out and only restates the
  user-facing pad-byte rule.
- The `gfx_*` display API, palette wire format, and double buffering — those stay
  on `graphics.html`.
- Suzy hardware math (`FACTOR_*`, `PRODUCT`, divide) — `_suzy.h` declares it but it
  is a `suzymath.h`/funcref topic; sprites.html only notes the shared `SPRSYS`
  math bits exist.

## 8. Implementation order (when built)

1. Scaffold `sprites.html` from an existing Reference page (`collisions.html` is
   the closest sibling: same chapter-6 lineage, `<site-nav>` chrome, SVG
   diagrams).
2. Fill §1–§2 (model + SCB structure) from `_suzy.h`; author Diagram A.
3. Fill §2.1–§2.2 (linking + reload variants); author Diagrams B and C — the
   reload-ladder table is the page's centrepiece, get it right against `_suzy.h`.
4. Fill §3 (types) and author Table/Diagram E; wire the cross-links to
   `collisions.html`.
5. Fill §4–§5 (penpal, data format); author Diagram D; link `sp65.html` and the
   pad-byte design.
6. Fill §6–§7 (sizing/stretch/tilt/flip and driving the engine); author Diagram F.
7. Register nav + search-index entries; add the inbound cross-links from
   `graphics.html`, `collisions.html`, `sp65.html`, `samples.html`; verify
   quick-jump and the light/dark rendering of every diagram.
