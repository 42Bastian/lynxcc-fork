# Design: Replacing the TGI Driver Mechanism with a Static Sprite/Text Library

Based on analysis of this tree (cc65 2.19, Lynx-only). Companion to
`LYNX_CODEGEN_DESIGN.md` (the synchronous-drawing contract in its §2.6 is preserved here).

**Premise (agreed):** every Lynx program needs graphics, there is exactly one display mode
(160×102×16), all drawing is done by the Suzy sprite engine, and the only software
rendering kept is the 8×8 bitmap font. Geometric primitives (`line`, `bar`, `circle`,
`ellipse`, `arc`, `pieslice`, `setpixel`, `getpixel`) are removed: they pretend to be
framebuffer operations but actually launch sprite draws, which is misleading — programs
that want shapes should build sprites. Vector fonts are removed. Clean API break, no
compatibility shims.

## 1. What exists today, and what it costs

TGI is a *loadable driver* framework designed for machines with interchangeable video
modes and disk-loaded drivers. The call path for, e.g., `tgi_setcolor()`:

```
C call → kernel wrapper (libsrc/tgi/tgi_setcolor.s, arg check + bookkeeping)
       → jsr tgi_setcolor (RAM jump vector in tgi-kernel.s)
       → jmp $xxxx (patched at install time)
       → driver routine SETCOLOR (libsrc/lynx/tgi/lynx-160-102-16.s)
```

The machinery behind this, all of it dead weight when there is one driver that is always
present:

| Component | Where | Cost |
|---|---|---|
| Driver header (magic, API version, libref, 10 var bytes, 19-entry jump table) | `lynx-160-102-16.s` | 55 bytes |
| RAM jump vector table (19 × `jmp $0000`) | `tgi-kernel.s` `.data` | 57 bytes RAM |
| `tgi_install`: signature check, libref patch, vector copy, header-var mirror copy | `tgi-kernel.s` | ~130 bytes code |
| Mirrored header vars (`_tgi_xres` … `_tgi_flags`, `_tgi_xmax/ymax`, `_tgi_drv`) | `tgi-kernel.s` `.bss` | 16 bytes RAM |
| o65 module loader path (`tgi_load_driver` → `modload.s` + `open/read/close`) | `libsrc/common` | ~1 KB when referenced |
| Self-modifying IRQ stub (`tgi_libref`/`tgi_irq` RTS↔JMP patched by INSTALL/UNINSTALL) | `tgi_irq.s`, driver INSTALL | 3 bytes data + patch code |
| `.tgi` o65 file build + `tgi_stddrv`/`tgi_static_stddrv` indirection | Makefile `DRVTYPES`, `target/lynx/drv/tgi/` | build complexity, 2460-byte artifact |
| `CONTROL`/`tgi_ioctl` dispatcher (cmp-chain for the 6 Lynx ops incl. `tgi_sprite`) | driver + `tgi_ioctl.s` | ~40 cycles per call on the hot path |

Worst structural cost: the jump table references **every** driver entry point, so ld65
links the entire 1.1 KB driver (including BAR/LINE/pixel code and the 768-byte font) into
every TGI program regardless of what it calls. Smart linking is defeated by design.

## 2. New architecture: direct-call static library

The kernel/vector/driver triple collapses to one C-callable routine per function, living
in `libsrc/lynx/tgi/`, one object module per function (or small cohesive group) so ld65's
smart linking finally applies. No header, no vectors, no install step, no module format.

### 2.1 Surviving API

| Function | Implementation notes |
|---|---|
| `tgi_init()` | Absorbs old INSTALL+INIT+kernel `tgi_init`: enable VBL timer IRQ, set collision buffer regs ($A058), reset text defaults, set draw/view page 0, default palette, color black (changed from the old driver's white; see §2.8). Cannot fail (fixed hardware) → returns `void`, no error path. |
| `tgi_clear()` | The cls sprite + shared `draw_sprite` core. Clears in the current draw colour; see §2.8. |
| `tgi_clearrows(first, count)` | Lynx extension: clear a horizontal band of rows in the current draw colour. See §2.8. |
| `tgi_sprite(scb)` | **Promoted from `tgi_ioctl(0,·)` to a real `__fastcall__` function**: `sta/stx` SCB pointer → `draw_sprite`. This is the hot call (once per frame per chain in `breakout.c`); drops the C ioctl wrapper + vector + cmp-chain (~40 cycles/call). |
| `tgi_flip()` | Promoted from ioctl 1. |
| `tgi_setbgcolor(c)` | Promoted from ioctl 2. |
| `tgi_setframerate(r)` | Promoted from ioctl 3. Returns `unsigned char` (0 = ok, nonzero = bad rate) instead of setting `tgi_error`. |
| `tgi_busy()` / `tgi_updatedisplay()` | Promoted from ioctl 4. |
| `tgi_setcollisiondetection(on)` | Promoted from ioctl 5. |
| `tgi_setviewpage(p)` / `tgi_setdrawpage(p)` | Direct; page addresses stay $E018/$C038. |
| `tgi_setbpp(bpp)` | Select display depth via DISPCTL B2: 4 (default) or 2 bits/pixel. See §2.7. |
| `tgi_setcolor(c)` / `tgi_getcolor()` | Direct; range check replaced by `and #$0F`. |
| `tgi_setpalette(p)` / `tgi_getpalette()` / `tgi_getdefpalette()` | Direct. |
| `tgi_gotoxy(x,y)` | Kept solely as text cursor positioning for `tgi_outtext`. |
| `tgi_settextstyle/settextdir/settextscale` | Bitmap-font only; see §2.3. |
| `tgi_outtext(s)` / `tgi_outtextxy(x,y,s)` | Direct; font + 169-byte `text_bitmap` buffer (`8*(1+20)+1`, §2.3.1) link only when used. |
| `tgi_gettextwidth(s)` / `tgi_gettextheight(s)` | Bitmap math only (8 × scale × strlen). |

The `lynx.h` macros (`tgi_sprite`, `tgi_flip`, …) become declarations of the real
functions; existing call sites compile unchanged. `tgi_ioctl` itself is removed.

### 2.2 Constants replace queries

Resolution and capabilities are compile-time facts. `tgi.h` gains:

```c
#define TGI_XRES        160
#define TGI_YRES        102
#define TGI_COLORCOUNT  16
#define TGI_PAGECOUNT   2
```

`tgi_getxres()`, `tgi_getyres()`, `tgi_getmaxx()`, `tgi_getmaxy()`, `tgi_getcolorcount()`,
`tgi_getmaxcolor()`, `tgi_getpagecount()` become macros over these (source compatibility
for free, zero code). Their kernel wrapper files and the RAM mirror variables are deleted.
`tgi_getaspectratio`/`tgi_setaspectratio` are deleted outright — their only consumers were
the circle/ellipse emulations.

There is deliberately no `TGI_FONTWIDTH`/`TGI_FONTHEIGHT` constant. Multiple fonts of
differing dimensions are selectable at run time (`TGI_FONT_BITMAP` 8×8, `TGI_FONT_COMPACT`
5×5 at 6-px pitch, and the proportional font with per-glyph widths), so a single fixed cell
size would be meaningless. Code that needs text extents calls `tgi_gettextwidth` /
`tgi_gettextheight`, which account for the active font and the current text scale.

### 2.3 Text path

The bitmap font path is kept intact (glyph-strip build into `text_bitmap`, drawn as one
sprite). Vector fonts are removed entirely, which also removes the 8.8 dual-scale
machinery in the kernel (`_tgi_textscalew/h`, `_tgi_charwidth/height` interplay,
`TGI_BM_FONT_FINESCALE` flag tests).

Text scaling becomes a single 8.8 word per axis passed straight into the text sprite's
`sx/sy` fields. Note: the current driver only honors the integer part (`TEXTMAGX` →
`text_sx+1`); since Suzy sprite scaling is natively 8.8, passing the full word gives true
fractional text scaling *at zero cost*. `tgi_gettextwidth` computes
`(len*8*scale)>>8` — a natural client for the §2.6 Suzy multiply helpers in the codegen
design. The 20-character-per-call draw limit is pre-existing; see §2.3.1.

#### 2.3.1 The 20-character draw cap

`tgi_outtext` / `tgi_outtextxy` draw **at most 20 characters per call**. The limit is a
buffer bound, not an arbitrary one: the whole string is composed into a single fixed,
statically-allocated glyph strip (`text_bitmap`) and blitted as one Suzy sprite. There is
no heap and no second pass, so the buffer is sized for the worst case the renderers can
emit — `8 * (1 + 20) + 1` bytes for the 8×8 font (8 rows of one offset byte plus up to 20
glyph columns, plus a zero-offset terminator); the compact 5×5 strip (`build5x5`) fits
inside the same allocation. See `tgi-text.s`.

The cap is enforced **once**, in the shared `_tgi_outtext` prologue, by the strlen loop:

```
        ldy     #<-1
@L2:    iny
        cpy     #20             ; saturate STRLEN at 20
        beq     @L3
        lda     (STRPTR),y
        bne     @L2
@L3:    sty     STRLEN
```

Because the scan stops at index 20, a long string is never even read past the 20th byte.
Both font builders (`build8x8`, `build5x5`) and `draw_and_advance` consume `STRLEN`, so the
cap holds regardless of which font `tgi_setfont` has selected. Characters 21+ are **silently
dropped** — there is no wrap, no truncation marker, and no error (the `tgi_error` model is
removed, §2.5).

**Caveat — `tgi_gettextwidth` is *not* capped.** It calls the full `_strlen` and returns the
pixel width of the *entire* string, so for inputs longer than 20 characters it over-reports
what `tgi_outtext` will actually paint. (`tgi_gettextheight` is length-independent, so it is
unaffected.) Callers that lay out runs longer than 20 characters must split the text into
chunks of ≤20 characters themselves and advance the cursor per chunk — for example via
successive `tgi_outtextxy` calls, or `tgi_outtext` after a `tgi_gotoxy`, rather than relying
on a single `tgi_gettextwidth` over the whole string.

### 2.4 IRQ hook

The self-modifying `tgi_libref` stub exists only so a *runtime-loaded* driver can attach
its VBL handler. Statically, the driver's `irq` routine (display swap on `SWAPREQUEST`)
is exported directly with `.interruptor` from the page-flip module. `tgi_irq.s`, the
libref patching in INSTALL/UNINSTALL, and `tgi_libref` all disappear. The handler is
placed in the same module as `tgi_updatedisplay`/`tgi_setviewpage` so it is linked exactly
when double-buffer swapping is used.

### 2.5 Error model: removed

With loading, installation, palettes-always-supported, and infallible init, the only
fallible call left is `tgi_setframerate` (invalid rate), which now reports via return
value. `tgi_geterror`, `tgi_geterrormsg`, `_tgi_error`, `tgi-error.inc`, and the
per-driver `ERROR` byte are deleted (~250 bytes incl. message strings).

### 2.6 Module split (smart-linking layout)

```
libsrc/lynx/tgi/
  tgi-core.s      draw_sprite, DRAWINDEX, DRAWPAGE/VIEWPAGE vars   (always linked)
  tgi-init.s      tgi_init
  tgi-clear.s     tgi_clear, tgi_clearrows + shared cls sprite
  tgi-sprite.s    tgi_sprite
  tgi-page.s      setviewpage, setdrawpage, flip, busy, updatedisplay, irq (.interruptor)
  tgi-color.s     setcolor, getcolor, setbgcolor
  tgi-palette.s   setpalette, getpalette, getdefpalette + DEFPALETTE
  tgi-rate.s      setframerate
  tgi-collision.s setcollisiondetection
  tgi-text.s      outtext, outtextxy, gotoxy, settextstyle/dir/scale,
                  gettextwidth/height, text_bitmap
  tgi-font.s      the 768-byte font (referenced only by tgi-text.s)
```

A sprite-only game links core+init+clear+sprite+page ≈ 350 bytes instead of today's
~1.6 KB (driver + kernel + install). A game with text adds ~1.2 KB (font + builder).

### 2.7 Selectable display depth: `tgi_setbpp(4|2)` (DISPCTL B2)

`DISPCTL` ($FD92) B2 selects how Mikey's display DMA interprets the buffer: 1 = 4-bit
(normal), 0 = 2-bit. The API exposes this:

```c
void __fastcall__ tgi_setbpp (unsigned char bpp);   /* 4 (default) or 2 */
```

Implementation: update bit 2 of the `__viddma` shadow, write `DISPCTL`, re-issue
`DISPADR` for the current view page (in flip mode the end-of-buffer offset is **4079**
in 2bpp vs 8159 in 4bpp — `tgi_flip` and `SETVIEWPAGE` consult the mode). One mode byte
in `tgi-page.s`; `tgi_init` sets 4bpp (`$0D`, the value `crt0.s:101` already seeds).

**What 2bpp mode is — and is not.** Mikey then reads 40 bytes/line × 102 lines =
**4080 bytes/page**, 4 pixels/byte, still 160×102 (the §2.2 resolution constants are
depth-independent). But Suzy is unaffected by `DISPCTL`: the sprite engine always paints
4-bit nibbles at 80 bytes/line into its hardwired build buffer — sprite *source* data may
be 1–4 bpp (SPRCTL0 B7,B6), but its *output* is always 4bpp. The spec's Display chapter
is explicit that the display system "has 4 bits of pen number per pixel" and prescribes
`$0D` as the normal value; the 2-bit/mono logic is a development leftover. Consequently:

- 2bpp is a **CPU-rendered framebuffer mode**: the program writes the 4080-byte buffer
  itself. `tgi_sprite`, `tgi_outtext`, and `tgi_clear`-via-sprite produce 4bpp data that
  a 2bpp display scans out garbled (each nibble becomes two 2-bit pixels, each Suzy line
  spans two display lines). Documented as caller responsibility, not guarded in code.
  Exception: a pen-0 sprite fill writes $00 bytes, which reads as pen 0 at any depth, so
  `tgi_clear` with color 0 still clears validly.
- Page base addresses stay $E018/$C038 in both modes; in 2bpp the upper 4080 bytes of
  each page are free for application use.
- Palette/collision/IRQ-swap machinery is depth-independent and works unchanged.

**To verify on hardware** (the spec documents none of this; emulators — Handy,
Mednafen — may not implement 2bpp at all): pixel order within the byte (assumed
MSB-first, matching 4bpp nibble order), whether 2-bit pen numbers index palette entries
0–3, and that PBKUP/line timing needs no adjustment for the halved DMA fetch. A small
test ROM that hand-fills a 2bpp buffer with a known pattern settles all three.

### 2.8 Coloured clears and partial clears: `tgi_clear` honours the draw colour, `tgi_clearrows`

```c
void tgi_clear (void);
/* Clear the whole draw page in the current draw colour. */

void __fastcall__ tgi_clearrows (unsigned char first, unsigned char count);
/* Clear rows [first, first+count) of the draw page in the current draw
** colour. Lynx extension (like tgi_setbpp). */
```

Both clear in the colour last set with `tgi_setcolor()` — no separate "clear colour"
state. `tgi_clear(); /* after tgi_setcolor(BG_DEAD) */` paints the screen `BG_DEAD`.

**Mechanism.** The existing cls sprite (1×1 1bpp pixel, hardware-scaled) serves both
functions; everything stays in `tgi-clear.s`. Per call, before `tgi_draw_sprite`:

- **Pen byte** ← `(c << 4) | c` with `c = tgi_drawindex` (new import from `tgi-core.s`).
  Duplicating the colour into both nibbles of the 1bpp pen table makes the result
  independent of which pixel index the stretched source pixel carries.
- **Geometry**: `y` ← `first` (high byte 0), `sy` ← `count` in the integer half of the
  8.8 word; `x = 0` / `sx = $A000` stay static. `tgi_clear` becomes a 4-byte preamble
  (`first = 0, count = 102`) falling into the common path. Cost: ~25 bytes over today.

**Edge cases.**

- `count == 0`: explicit `rts` guard — Suzy's behaviour at `sy = 0` is not trusted.
- `first + count > 102` (or `first > 101`): Suzy clips at the screen edge; partial or
  fully offscreen bands draw nothing extra. No software range checks.
- **Collision interplay**: because both functions share `tgi_cls_sprite`, the
  `tgi_setcollisiondetection` type/SPRCOLL rewrite applies unchanged — with collision
  detection on, a partial clear also erases the matching rows of the collision buffer.

**Behaviour change — flag loudly.** `tgi_clear` previously always filled with pen 0.
To keep the init→clear idiom black-clearing, `tgi_init`'s default draw colour changes
from white to **black** (§2.1) — `tgi_init(); tgi_clear();` behaves exactly as before.
The flip side: programs that drew (text, sprites) after init *without* an explicit
`tgi_setcolor` relied on the old white default and now draw black-on-black — invisible,
silently. Both changes are runtime-silent, not compile errors — call them out in the
`tgi.h` comments for `tgi_init` and `tgi_clear`, and audit the samples (`breakout.c`,
`lynxdemo.c`, etc.) for missing `tgi_setcolor(COLOR_WHITE)` in the same commit.

**Alternative considered**: keying clears off `tgi_bgindex` (`tgi_setbgcolor`) instead.
Rejected — one colour mechanism (`tgi_setcolor`) for all drawing as specified;
`tgi_bgindex` stays text-only.

**2bpp caveat (§2.7).** A colour-`c` clear writes bytes `$cc`. At 2bpp display depth
that scans out uniformly only when both bit-pairs of the nibble match:
`c ∈ {$0, $5, $A, $F}` → 2bpp pens 0–3. Pen 0 remains valid at any depth; other values
produce a two-colour vertical stripe pattern. Documented, not guarded.

**Touch list**: `tgi-clear.s` (rework), `tgi-init.s` (default colour white → black),
`include/tgi.h` (prototype + revised `tgi_init`/`tgi_clear` comments), `doc/tgi.sgml`,
samples (add `tgi_setcolor(COLOR_WHITE)` where the white default was assumed).

## 3. Deletions

**`libsrc/tgi/` (entire directory eventually):** `tgi-kernel.s`, `tgi_load.s`,
`tgi_unload.s`, `tgi_init.s` (reimplemented in lynx tree), `tgi_done.s`, `tgi_ioctl.s`,
`tgi_bar.s`, `tgi_circle.s`, `tgi_ellipse.s`, `tgi_arc.c`, `tgi_pieslice.c`,
`tgi_line.s`, `tgi_lineto.s`, `tgi_clippedline.s`, `tgi_outcode.s`, `tgi_linepop.s`,
`tgi_setpixel.s`, `tgi_getpixel.s`, `tgi_curtoxy.s`, `tgi_popxy.s`, `tgi_popxy2.s`,
`tgi_getset.s`, `tgi_imulround.s`, `tgi_getaspectratio.s`, `tgi_setaspectratio.s`,
`tgi_geterror.s`, `tgi_geterrormsg.s`, `tgi_load_vectorfont.c`,
`tgi_install_vectorfont.s`, `tgi_free_vectorfont.s`, `tgi_vectorchar.s`, all `tgi_get*`
query wrappers, `tgidrv_line.inc`.

**`libsrc/lynx/`:** `tgi/lynx-160-102-16.s` (split per §2.6; BAR/LINE/SETPIXEL/GETPIXEL
sections and the header/INSTALL/UNINSTALL dropped), `tgi_stat_stddrv.s`, `tgi_stddrv.s`,
`tgi_irq.s`, the tgi entry in `libref.s` stays untouched (joy/ser only).

**Headers:** `tgi/tgi-vectorfont.h`, `tgi/tgi-error.h`, `tgi/tgi-kernel.h`;
`asminc/tgi-kernel.inc` shrinks to constants + zp/text variable globals;
`asminc/tgi-error.inc`, `asminc/tgi-vectorfont.inc` deleted. `tgi.h` rewritten (~80
lines): surviving functions, constants of §2.2, color macros.

**Build:** drop `tgi` from `DRVTYPES` in `libsrc/Makefile`; stop shipping
`target/lynx/drv/tgi/lynx-160-102-16.tgi`.

**Samples:** remove `tgi_install (tgi_static_stddrv)` lines; `lynxdemo.c` loses its
`tgi_line`/`tgi_circle` calls (replace with a sprite + text demo); `breakout.c` compiles
unchanged apart from the install line.

## 4. Expected impact

| Item | Today | After |
|---|---|---|
| Per-call overhead | wrapper + RAM `jmp` (+3 cycles) | direct `jsr` |
| `tgi_sprite` per frame | ioctl wrapper + vector + cmp-chain ≈ 60+ cycles | ≈ 15 cycles |
| RAM (vectors + mirrors + header copies) | ~75 bytes | 0 |
| Code always linked | ~1.6 KB (whole driver + kernel) | only what's called (~350 B min) |
| Startup | install (sig check, 2 copy loops) + init | init only |
| API entry points in `tgi.h` | 47 | 24 (7 of them zero-cost macros) |

## 5. Hardware "perniciousness" rules (spec ch. 3) applied

The spec's "Software Related Hardware Perniciousness" chapter imposes rules the design
must obey explicitly, not by accident:

- **Unsafe SCB register access (3.1.1).** All Suzy SCB registers marked (U) may not be
  read *or* written while the sprite engine or math unit is running. TGI complies by
  construction: `draw_sprite` is synchronous (busy-waits on SPRSYS sprite-working before
  returning), and the Suzy math helpers (`LYNX_CODEGEN_DESIGN.md` §2.6) poll math-working
  before reads. So every TGI entry point starts with the engine provably idle. This is
  now a stated invariant: any future *asynchronous* sprite API must add explicit SPRSYS
  polls before every (U) access in the library.
- **No back-to-back Suzy accesses (3.1.2).** A single instruction producing two Suzy
  accesses "will probably break Suzy" — i.e. **no RMW opcodes on $FCxx**: no
  `INC/DEC/ASL/LSR/ROL/ROR` and notably no `TRB/TSB`. Library rule: Suzy registers are
  written with plain `STA` only; state mutations go through the zp shadows (`__sprsys`)
  first. Cross-impact on the codegen work: cc65's stock `Opt65C02BitOps` pass rewrites
  `LDA mem / ORA|AND #imm / STA mem` into `TSB/TRB mem` — applied to a C
  `*(volatile uint8_t*)0xFCxx |= m` this emits a forbidden Suzy RMW. The pass needs an
  address-range guard excluding $FC00–$FCFF (action item for `LYNX_CODEGEN_DESIGN.md`
  §2.2; also document that compound assignment to Suzy registers is unsafe in C).
- **Write-only registers and read-back chains (3.1.3 + appendix legend).** Several
  registers TGI touches are write-only (`DISPCTL`, SPRSYS's write view) or read back as
  something else. The design keeps the existing shadow pattern — `tgi_flip`,
  `tgi_setbpp`, `tgi_setcollisiondetection` all modify `__viddma`/`__sprsys` shadows and
  store. C callers must avoid chained/compound assignment on hardware addresses (the
  spec's own compiler warning); never read a write-only register to "preserve bits".
- **Cart-write 12-tick rule (3.1.4).** After a game-cart write, no Suzy access for 12
  ticks. TGI never touches the cart; the constraint lives in the cart I/O code
  (`libsrc/lynx/read.s` et al.), but `tgi.h` documentation warns against interleaving
  raw cart strobes with sprite calls.
- **Palette-at-$xxFA hardware bug (3.1.5).** An SCB pen-index palette starting at
  address $xxFA loses its last 2 bytes (pens C–F keep stale values). TGI's internal
  sprites use 1-byte palettes (1bpp source data) and cannot straddle the break, but
  `tgi_sprite` passes user SCBs straight to hardware: document in `tgi.h` that an
  8-byte SCB palette must not begin at $xxFA (pad/align the SCB; ld65 `.align` or
  segment placement suffices). Worth a one-line check in a debug build of `tgi_sprite`.
- **Sprite-data "last-pixel" pad-byte bug (spec ch. 6; verified on GearLynx).** Suzy's
  sprite-data shift register drops the final pixel group of any scan line whose data ends
  on a byte boundary. **LITERAL** lines always end on a byte boundary, so every literal
  line loses its rightmost source pixel unless padded — measured on GearLynx, lynxdemo's
  8-px ball renders 14 px wide (at 2×) without a pad and 16 px with one. The fix: append
  one pad byte per literal line **and** add 1 to the line's offset byte. The pad's pixels
  must resolve to **pen 0** (transparent on a normal sprite), which is decided on the pen
  *after* the penpal lookup, not on the raw pixel value: use `$00` for the usual
  value-0-transparent idiom, but a value the sprite's penpal maps to pen 0 when value 0 is
  an opaque colour (setbpp's bands use `$22`), and `$FF` for the active-low 8×8 font in
  `tgi-text.s`. **PACKED** lines need the pad only when the last meaningful bit
  lands on bit 0 of a byte (~1/8 of lines). Two corollaries: (a) literal lines at **3bpp**
  must additionally be padded to a whole-byte pixel count, since 3bpp doesn't tile bytes
  evenly and the spec paints the leftover bits as 1-2 stray pixels
  (`BPP_2`/`BPP_4`/`BPP_1` are exempt from *that* hazard); and (b) any future
  packed-sprite authoring or import path must apply the pad-byte rule — preferably via an
  offline `sprpck`-style packer rather than by hand. Full analysis, with the GearLynx
  measurements, in `LYNX_SPRITE_PADBYTE_DESIGN.md`.
- **"Please don't" — undefined bits (3.2).** The 2bpp mode of §2.7 uses a DISPCTL bit
  the Display chapter disowns; under the spec's own guidance this is exactly the kind of
  unapproved use it asks designers not to rely on. The feature stays (explicitly
  requested) but remains opt-in, defaults to the prescribed $0D, and is documented as
  outside spec guarantees pending the §2.7 hardware verification.
- **Timer handling (3.3.1/3.3.2).** `tgi_init` enables the VBL interrupt with a
  read-modify of Mikey's `VTIMCTLA` (Mikey RMW is legal; the Suzy ban doesn't apply) and
  must never leave 'reset timer done' (B6) set — it's a level signal that can stream
  interrupts. `tgi_setframerate` writes only backup registers (`HTIMBKUP`, `PBKUP`),
  which is the safe subset. If a future function needs to clear a timer-done state, it
  must use the spec's pulse pattern: set B6 with the int-enable bit cleared, then
  restore both.

## 6. Risks and constraints

- **Page addresses are config-coupled.** $E018/$C038 and the $A058 collision buffer
  mirror `cfg/lynx.cfg`'s memory map. Unchanged behavior, but now is the moment to
  document the coupling in `tgi-page.s`/`tgi-init.s` headers (follow-up option: export
  them from the linker config).
- **Synchronous-draw contract.** `draw_sprite` still busy-waits on `SPRSYS` with
  `CPUSLEEP`. The Suzy math helpers (`LYNX_CODEGEN_DESIGN.md` §2.6) rely on the sprite
  engine being idle during C arithmetic — this design keeps that invariant. Any future
  *asynchronous* `tgi_sprite` variant must revisit that section first.
- **`.interruptor` linkage.** The VBL swap handler must end up in the interruptor chain
  only when its module links; verify a program that never flips pages really drops it.
- **API break.** Upstream cc65 Lynx code using `tgi_line`/`tgi_setpixel`/ioctl no longer
  compiles. Intentional; compile errors (not silent behavior change) are the failure mode.
- **`tgi_busy` semantics caveat (pre-existing):** it reports a *pending swap request*,
  not sprite-engine business. Keep the name, document it.
- **2bpp mode is unverified territory.** No commercial software or spec text exercises
  DISPCTL B2=0; emulators may render it wrong or not at all, so §2.7's verification
  items need real hardware before the mode is documented as supported.

## 7. Verification plan

1. Rebuild `lynx.lib`; link `breakout.c` and the revised `lynxdemo.c`; run in
   Handy/Mednafen — visual parity, collision toggle, frame-rate switch, double-buffer
   flips without tearing.
2. `.map` diff: confirm no `tgi_install`/jumpvector symbols remain; confirm a
   text-free test program does not link `tgi-font.s`/`tgi-text.s`, and a flip-free
   program does not link the interruptor.
3. Cycle measurement of `tgi_sprite` before/after (Mednafen debugger), validating the
   §4 estimate.
4. Grep audit: no remaining references to `tgi_drv`, `TGI_HDR`, `tgi-error`, `modload`
   from TGI code (`modload` itself stays until joy/ser get the same treatment).
5. Perniciousness audit (§5): grep library output for RMW opcodes
   (`INC|DEC|ASL|LSR|ROL|ROR|TRB|TSB`) targeting $FCxx; confirm every Suzy/Mikey
   write-only register store is preceded by its shadow update; confirm no internal SCB
   palette can land at $xxFA.

## 8. Implementation order

Each step shippable: (1) split the driver into the §2.6 modules with direct C entry
symbols, keep old kernel temporarily delegating; (2) rewrite `tgi_init`,
promote the six ioctl functions, switch `lynx.h` macros to declarations; (3) delete the
kernel, loader, vector-font, primitive, and error files; rewrite `tgi.h` with constants;
(4) Makefile/`DRVTYPES` cleanup, samples, `.map`-based size report.

## 9. Out of scope / follow-ups

- Same de-driverization for `joy` (lynx-stdjoy) and `ser` (comlynx); after both,
  `modload.s`/`modfree.s` and o65 module support leave the library entirely, and
  `libref.s` disappears.
- Fractional 8.8 text scaling exposure in `tgi_settextscale` (hardware-free, §2.3).
- Collision-result readback API (`tgi_sprite` currently ignores the collision buffer
  beyond enabling/disabling it) — natural next feature for game use.
