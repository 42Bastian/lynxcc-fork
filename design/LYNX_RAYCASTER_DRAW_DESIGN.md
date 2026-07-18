<!--
SPDX-License-Identifier: CC-BY-4.0
Lynx Game Development SDK documentation, (c) 2026 the lynxcc authors,
licensed under Creative Commons Attribution 4.0 International.
See doc/licenses.html.
-->

# Design: raycaster draw pipeline — one master sprite chain, text last, OPT1 text toggle

Scope: how `examples/games/raycaster.c` issues its per-frame draw calls, and
how to evolve it so that (1) **every** sprite in the frame is painted by a
single chained Suzy engine run instead of a handful of separate
`gfx_sprite()` launches, (2) all glyph text (`gfx_outtextxy`) is emitted after
the last sprite, and (3) the Option-1 button toggles that text on and off at
runtime. This document is the source of truth for that pipeline; it is a plan
for a **lynxcc** example, not an SDK API change.

> **Status — implemented 2026-07-18.** Goals A, B and C are all done and
> GearLynx-verified. Goal A (§2): every sprite in the frame — sky, the 80 wall
> columns, the visible billboards, the gun, the muzzle flash and the three
> status-bar pieces — is now one master `.next` chain launched by a single
> `gfx_sprite(&sky_scb)`; `guard_scb` became `guardscb[NENEMY]` and the variable
> joints (wall tail, enemy links, `gun_scb.next` → flash/hud) are relinked each
> frame in `project_enemies()`, with the static joints set once in `main()`.
> Goal B (§3): `draw_hud` split into the always-drawn status-bar sprites (now
> chain members) and `draw_hud_text` (gated readouts); all glyph text is emitted
> last in one block. Goal C (§4): Opt&nbsp;1 toggles that text block. The frame's
> start-of-game output is byte-hash-identical to the pre-refactor build (the §6
> item-1 regression check). The separate §8 compute-side work is complete:
> §8.1–§8.3 implemented (cast_walls() now issues zero Suzy divides) plus §8.4's
> DDA row pointer; §8.4's other two ideas are documented as declined (the
> accumulator narrowing is numerically unsound and the projection dedup changes
> gameplay — see §8.4).
>
> The second optimization pass (§9) is **implemented 2026-07-18** except for
> its two explicitly-conditional items: §9.3 item 8 is *declined* (the sprite
> launch is synchronous by library design, so there is nothing to overlap —
> see §9.3) and the §9.2/§9.3 items that change pixels are behind the Opt 2
> toggle exactly as planned. Measured on GearLynx (§9.0 method): idle
> 5 → 20 fps, walking 5 → 12.5 fps, turning 5 → 8.5 fps (13.5 fps in the
> Opt 2 half-resolution mode). The start-of-game frame stays byte-identical
> to the pre-§9 build.

## 1. Current draw order

`draw()` today runs, in sequence:

1. `gfx_clear()` — floor fill (a screen clear, not a sprite).
2. `gfx_sprite(&sky_scb)` — ceiling band.
3. `gfx_sprite(&wallscb[0])` — the one existing chain: 80 wall columns linked
   `wallscb[i].next = &wallscb[i+1]`, terminated `wallscb[NCOL-1].next = 0`.
4. `draw_enemies()` — a loop of `gfx_sprite(&guard_scb)`, one launch per visible
   billboard, reusing a single SCB whose fields are rewritten each iteration.
5. `gfx_sprite(&gun_scb)`, then conditionally `gfx_sprite(&flash_scb)`.
6. crosshair `gfx_outtextxy(..., "+")`.
7. `draw_hud()` — `gfx_sprite` of panel, highlight edge and face, then the four
   `gfx_outtextxy` readouts.
8. game-over / wave-clear banners (`gfx_outtextxy`).
9. `gfx_updatedisplay()`.

Every sprite is a separate CPU-issued launch, and text is interleaved at steps
6, 7 and 8. The wall run already proves the pattern we want everywhere: build
the SCB fields ahead of time (in `cast_walls()` / `project_enemies()`, where all
the Suzy math lives), link `.next`, then let the engine walk the chain from one
launch.

## 2. Goal A — a single master chain per frame

All of the frame's sprites are `SCB_REHV_PAL` (same struct, `.next` as the first
field, all `BPP_4 | TYPE_NORMAL`, `LITERAL | REHV`, `NO_COLLIDE`), so they can be
linked into one chain regardless of image or size. The engine draws chain
members in list order, and later members paint over earlier ones, so **chain
order is z-order**. The target topology, back-to-front:

```
sky ─▶ wall0 ─▶ … ─▶ wall79 ─▶ [enemy0 ─▶ … ─▶ enemyN-1] ─▶ gun ─▶ [flash] ─▶ hudPanel ─▶ hudEdge ─▶ face ─▶ 0
```

This z-order is correct by construction: ceiling behind walls; billboards in
front of walls; the gun in front of billboards; the muzzle flash over the gun;
the opaque status bar (panel, then its 1-px highlight edge, then the face) over
everything at the bottom. The gun's sprite reaches ~row 82 (into the top of the
bar); because the panel comes later in the chain it correctly covers the gun's
lower edge, exactly as the separate-call order does today.

### 2.1 Static vs per-frame links

Most links never change and are set once in `main()`:

- `sky_scb.next = &wallscb[0]` (new — folds the sky into the wall run).
- `wallscb[i].next = &wallscb[i+1]` for `i < NCOL-1` (unchanged).
- `hud_scb.next = &hudlt_scb`, `hudlt_scb.next = &face_scb`, `face_scb.next = 0`
  (the status bar is a fixed 3-SCB tail).

Only the joints around the variable-length middle are relinked each frame, in
`project_enemies()` after the draw list is known:

- `wallscb[NCOL-1].next` → first enemy SCB if `draw_n > 0`, else `&gun_scb`.
- enemy SCBs chained `guardscb[k].next = &guardscb[k+1]`; the last enemy
  `.next = &gun_scb`.
- `gun_scb.next` → `&flash_scb` when the flash is active this frame, else
  `&hud_scb`.
- `flash_scb.next = &hud_scb` (static; only reached when linked in).

The whole frame is then one call: `gfx_sprite(&sky_scb)`.

### 2.2 Enemies need an SCB array

Chaining forbids reusing one `guard_scb` for every billboard — each link must be
a distinct SCB because the members coexist in the list with different `data`
(normal vs attack frame), `hpos`, `vpos` and `hsize`/`vsize`. Replace the single
`guard_scb` with `static SCB_REHV_PAL guardscb[NENEMY]`. `project_enemies()`
already computes and sorts the visible set far-to-near into `draw_*[]`; it fills
`guardscb[0..draw_n-1]` from those arrays (same values it stores today) and links
them. The shared penpal / control bytes are initialised once in `main()`.
`draw_enemies()` disappears — its work is now the relink in `project_enemies()`
plus the single master launch.

### 2.3 Suzy-math contract still holds

Chaining requires no CPU work between SCBs, which is already satisfied: every
divide/multiply for the frame is done in `cast_walls()` and `project_enemies()`
*before* the launch, and the per-frame relink is only pointer stores (no Suzy
access). The sprite engine shares Suzy's math registers, so keeping all math
strictly ahead of the one launch is exactly the existing rule, now enforced by
construction because there is a single launch.

## 3. Goal B — text drawn last

Split `draw_hud()` into two:

- `draw_hud_panel()` — nothing; the panel/edge/face are now chain members
  launched in §2. (The function is removed; the SCBs are linked, not called.)
- `draw_hud_text()` — the four `gfx_outtextxy` readouts (WV / SC / HP / EN) with
  their `gfx_setcolor` states.

`draw()` becomes: clear → build/relink chain → `gfx_sprite(&sky_scb)` → **text
block** → `gfx_updatedisplay()`. The text block, in order, is the HUD readouts,
the crosshair, and the game-state banner. Each `gfx_outtext` remains its own
engine launch (glyphs are sprites; batching does not chain them), so this is a
structural tidy, not a speedup — but it means the frame is exactly one sprite
chain followed by a short, self-contained text pass, which is what makes the
toggle in §4 a one-line gate.

Z-order is preserved: all text overlays sprite surfaces it should sit on (HUD
digits over the panel, crosshair over the 3D view, banner over all), and drawing
them after the master chain keeps them on top.

## 4. Goal C — OPT1 toggles text

Add `static unsigned char show_text = 1;`. In the main loop's input handling,
where `pressed` is already computed as the rising edge
(`joy & ~prev_joy`), toggle on Option 1:

```c
if (pressed & JOY_OPT1_MASK) show_text ^= 1;   /* JOY_OPT1_MASK = 0x08 */
```

`JOY_OPT1_MASK` lives in `<lynx/lynx.h>` and occupies bit 3 of the low byte, so
the example's existing `unsigned char joy = joy_read()` already carries it (no
need to widen to the full `int` the way `JOY_PAUSE_MASK` (bit 8) would require).
Using the debounced `pressed` edge — not the raw level — makes each press flip
the state once.

`draw()` gates the entire text block on the flag:

```c
if (show_text) {
    draw_hud_text();
    gfx_outtextxy(SCREEN_W/2 - 4, VIEW_CY - 4, "+");   /* crosshair */
    /* game-over / wave-clear banner */
}
```

With text off the master sprite chain is unchanged, so the status-bar panel and
face still frame the view — you get a clean, glyph-free screen (useful for
screenshots and for seeing the raw 3D view). Default is on. The banner is text,
so in the rare case of dying with text off the "A = NEW GAME" prompt is hidden;
that is acceptable for a debug/clean-view toggle, and pressing A still restarts.

## 5. Expected effect

- Sprite dispatch drops from up to ~90 CPU-issued `gfx_sprite()` launches per
  frame (1 sky + 1 wall-chain + up to 6 enemies + gun + flash + 3 HUD) to a
  single launch that the engine walks via `.next`. The wall columns are already
  one run; the win here is folding the sky, the per-enemy loop and the HUD tail
  into the same run, removing the per-launch CPU issue/wait overhead — most
  visible when several billboards are on screen.
- Text cost is unchanged (still one launch per string) but is now skippable
  wholesale via OPT1.
- Frame output with `show_text = 1` should be **pixel-identical** to the current
  build; this is the primary regression check.

## 6. Verification plan

1. Golden parity: with text on, a fresh 90-frame-from-reset GearLynx capture must
   match the pre-change frame (the change is pure draw-call restructuring). If a
   golden is later minted for `games/raycaster` it should be added to the
   `gearlynx_check.py` `EXAMPLES` list at that time; today the raycaster is not
   in the curated golden set (like `sybil`/`invaders`).
2. Chain integrity: sweep the view with several guards visible and confirm all
   billboards, the gun, the flash and the bar render in one frame (no dropped or
   mis-z-ordered sprite), including the `draw_n == 0` and flash-inactive link
   variants.
3. Toggle: press Option 1 and confirm all glyph text (HUD readouts, crosshair,
   banner) disappears while the panel/face sprites remain, and a second press
   restores it. Verify the edge debounce (holding OPT1 does not flicker).

## 7. Follow-through when implemented

Per the repo rule that docs track code in the same pass: flip this document's
status to *implemented*, refresh the `doc/samples.html` raycaster bullets
(mention the single master chain and the OPT1 clean-view toggle) and its
Controls line (add "Opt 1 toggles the HUD/text"), update the screenshot if the
default view changed, and note the SCB-array / chain refactor in the
`lynxcc-raycaster-sample` memory. No public SDK header or `funcref` entry
changes — this is entirely internal to the example.

## 8. Compute-side optimizations

Goals A–C above restructure *draw dispatch*. This section covers the orthogonal
axis: the per-frame **compute** in `cast_walls()` and `project_enemies()`. The
guiding idea is the one the DDA line algorithm is built on — replace a
per-step divide with work done once up front (an incremental add, or a
precomputed table) — extended past the ray-direction ramp the file already
steps incrementally.

### 8.1 Hoist invariant wall-SCB fields (implemented 2026-07-18)

`cast_walls()` rewrote six SCB fields for all 80 columns every frame, but only
three vary. `sprctl0`, `sprctl1`, `sprcoll`, `data` (always `solid_img`),
`hsize` (always `0x0100`) and `hpos` (`col*COLW`, fixed per column) are
constant for the life of the program. They are now initialised **once** in
`main()`, alongside the existing `.next` chain links; the per-frame loop writes
only `vpos`, `vsize` and `penpal[0]`.

This removes ~5 stores × 80 = ~400 redundant stores per frame plus the per-column
`col*COLW` multiply, and it is **pixel-identical by construction**: the hoisted
fields were already being set to those exact constants every frame, and
`wallscb[]` is touched nowhere but `cast_walls()` and this one-time init. It is
the same "fill the SCB ahead of the launch" principle Goal A applies to the
chain, applied to the fields themselves. This is the safe baseline optimization.

### 8.2 Reciprocal LUT for the delta-distance divides (implemented 2026-07-18)

`cast_walls()` issues three Suzy divides per column — 240 per frame:

```c
ddX = ABSCALE !/ arX;      /* 4096 / arX */
ddY = ABSCALE !/ arY;      /* 4096 / arY  — same map */
...
lh  = PROJK  !/ perp;      /* 1280 / perp */
```

A reciprocal is nonlinear, so — unlike the ray-direction ramp — it cannot be
*incrementally added*; but it can be precomputed into a table, which is the same
DDA spirit (divide once, look up thereafter). `ddX` and `ddY` are the **same
function** `4096/ar`, and after the small-value clamp `ar` lies in ~`[16, 430]`,
so a single shared table `recip[RECIP_MAX+1]` (`RECIP_MAX = 430`) with the
`DDMAX` clamp pre-baked replaces both divides with two indexed reads. Because the
clamped quotient never exceeds `DDMAX = 255` it is stored as `unsigned char`, so
the table is ~431 bytes, not the ~860 first estimated. It is filled once at
startup (`build_tables()`) rather than shipped as a `const` blob — that spends
~431 bytes of RAM plus a one-off startup loop instead of ROM. Since the Suzy
divide and C `/` both truncate, `recip[ar]` is bit-for-bit the old `ABSCALE !/ ar`
result, so §8.2 alone is output-identical; an indexed read (~10 cycles) beats the
Suzy divide's operand-store + poll and frees the shared math unit for the sprite
engine. `arX`/`arY` are clamped to `RECIP_MAX` before indexing (their true max is
~425, so the clamp is a safety bound, not a visible change).

### 8.3 Reciprocal LUT for the slice-height divide (implemented 2026-07-18)

`lh = PROJK !/ perp` is clamped to `1..MAXSLICE (80)` and so is non-trivial only
for `perp < 1280`; `lh` fits in a byte. `slicelut[SLICE_N]` (`SLICE_N = 641`,
`unsigned char`, indexed `perp>>1`) with the clamp baked in removes the third and
last per-column divide, taking `cast_walls()` from 240 hardware divides/frame to
zero (verified in `raycaster.s`: no `suzyudiv` remains in the wall cast; the one
left is the enemy-height divide in `project_enemies()`). Dropping the low bit of
`perp` quantizes the slice height by at most ±1 px on some columns, so unlike 8.2
this is NOT output-identical — a start-of-game frame differs from the pre-LUT
build in ~0.56 % of pixels (a few wall-edge rows), which is why 8.5 calls for a
frame compare, not a byte-exact check. Cost across 8.2 + 8.3 is ~1.1 KB of table
(here in RAM, built at startup); staged after the free 8.1 win rather than bundled
with it. Weigh the RAM/ROM placement against the cart budget
(`design/LYNX_CART_SIZES_DESIGN.md`) if ported to a size-critical target.

### 8.4 Smaller wins

- **DDA inner loop — IMPLEMENTED 2026-07-18.** `worldmap[mapY][mapX]` cost a
  `×MAPW` each iteration. `cast_walls()` now caches `const unsigned char* row =
  worldmap[mapY]` and refreshes it only on a Y-step; each step moves exactly one
  axis, so the per-iteration bounds test is split to test only the stepped axis
  (the other is still valid from the previous step). `row[mapX]` replaces the
  double index. Pixel-identical (byte-identical GearLynx frame vs the 8.2/8.3
  build); it only removes the row multiply and half the bounds compares from the
  hot loop.
- **Duplicated projection — DECLINED (behaviour change, negligible gain).** The
  hitscan in `update_player()` recomputes `depthC`/`lat`/`proj` per enemy that
  `project_enemies()` also computes. Reusing the draw list is NOT a safe swap:
  the hitscan runs *before* `project_enemies()` in the frame and deliberately
  considers **every** alive enemy, including those `project_enemies()` culls
  (`sh < 6` too far, or occluded), so reusing the list would shrink the hittable
  set (you could no longer shoot a distant guard) — a gameplay change to save
  ≤ `NENEMY` fused muldivs, and only on the frames the trigger is pulled. Left as
  two independent projections on purpose.
- **Accumulator width — DECLINED (unsound as specified).** `accX/accY` step in
  16.16 (two 32-bit adds × 80/frame), but a **24-bit** accumulator cannot hold
  them: `|accX|` reaches `planeX<<16` with `planeX` up to `FOV = 169`, i.e.
  ~11.08 M, past the signed-24-bit limit (±8.39 M), so it would overflow. cc65
  also has no 24-bit integer type, so this is not expressible in portable C
  (it would need hand-written asm), and any narrower accumulator drops fractional
  precision and stops being pixel-identical. Kept as `long`. A genuine win here
  would be an asm-coded 3-byte-mantissa step, out of scope for a C example.

### 8.5 Verification note

8.1 is a pure code move and must stay pixel-identical (screenshot compare, not a
byte-identical `.lnx` — relocating the field init changes generated code
ordering). 8.2 is also output-identical (the table stores the same truncated
`ABSCALE/ar` the Suzy divide produced), so 8.1 + 8.2 together still render frame-
identically to their predecessor. 8.3 drops the low bit of `perp`, so it is the
first change that alters pixels (~0.56 % of a start frame, ±1 px wall edges) and
must be signed off with a GearLynx frame compare and a visual check rather than a
byte/pixel-exact guarantee; 8.4's implemented item (the DDA row pointer) is again
pure and verified byte-identical, while its other two ideas were declined (§8.4).

## 9. Second optimization pass — implemented 2026-07-18

With §2–§4 and §8 done, the frame is a single sprite-chain launch and
`cast_walls()` issues zero Suzy divides. The remaining per-frame cost is
(a) cc65-generated **compute** in the 80-column loop — 32-bit accumulator
stepping, clamps, stack-frame locals, signed arithmetic — and (b) redundant
**Suzy fill**: the full-screen `gfx_clear()` paints ~16.3 k pixels of which
~62 % are immediately overpainted by the sky band (rows 0–39) and the opaque
HUD panel (rows 80–101). The plan below is ordered cheapest-first; each phase
is independently shippable and independently verifiable.

### 9.0 Measure first (done 2026-07-18)

Before changing anything, instrument frame time: read a free-running Mikey
timer at the top of the main loop (or count VBLs per `gfx_updatedisplay`) and
capture a baseline ms/frame on GearLynx, plus a bracket around `cast_walls()`
alone. The perceived slowness is almost certainly a low frame rate making
`MOVESPEED`/`TURNSPEED` feel sluggish; the measurement decides when the pass
is done and whether §9.3's tradeoff items are needed at all. The
instrumentation is temporary and must be removed (or compiled out) before any
golden capture.

**Done.** Method used: a throwaway build with a `loopcnt` counter bumped once
per main-loop iteration, read via the emulator's `read_memory` while stepping
display frames (no source change survives; the shipped ROM is uninstrumented).
Baseline: **~5 fps** (200 ms/frame) regardless of input; a second build calling
`cast_walls()` twice measured 3 fps, putting the cast at **~133 ms ≈ ⅔ of the
frame** and everything else (project, draw, text, VBL waits) at ~67 ms. After
the pass: idle 20 fps, walking 12.5 fps, turning 8.5 fps (the turn frames pay
the `rebuild_raycache()` ramp), Opt 2 low-res turning 13.5 fps.

### 9.1 Pixel-identical, low risk (implemented 2026-07-18)

All four items are in, plus one found-along-the-way fix: `&wallscb[col]`
inside the column loop was a *software* multiply by `sizeof (SCB_REHV_PAL)`
(23, not a power of two) per column; it is now a running pointer bumped with
`++s`. Item 1 took the "better" branch: a `floor_scb` chain member
(`sky -> floor -> walls`), and the `gfx_clear()`/`gfx_setcolor` pair is gone.
Item 2's border-wall invariant is documented at the `worldmap` definition.
Item 3's flat map also feeds item 2: the DDA inner loop is now
add / index-step (`±1`/`±16`) / load / test only. Item 4: the hot scalars are
`__zeropage__` statics, `col` is a byte, `vposlut[]` (filled beside
`slicelut[]` in `build_tables()`) replaces the signed `(VIEW_H - lh) / 2`,
and only the `vsize` high byte is stored per column (the low byte stays 0
from BSS) — see the §9.4 miscompile note for how that store must be written.

1. **Drop the full-screen clear.** `gfx_clear()` (a 1 bpp stretched pen-fill
   sprite, see `libraries/graphics/gfx-clear.s`) fills all 102 rows with
   `PEN_FLOOR`, but only rows 40–79 survive to scan-out. Either call
   `gfx_clearrows (VIEW_CY, VIEW_H - VIEW_CY)` instead, or — better — fold a
   40-row floor band sprite into the master chain (`sky -> floor -> walls`)
   and delete the clear call plus its `gfx_setcolor`, saving ~10 k fill pixels
   and one CPU-issued engine launch per frame. Pixel-identical either way.
2. **Delete the DDA bounds tests.** The `worldmap` border is fully walled
   (every edge cell is nonzero) and each DDA step moves exactly one axis, so a
   ray can never leave the map: the `mapX/mapY` range tests in the inner loop
   are dead code. Keep the 40-step guard as the safety net. If implemented,
   document the border-wall invariant at the map definition — the data now
   carries a correctness obligation.
3. **Flat 256-byte map.** Store the map as a flat array indexed
   `(mapY << 4) | mapX` in a single byte, stepping `±1` (X) or `±16` (Y).
   This removes the row-pointer refresh on Y-steps and lets the map coordinate
   live in one `unsigned char` — combined with item 2 the inner loop becomes:
   add side, add step to the index, load, test.
4. **Codegen hygiene in the hot loop.** Hoist `cast_walls()` locals to
   `__zeropage` statics (`<zeropage.h>`, the fork's zeropage attribute);
   narrow `int` locals to `unsigned char`/`unsigned int` where ranges allow
   (`col`, map coords, `guard`); replace the signed
   `(VIEW_H - (int)lh) / 2` with a `vposlut[]` parallel to `slicelut[]`
   (same index, byte entries); and stop rewriting the always-zero low byte of
   `vsize` (init once, store only the high byte per frame).

### 9.2 Algorithmic (implemented 2026-07-18)

All three items are in. Item 5 lives in `rebuild_raycache()` (four `NCOL`-byte
arrays `cddX/cddY/cstepX/cstepY`; `update_camera()` sets a `raydirty` flag,
`cast_walls()` rebuilds on entry when it is set — the cache is a pure function
of `ang`, and with the flat map the cached Y step is `±MAPW` directly). Item 6
is the `lastX/lastY` early-out at the top of `cast_walls()`. Item 7 indexes
`slicelut[depthC >> 5]` (the `>>4` then `>>1`), whose baked-in clamp replaces
the old `sh > VIEW_H` test; `depthC < 8192` keeps the index in range. With it,
`project_enemies()`' divide is gone and the frame issues no mid-frame Suzy
divide at all — the remaining Suzy ops are the two first-step multiplies per
column and the two fused muldivs (billboard + hitscan projections).

5. **Angle-dirty ray cache.** Per-column `ddX/ddY/stepX/stepY` depend only on
   `ang` (256 quantized values) — not on position. Cache them in four 80-byte
   arrays rebuilt only when the player turns; walking and idle frames then
   skip the entire 32-bit `accX/accY` ramp, the small-ray clamps, the abs, and
   both `recip[]` lookups for every column. Turning frames pay today's cost.
   Output-identical (the cached values are exactly what the ramp computes).
6. **Idle skip.** If neither `posX/posY` nor `ang` changed this frame, skip
   `cast_walls()` entirely — `zbuf[]` and the wall SCBs are still valid
   (enemies still reproject against the stale-but-correct z-buffer). Free
   frames while the player is aiming or firing from a standstill.
7. **`slicelut` for the enemy height.** `sh = PROJK !/ (depthC >> 4)` in
   `project_enemies()` is the same map as `slicelut` (clamped to `VIEW_H`);
   replacing it accepts the same ±1 px quantization as §8.3 on ≤ `NENEMY`
   sprites. Minor on its own, but it removes the last mid-frame Suzy divide,
   which §9.3 item 8 requires.

### 9.3 Bigger levers — resolved 2026-07-18 (item 8 declined, item 9 done)

8. **Overlap CPU with the sprite engine — DECLINED (nothing to overlap).**
   The premise fails on the library contract: `gfx_sprite()` /
   `gfx_draw_sprite` is *synchronous* — it busy-waits (via `CPUSLEEP`) until
   the engine has walked the whole chain, precisely so that the engine is
   provably idle on entry to every graphics call and unguarded SCB-register
   access plus the Suzy math helpers stay legal
   (`libraries/graphics/gfx-core.s`; design/LYNX_GFX_DESIGN.md §5). The CPU
   therefore never gets the chain-walk time back, and `gfx_busy()` reports
   the pending page-*swap*, not engine business, so the sketched
   "wait before the first Suzy computation" has nothing to wait on. Doing
   this for real means an asynchronous launch primitive plus a busy-guard in
   every gfx entry point — an SDK design change (compare `suzyasync.s` for
   the math unit), out of scope for an example. §9.0's numbers also show the
   prize is small: the frame is CPU-compute-bound, not fill-bound.
9. **Column count tradeoff — IMPLEMENTED as the Opt 2 runtime toggle.**
   `NCOL_LO = 40` double-width columns (`hsize 0x0200`, hpos spacing 4,
   `CAMSTEP_LO = 3276`); `set_quality()` reconfigures the wall run's
   hpos/hsize/`.next` links and the `ncol`/`colshift`/`camstep` globals and
   marks the ray cache dirty, so flipping is safe mid-game (the hitscan and
   occlusion `sx >> colshift` column mapping follows it). Halves both the
   cast and the turn-frame cache rebuild: turning 8.5 → 13.5 fps measured.
   The interlace variant was not taken (its turn-fast artifacts fight the
   whole point of speeding turns up). Default remains full resolution, so
   golden captures are unchanged.

### 9.4 Verification (done 2026-07-18)

Per phase: full rebuild, GearLynx frame compare, fps re-measure against the
§9.0 baseline. Items 1–6 must be pixel-identical by construction (item 1 via
frame compare since code layout shifts; items 2–5 change no computed value);
item 7 inherits §8.3's ±1 px caveat; items 8–9 need a visual/behavioural
check, and item 9 changes pixels by design. The raycaster is still not in the
curated golden set, so comparisons are fresh 90-frames-from-reset captures,
per §6.

**Results.** The post-§9 repo build's 90-frame-from-reset capture is
*byte-identical* (same PNG SHA-256) to the pre-§9 baseline — items 1–6 as
required, and item 7's ±1 px caveat does not surface on the start frame (no
billboard is visible there). Chain integrity was checked with a guard on
screen (`draw_n > 0`, attack frame, both quality modes) and the Opt 2 toggle
verified live in both directions (`ncol` 80 → 40 → 80). The full curated
golden suite passes (the standing `mikey/setbpp` drift predates this work).
fps numbers are in §9.0.

**Compiler note (found by item 4's vsize store).** The fork (inherited from
upstream cc65 2.19) miscompiled `((unsigned char*)&s->member)[k] = v` for a
struct pointer `s` and **constant** `k`: the member offset was dropped and
the store landed at `((unsigned char*)s)[k]` (here: the wall heights were
written over `sprctl1` at offset 1 instead of `vsize+1`, leaving every
`vsize` zero — no walls). The initial workaround was a two-step byte-pointer
variable; the compiler has since been **fixed** (ArrayRef's constant-
subscript path now adds the pending member offset instead of overwriting
it), the workaround has been removed, and this store is the live regression
canary. Full analysis, fix and test coverage:
`design/LYNX_MEMBER_ADDR_CAST_FIX_DESIGN.md` and
`tests/compiler/member_addr_cast.py`.

