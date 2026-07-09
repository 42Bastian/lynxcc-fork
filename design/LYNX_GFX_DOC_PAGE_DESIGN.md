<!--
SPDX-License-Identifier: CC-BY-4.0
Lynx Game Development SDK documentation, (c) 2026 the lynxcc authors,
licensed under Creative Commons Attribution 4.0 International.
See doc/licenses.html.
-->

# Design: a dedicated `doc/graphics.html` Reference page

**Status: implemented.** `doc/graphics.html` was built to this specification
(nav entry, search-index registration, and the cross-links from
`memory.html`/`index.html`/`samples.html` are all in place). This document remains
the source-of-truth outline for the page, the way `design/LYNX_GFX_DESIGN.md` is the
source of truth for the library itself; keep both in sync with the page under the
`CLAUDE.md` docs-track-code rule.

**Note — `lynx.html` is being removed entirely.** The old "Lynx specifics" page
(`doc/lynx.html`) is scheduled for removal, so this document does not treat it as a
source to slim, link to, or migrate content out of. The graphics and colour-constant
material that page used to carry is authored fresh on `graphics.html`; nothing here
depends on `lynx.html` continuing to exist.

## 1. Problem

The graphics system has no reference page of its own. What little user-facing
prose existed lived on the "Lynx specifics" page, which is being removed entirely,
so that material needs a proper new home rather than a rescue:

- The old graphics prose was nine short paragraphs — it named the `gfx_*` calls in
  passing but did not enumerate the API, never gave the display resolution or
  colour depth as a fact, never stated where the frame buffers live in memory, and
  described double buffering in two sentences with no diagram. `gfx_setdrawpage`,
  `gfx_setviewpage`, `gfx_setbpp`, `gfx_clearrows`, the text/font calls and the
  palette calls were not covered at all.
- The 16 `COLOR_*` names and the `GFX_COLOR_*` aliases sat under "hardware
  structures" rather than under graphics.
- `doc/funcref.html` — one-line-per-function signatures, no narrative.

So a reader who wants to understand *how the graphics system actually works* — the
single fixed mode, the two frame buffers and their addresses, how a page flip is
timed against vertical blank, how much compute is left between refreshes, how the
palette is laid out — has nowhere to go. `design/LYNX_GFX_DESIGN.md` has the
depth, but it is an internal rationale document (why TGI was removed), not
user-facing reference, and it is not linked from the Reference menu.

This mirrors the recent `comlynx.html` split, where ComLynx serial was promoted
into its own Reference page with proper hardware description and SVG diagrams (see
the `lynxcc-comlynx-doc-page` change). Graphics deserves the same treatment, and is
a more central topic.

## 2. Goal

A single Reference page, `doc/graphics.html`, titled **"Graphics"**, that fully
documents the Lynx graphics system as the SDK exposes it:

1. The one fixed display mode and what "one mode" buys the programmer.
2. The complete `gfx_*` API, grouped by purpose, each entry cross-linked to its
   `funcref.html` anchor rather than re-specified from scratch.
3. **Double buffering** — the two pages, where they live in memory, how drawing
   and viewing are separated, and how `gfx_updatedisplay` defers the swap to VBL.
   With diagrams.
4. **Refresh rate and frame timing** — the three selectable rates, the VBL
   period each implies, and the resulting CPU budget between refreshes (with the
   caveat that Suzy and display DMA steal cycles from that budget).
5. **Colour and the palette** — the `COLOR_*` / `GFX_COLOR_*` constants and the
   32-byte palette layout.
6. Pointers out to the deeper material: fonts (`lynx_gfx_fonts.html`), sprite
   authoring / pad-byte rule, Suzy math, and the design doc.

The page follows `design/DOC_STRUCTURE_DESIGN.md` (shared `<site-nav>` /
`<site-foot>` chrome, `<title>` = `lynxcc - Graphics`) and
`design/DOC_SVG_STYLE_DESIGN.md` for every diagram.

## 3. Page outline

Section numbering is the doc-set convention (`sect-N`, `sect-N-M` ids for the
search index and quick-jump palette).

### 3.1 The display mode (§1)

State the compile-time facts up front, as facts, not queries:

- 160 × 102 pixels, 16 colours, 2 display pages — from `GFX_XRES`, `GFX_YRES`,
  `GFX_COLORCOUNT`, `GFX_PAGECOUNT` in `include/lynx/gfx.h`.
- There is exactly one mode; it cannot fail to initialise and cannot be changed
  (except depth, §3.6). The old query functions (`gfx_getxres()` …) are
  zero-cost macros over the constants — mention this and point at the constants
  table.
- All drawing is done by Suzy's sprite engine; there are no geometric primitives
  and no pixel plot call. A one-paragraph "if you want a shape, build a sprite"
  note, linking the sprite/pad-byte material (§3.8).
- `gfx_init()` is the one required call: enables the VBL timer IRQ, sets the
  collision buffer, selects 4bpp, page 0 for both view and draw, loads the
  default palette, and sets the drawing pen to **black**. So `gfx_init();
  gfx_clear();` gives a black screen. Flag the black-default gotcha (draw a
  colour explicitly before drawing, or you draw black-on-black).

### 3.2 API overview (§2)

A grouped table of every `gfx_*` entry point, each linking to its
`funcref.html#gfx_*` anchor. Grouping mirrors `gfx.h`'s own banners so the page
and the header stay legible against each other:

- **Init / clear** — `gfx_init`, `gfx_clear`, `gfx_clearrows`.
- **Sprites / display** — `gfx_sprite`, `gfx_flip`, `gfx_setviewpage`,
  `gfx_setdrawpage`, `gfx_busy`, `gfx_updatedisplay`, `gfx_setframerate`,
  `gfx_setcollisiondetection`, `gfx_setbpp`.
- **Colour / palette** — `gfx_setcolor`, `gfx_getcolor`, `gfx_setbgcolor`,
  `gfx_setpalette`, `gfx_getpalette`, `gfx_getdefpalette`.
- **Text** — `gfx_gotoxy`, `gfx_outtext`, `gfx_outtextxy`, `gfx_settextscale`,
  `gfx_settextstyle`, `gfx_setfont`, `gfx_settextdir`, `gfx_gettextwidth`,
  `gfx_gettextheight`.
- **Compile-time constants / query macros** — `GFX_XRES`, `GFX_YRES`,
  `GFX_COLORCOUNT`, `GFX_PAGECOUNT`, the `GFX_FONT_*` and `GFX_TEXT_*` selectors,
  and the `gfx_getxres()`-style macros.

Rule (from the repo convention): this table is the *only* place signatures might
drift from `funcref.html`, so the page links out rather than duplicating
prototypes. Keep it a name + one-line-purpose + anchor table.

### 3.3 Frame buffers and memory (§3)

The concrete facts, none of which are currently written down for the reader:

- Two frame buffers ("pages"), each the full 160 × 102 × 4bpp screen = 80
  bytes/line × 102 lines = **8160 bytes**.
- Page 0 base **$E018** (`GFX_PAGE0_ADDR`), page 1 base **$C038**
  (`GFX_PAGE1_ADDR`). These addresses come from the linker config memory map
  (`cfg/lynx*.cfg`); note the coupling — they are not free-floating constants.
- Cross-link to `memory.html` for the whole address-space picture; this page owns
  only the two buffers.
- **2bpp mode** (§3.6) changes the scanned-out size to 40 bytes/line × 102 =
  **4080 bytes/page**, leaving the upper 4080 bytes of each page free; the base
  addresses do not move.
- A small labelled memory-map figure (§3.9, Diagram A) showing the two page
  ranges within the address space.

### 3.4 Double buffering (§3, continued)

The heart of the page. Explain the model in terms of two independent pointers:

- **Draw page** (`gfx_setdrawpage`, `gfx_drawpage` internally) — where
  `gfx_sprite` / `gfx_clear` / `gfx_outtext` render.
- **View page** (`gfx_setviewpage`, latched into Mikey's `DISPADR`) — what the
  hardware scans out.
- Single-buffered use: set draw = view = same page (0 or 1). Every draw is
  immediately visible; simplest, but the screen can tear/flicker mid-frame.
- Double-buffered use: draw into the hidden page while the other is shown, then
  swap. The swap must happen during vertical blank or the display tears.
- `gfx_updatedisplay()` sets a *swap-request* flag and returns immediately; the
  actual swap is performed by the VBL interruptor (`gfx_vbl_irq`), which on the
  next vertical-blank interrupt makes the just-drawn page the view page, flips the
  draw page to the other buffer, and clears the request. Emphasise: it does **not
  block** — it is fire-and-forget; poll `gfx_busy()` (returns nonzero while a
  requested swap is still pending) if you must know the swap has happened before
  reusing the buffer.
- Be precise that `gfx_updatedisplay` does **not** wait for the next VBL (a common
  misreading); it *defers* the swap to the next VBL and returns straight away. This
  page is the place to get that right.
- The `.interruptor` linkage note: the VBL swap handler is only linked when a
  program actually uses paging, so a single-buffered program pays nothing for it.
- Two diagrams: Diagram B (draw/view pointer state across a frame) and Diagram C
  (the `gfx_updatedisplay` → VBL → swap timeline). §3.9.

### 3.5 Refresh rate and frame timing (§4)

- Three selectable rates via `gfx_setframerate(50|60|75)`; returns nonzero for an
  invalid rate (the only fallible call in the library). It writes only the timer
  backup registers `HTIMBKUP` and `PBKUP` (the safe subset of timer handling),
  with the concrete values `50 Hz → HTIMBKUP $BD, PBKUP $31`; `60 Hz → $9E,
  $29`; `75 Hz → $7E, $20`.
- The VBL period each rate implies: **50 Hz → 20.0 ms**, **60 Hz → 16.67 ms**,
  **75 Hz → 13.33 ms** per frame.
- **Compute budget between refreshes.** At the Lynx's ~4 MHz 65SC02 the *gross*
  CPU budget per frame is ≈ system-clock / rate:

  | Rate | Frame period | Gross CPU cycles/frame (~4 MHz) |
  |------|--------------|---------------------------------|
  | 50 Hz | 20.0 ms | ≈ 80,000 |
  | 60 Hz | 16.67 ms | ≈ 66,700 |
  | 75 Hz | 13.33 ms | ≈ 53,300 |

  State clearly that this is a *ceiling*, not the real budget: the display DMA
  steals cycles every scanline to refresh the screen, and every synchronous
  `gfx_sprite` call busy-waits while Suzy renders (the drawing engine and the CPU
  do not run in parallel from the program's point of view). So a higher refresh
  rate buys smoother motion at the cost of fewer cycles for game logic and fewer
  sprites drawable per frame. Frame the trade-off; do not over-claim exact
  steal-cycle counts (mark the DMA/Suzy contention figure as
  hardware-measurement territory, consistent with the design doc's tone).
- Practical guidance: 60 Hz is the usual choice; 75 Hz for the smoothest motion
  when the frame's work is light; 50 Hz to buy the most compute per frame.
- Diagram D: a frame-time bar showing VBL, the swap window, and the remaining
  compute slice, at the three rates. §3.9.

### 3.6 Display depth (§5)

- `gfx_setbpp(4|2)` selects how Mikey's display DMA reads the buffer (DISPCTL
  B2); 4bpp is the default and the only spec-blessed mode.
- 2bpp is a CPU-rendered framebuffer mode (Suzy always emits 4bpp, so
  sprite/text output scans out garbled in 2bpp); it uses a DISPCTL bit outside
  spec guarantees and is unverified on real hardware. Keep this clearly flagged
  as advanced/experimental and link `LYNX_GFX_DESIGN.md` §2.7 for the full
  analysis. Note the freed upper 4080 bytes/page.

### 3.7 Colour and the palette (§6)

- The `COLOR_*` table and the `GFX_COLOR_*` alias note live here. Present the 16
  default-palette entries.
- Document the palette wire format: 32 bytes — 16 green nibbles followed by 16
  blue/red bytes — as consumed by `gfx_setpalette` and returned by
  `gfx_getpalette` / `gfx_getdefpalette`.
- `gfx_setcolor` / `gfx_getcolor` (drawing pen 0–15) and `gfx_setbgcolor`
  (text background; 0 = transparent).
- Diagram E (optional): the 16 default swatches with names + indices, using
  theme-safe rendering.

### 3.8 Sprites, collision, and going deeper (§7)

Short pointer section, no duplication:

- `gfx_sprite` draws an SCB / SCB chain synchronously; link the sprite pad-byte
  rule and the `$xxFA` palette bug from `LYNX_GFX_DESIGN.md` §5 and the packtest
  example.
- `gfx_setcollisiondetection` + the `lynx-coll.cfg` requirement to reserve the
  collision buffer.
- Fonts → `lynx_gfx_fonts.html`. Suzy math → funcref / `suzymath.h`. Design
  rationale → `design/LYNX_GFX_DESIGN.md`.

### 3.9 Diagrams

All per `design/DOC_SVG_STYLE_DESIGN.md`: `viewBox="0 0 720 …"`, theme CSS
variables only (no hard-coded colours except the palette swatches, which must
still read in both themes), mono font for addresses/register values, sans for
labels, each wrapped in `<figure>` / `<figcaption>`.

- **Diagram A — memory map of the two pages.** Two stacked address ranges
  ($E018 and $C038, each 8160 bytes) positioned in the address space, with the
  2bpp free-region annotation.
- **Diagram B — draw vs view pointers.** Two page boxes; arrows labelled "DRAW"
  and "VIEW" pointing at them; two states side by side (before swap / after
  swap) to show the pointers exchanging.
- **Diagram C — deferred swap timeline.** `gfx_updatedisplay()` sets flag →
  frame continues → VBL interrupt → interruptor swaps pages + clears flag.
  Horizontal timeline with the VBL marker.
- **Diagram D — frame budget bar.** For 50/60/75 Hz, a bar of the frame period
  with the VBL/swap slice and the "compute" slice called out; annotate the gross
  cycle ceiling and note DMA/Suzy steal.
- **Diagram E (optional) — default palette swatches.**

## 4. Content sourcing (facts and where they come from)

| Fact | Source in tree |
|------|----------------|
| 160×102, 16 colours, 2 pages | `include/lynx/gfx.h` (`GFX_XRES` etc.) |
| Page 0 = $E018, page 1 = $C038 | `asminc/gfx.inc` (`GFX_PAGE0_ADDR`, `GFX_PAGE1_ADDR`) |
| 8160-byte / 4080-byte page sizes; flip offsets 8159 / 4079 | `asminc/gfx.inc` (`GFX_FLIPOFFS_4BPP/2BPP`), `gfx-page.s` |
| Draw/view separation, deferred VBL swap, `.interruptor` | `libraries/graphics/gfx-page.s` (`gfx_vbl_irq`, `gfx_updatedisplay`, `set_dispadr`) |
| Refresh-rate register values ($BD/$31, $9E/$29, $7E/$20) | `libraries/graphics/gfx-rate.s` |
| `gfx_init` defaults (4bpp, page 0, black pen) | `gfx-init.s`, `gfx.h` doc comment, `LYNX_GFX_DESIGN.md` §2.1/§2.8 |
| 2bpp caveats | `LYNX_GFX_DESIGN.md` §2.7, `gfx-page.s` (`gfx_setbpp`) |
| `COLOR_*` / `GFX_COLOR_*`, palette format | `gfx.h`, `include/lynx/lynx.h` |
| Frame-period ms + cycle ceilings | derived (1/rate; ~4 MHz × period) — **mark as approximate; DMA/Suzy steal not counted** |

The only *derived* numbers are the frame-timing table (§3.5). Everything else is
transcribed from the header/asminc/source. The cycle ceilings must be labelled
approximate, matching the careful "verify on hardware" register the rest of the
graphics docs use.

## 5. Integration with the existing doc set

1. **Nav (`doc/doc.js`, `TOPBAR_HTML`).** Add a Reference-dropdown entry. Place
   it first in the Reference group (graphics is the most central):
   `<a href="graphics.html"><span>Graphics</span><span class="tdesc">display, double buffering</span></a>`.
   Per `DOC_STRUCTURE_DESIGN.md` the active-page highlight derives from the
   filename automatically; no per-page edit needed.
2. **Search index (`doc/doc.js` section-index array + `gen-search-index.py`).**
   Register each `sect-N` / `sect-N-M` id so the quick-jump palette (`Cmd-K`,
   `/`) finds them, following the existing `["page.html","sect-id","Title"]`
   rows.
3. **No `lynx.html` handoff.** `lynx.html` is being removed entirely, so there is
   nothing to slim or stub there and no `#sect-2-3`-style inbound links to keep
   alive. Any nav/search-index entry or cross-link that still points at
   `lynx.html` is dropped as part of that page's removal, not repaired.
4. **Cross-links in.** Add "see Graphics" pointers from `memory.html` (the two
   buffer ranges), `funcref.html` intro (or leave funcref linking out — the
   Graphics page links *in* to funcref), and `samples.html` where the graphics
   examples are listed.
5. **`index.html`.** Add a Graphics card/link alongside the other Reference
   pages if the landing page lists them.
6. **License header.** `graphics.html` carries the same `CC-BY-4.0` SPDX comment
   as the other doc pages (per `LYNX_LICENSE_POLICY_DESIGN.md`).

## 6. Docs-track-code obligations

This is a new documentation surface, so the standing rule
(`CLAUDE.md` → "Documentation stays in sync with code") extends to it: any future
change to a `gfx_*` symbol, the page addresses, the refresh-rate values, or the
palette format must update `graphics.html` in the same pass, alongside
`gfx.h`/`gfx.inc` doc comments, `funcref.html`, and `LYNX_GFX_DESIGN.md`. Record
`graphics.html` in the design doc's touch lists where those symbols are listed.

## 7. Explicitly out of scope

- Writing `graphics.html` itself (this is design only).
- Any code or API change — the page documents the library exactly as it is.
- Font glyph tables — those stay in `lynx_gfx_fonts.html`; Graphics only links to
  them.
- Measured Suzy/display-DMA steal-cycle counts — the frame-budget section stays
  at the "gross ceiling + qualitative steal" level until someone measures real
  hardware, matching `LYNX_GFX_DESIGN.md`'s stance on unverified figures.

## 8. Implementation order (when built)

1. Scaffold `graphics.html` from an existing Reference page (`comlynx.html` is
   the closest template: hardware description + SVG diagrams + `<site-nav>`
   chrome).
2. Fill §1–§2 (mode + API table) from `gfx.h` / `funcref.html`.
3. Fill §3–§4 (buffers, double buffering, timing) and author Diagrams A–D.
4. Author the colour material in §6; author Diagram E.
5. Register nav + search-index entries; add cross-links; verify quick-jump and
   the light/dark rendering of every diagram.
