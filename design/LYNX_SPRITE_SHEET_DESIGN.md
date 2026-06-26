<!--
SPDX-License-Identifier: CC-BY-4.0
Lynx Game Development SDK documentation, (c) 2026 the lynxcc authors,
licensed under Creative Commons Attribution 4.0 International.
See doc/licenses.html.
-->

# Design: Sprite sheets — one source image, many Lynx sprites

Scope: how a game can author several sprite frames (animation cels, tile
variants, a font of glyphs) as a **single sheet image** and have them turned
into Lynx sprites, instead of carrying one `.pcx`/`.png` file per frame. This
document is the source of truth for the sprite-sheet workflow.

> **Status — implemented 2026-06-26.** Both routes ship: the manual
> `--slice`/`--pop` chain (§4) and the `--sprite-sheet` driver (§5A). Byte
> neutrality and on-hardware pixel parity were measured on GearLynx (§7). The
> as-built `--sprite-sheet` interface is a single self-contained option carrying
> all attributes (see §5A), not the `-c`/`-w` split the first draft sketched;
> the reason is recorded in §5A.

> **TL;DR.** A runtime sprite sheet — one big Suzy sprite that the SCB indexes
> into — is **impossible** on this hardware (§2). The right place to "unpack" a
> sheet is **build time**. sp65 already has the primitive for it (`--slice`,
> §3), so sheets work *today* through a verbose Makefile chain (§4). The
> proposed work (§5) is a thin sp65 convenience layer that turns one sheet image
> plus a grid description into **one** generated header exposing an indexable
> **frame-pointer table**, leaving the on-cart bytes identical to the N separate
> sprites they replace.

## 1. How sprites work today

The pipeline is one image → one sprite:

1. An indexed image (`heart.pcx`) holds a single frame.
2. The Makefile runs `sp65` once per encoding:

   ```
   sp65 -r heart.pcx -c lynx-sprite,mode=packed -w heart_packed.h,format=c,ident=heart_packed
   ```

   `compiler/sp65/lynxsprite.c` (`GenLynxSprite`) walks the bitmap scan line by
   scan line and emits the Lynx sprite-data block — a per-line, byte-counted
   RLE/literal structure, optionally split into four quadrants around an action
   point (see `design/LYNX_SPRITE_PADBYTE_DESIGN.md` for the encoding).
3. `compiler/sp65/c.c` (`WriteCFile`) wraps that blob in
   `const unsigned char heart_packed[] = { … };` plus `heart_packed_WIDTH`,
   `_HEIGHT`, `_COLORS` macros.
4. The example sets `scb.data = heart_packed` and calls `gfx_sprite(&scb)`
   (`examples/suzy/spritetest.c`).

So **every frame is its own image file, its own sp65 invocation, its own C
array, and its own symbol.** A four-frame walk cycle with left/right facings is
eight files and eight Makefile rules. Animation examples in the tree dodge this
by drawing primitives or by hand-mirroring at build time (`examples/games/sybil.c`
pre-mirrors frames); none of them carries a real multi-frame asset, precisely
because the authoring cost is high.

## 2. Why a *runtime* sprite sheet is impossible

On most machines a "sprite sheet" is a single bitmap the blitter samples a
sub-rectangle from: you keep one texture and pass `(x, y, w, h)`. **Suzy cannot
do this**, for three independent reasons:

- **The data is not a rectangle.** Suzy sprite data is a linked list of
  variable-length scan lines. Each line starts with a byte that says how many
  bytes it occupies; packed lines are a bit-stream of RLE packets whose length
  depends on the pixel content. There is no constant stride and no row pitch, so
  there is no arithmetic that yields "row Y, column X" inside the blob. You
  cannot point the SCB at the middle of an encoded sheet.
- **The SCB addresses whole sprites only.** `SCB.data` (`SPRDLINE`) points at the
  *first* scan line of *one* sprite; Suzy follows the per-line offset bytes until
  it hits an offset of `0` (end of sprite). A sub-rectangle has no such
  self-contained structure.
- **Quadrants and the last-pixel bug.** Each frame may be independently split
  into up to four quadrants and may carry its own end-of-line pad byte
  (`LYNX_SPRITE_PADBYTE_DESIGN.md`). These are per-sprite properties decided at
  encode time; they cannot be re-derived by an offset at draw time.

The consequence is firm: **each frame must be encoded as its own complete Suzy
sprite.** Whatever a "sprite sheet" means for the Lynx, the frames are separate
data blocks by the time they reach the cart. The only open question is *how the
toolchain gets there* and *how the data is packaged for the program*.

## 3. The primitive already exists: `--slice`

sp65 already supports cutting a sub-bitmap out of a loaded image. From
`compiler/sp65/main.c`:

- `--read` sets the original bitmap `B` and the work bitmap `C`.
- `--slice x,y,w,h` replaces `C` with `SliceBitmap(C, x, y, w, h)`
  (`compiler/sp65/bitmap.c`).
- `--convert-to` encodes the *current work bitmap* `C` into output `D`.
- `--write` writes `D`.
- `--pop` restores `C` back to the original `B`.

Crucially the option table is processed **left to right in one process**, so a
single sp65 run can read a sheet once and emit many frames. The machinery for
sprite sheets is therefore already present — it has simply never been documented
as a sheet workflow, exercised by an example, or made ergonomic. `--slice` today
appears only as one line in the `--help` text and nowhere in `doc/sp65.html`.

## 4. What works *today* (no code change)

A 4-frame, 16×16 strip laid out horizontally in `walk.pcx` can be diced like
this in one invocation:

```
sp65 -r walk.pcx \
  --slice  0,0,16,16 -c lynx-sprite,mode=packed -w walk0.h,format=c,ident=walk0 --pop \
  --slice 16,0,16,16 -c lynx-sprite,mode=packed -w walk1.h,format=c,ident=walk1 --pop \
  --slice 32,0,16,16 -c lynx-sprite,mode=packed -w walk2.h,format=c,ident=walk2 --pop \
  --slice 48,0,16,16 -c lynx-sprite,mode=packed -w walk3.h,format=c,ident=walk3
```

This already removes the *multiple source files* problem: one artist-facing
sheet, one shared palette, frames guaranteed pixel-aligned. It is the
recommended interim answer and should be documented and given an example
regardless of whether §5 lands.

Its limits are ergonomic, and they are what §5 fixes:

- **N headers, N arrays, N symbols.** The program still sees `walk0…walk3` as
  unrelated globals. To animate you must hand-write `const unsigned char* frames[]
  = { walk0, walk1, walk2, walk3 };` and keep the count in sync by hand.
- **Verbose, error-prone Makefile.** Four near-identical lines per strip; the
  `x` offsets are computed by the author and silently wrong if the sheet is
  re-laid-out.
- **No grid awareness.** Nothing checks that `cols*fw`/`rows*fh` match the image,
  so an off-by-one crops every frame.

## 5. Proposed work: a sheet driver in sp65

Add a grid-slicing convenience that reads one sheet and emits **one** header
containing all frames *plus an indexable pointer table*. Two layering options;
the design recommends **5A**.

### 5A. A new write-side aggregator (recommended)

Keep `GenLynxSprite` untouched (it stays a single-frame encoder) and add a
**driver** that loops over a grid, calls the existing encoder per cell, and
writes a combined header. As built, it is one self-contained sp65 option whose
attribute list carries the grid, the encoder settings and the output:

```
sp65 -r walk.pcx \
     --sprite-sheet name=walk.h,ident=walk,fw=16,fh=16,mode=packed[,cols=N,rows=M,first=K,count=C,gap=G,margin=Mn,ax=..,ay=..,edge=..,format=c,bytesperline=..,base=..]
```

A self-contained option was chosen over the first draft's `-c …`/`-w …` split
because sp65's existing pipeline carries exactly **one** converted blob in the
global `D` between `--convert-to` and `--write`; a sheet produces N blobs plus an
offset table, which that single-blob plumbing cannot pass from convert to write
without new global state. Folding read-grid → slice → encode → write into one
option (`compiler/sp65/spritesheet.c`, `OptSpriteSheet` in `main.c`) keeps the
existing convert/write path untouched and reuses `SliceBitmap` and the unchanged
`GenLynxSprite`. The encoder attributes (`mode`, `ax`, `ay`, `edge`) are read
straight from the same attribute list by `GenLynxSprite`, so a sheet frame is
encoded identically to a lone sprite.

Semantics:

- `fw,fh` — frame cell size (required). `cols,rows` default to
  `floor((W-margin)/(fw+gap))` etc.; `first`/`count` select a sub-range in
  row-major order so one sheet can hold several animations.
- The driver slices each cell (reusing `SliceBitmap`), runs the **already
  selected** converter/mode on it, and concatenates the blobs.
- The C writer emits, for `ident = walk`:

  ```c
  #define walk_COUNT   4
  #define walk_WIDTH   16
  #define walk_HEIGHT  16
  #define walk_COLORS  3
  const unsigned char walk_data[] = { /* frame0 … frame3 back-to-back */ };
  const unsigned char* const walk[walk_COUNT] = {
      walk_data + 0, walk_data + 68, walk_data + 136, walk_data + 204,
  };
  ```

  i.e. one data array and a `const`-pointer table indexing into it. Drawing
  frame `i` becomes `scb.data = (unsigned char*)walk[i];`. The table lives in
  ROM (`const`), costs `2*COUNT` bytes, and the frame *data* is byte-for-byte
  what the §4 chain produces — so this is **byte-neutral on the cart** apart from
  the small pointer table, and that table replaces one the author would have
  hand-written anyway. (Both symbols are emitted non-`static`, matching the
  existing single-sprite headers, so a frame table can also be `extern`-declared
  from another translation unit.)

This is a small, self-contained change: a new `spritesheet.c` (the driver plus
its own C/asm table writers), one option in `main.c`, and no change to the
existing converter or single-blob writers. The asm writer emits a parallel
`.proc` with the data under a `data:` label and a `.word` frame table, so the
feature is usable from assembly and from `.h`/`.inc` includes alike.

### 5B. A `mode=sheet` inside `lynx-sprite` (rejected)

Folding the grid loop into `GenLynxSprite` via `lynx-sprite,sheet,fw=…,fh=…`
keeps the option surface smaller but is the wrong cut: `GenLynxSprite` would have
to know about slicing, multi-frame concatenation, *and* table layout, which
entangles the per-line encoder with asset packaging and makes the existing
single-frame path a special case of a much larger function. Rejected in favour of
5A's separation (slice/loop driver on top of an unchanged encoder).

### Open question: per-frame action points

Quadrant sprites need an action point (`ax,ay`) per frame. v1 applies one
`ax,ay` (in cell-local coordinates) uniformly to every cell — correct for
center-anchored character frames, which is the common case. Per-frame action
points would need a side-car (e.g. a small `.txt` of `frame,ax,ay`) and are left
out of v1; the table format above is forward-compatible because action points
change only the encoded bytes, not the table shape.

## 6. Runtime side — no library change needed

Nothing in the runtime (`libraries/graphics`, `gfx_sprite`) needs to change: a
frame pointer is just a `SPRDLINE` like any other. An animation example would be:

```c
extern const unsigned char* const walk[];   /* from walk.h, walk_COUNT frames */

scb.sprctl1 = PACKED | REHV;
for (;;) {
    while (gfx_busy()) {}
    scb.data = (unsigned char*)walk[frame];
    gfx_sprite(&scb);
    gfx_updatedisplay();
    frame = (frame + 1) % walk_COUNT;
}
```

The only SDK surface that grows is sp65 (the asset side) and the docs.

## 7. Verification — measured

All on the shared 4-frame `examples/suzy/sheet.pcx`; emulator results on the
headless GearLynx (`tests/emu/gearlynx`), the method used for
`spritetest`/`packtest`.

1. **Byte-neutrality — PASS.** The driver's `sheet_anim_data[]` (1088 bytes,
   frames at offsets 0/68/136/204) is byte-identical to the concatenation of the
   four `frame0…frame3` headers produced by the `--slice`/`--pop` chain. The
   driver only repackages; it never re-encodes.
2. **Pixel parity on hardware — PASS.** `spritesheet.c` (table) and
   `spriteslice.c` (manual headers) reset deterministically and, stepped the same
   number of frames, draw the same animation frame at the same position; the
   64×64 ball region is SHA-identical between the two screenshots. Only the
   on-screen captions differ.
3. **Grid validation — PASS.** `--sprite-sheet` errors clearly on an oversized
   cell ("image 64x16 too small for … cell 20x20"), an out-of-range
   `first`/`count`, a grid that overruns the image, and a missing/invalid
   `ident`, instead of silently cropping.
4. **Golden ROM — done.** `suzy/spritesheet` and `suzy/spriteslice` are in the
   integration golden suite (`tests/integration/gearlynx_check.py`,
   `tests/golden/*.sha256`) and pass in compare mode, so a future converter
   change that perturbs either rendering is caught.

The asm writer was also checked: `format=asm` emits a `.proc` with the data
under `data:` and a `.word data + offset` frame table.

## 8. Documentation kept in sync (per CLAUDE.md)

Updated in the same pass as the code:

- `doc/sp65.html` §6.2 — the Sprite-sheets section, `--slice` and
  `--sprite-sheet` documented in the option reference, and a sheet→frames→table
  SVG (per `design/DOC_SVG_STYLE_DESIGN.md`).
- `doc/samples.html` + `examples/Makefile` — two examples and their build rules:
  `examples/suzy/spritesheet.c` (driver) and `examples/suzy/spriteslice.c`
  (manual chain), sharing `examples/suzy/sheet.pcx` (`sheet.pcx.py`).
- `compiler/sp65` `--help` text and `sp65.vcxproj` for the new source file.
- This file — status note (top) and the measured results below.

## 9. Outcome

Both routes shipped together: the documented `--slice`/`--pop` chain (§4) and
the `--sprite-sheet` driver (§5A), each with a worked example sharing one sheet
image. The driver's on-cart frame bytes are identical to the manual chain (§7);
the only added bytes are the ROM pointer table that authors would otherwise write
by hand.

Per-frame action points (the §5 open question) and any `.png`/tilemap import
remain out of scope; the emitted table format is forward-compatible with both
because they change the encoded frame bytes, not the table shape.
