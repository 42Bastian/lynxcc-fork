# LYNX_CART_SIZES_DESIGN

Design for fixed-size cart image generation in **lynxcc**: three linker
configurations derived from `cfg/lynx.cfg` that pad the linked output to an
exact physical cart size — 128 KB, 256 KB, or 512 KB — using the ld65 memory
area `fill` attribute. Status: **implemented** — `cfg/lynx-128k.cfg`,
`cfg/lynx-256k.cfg`, `cfg/lynx-512k.cfg`; docs in `doc/cart.html` §1.1. Verified
in‑sandbox: exact file sizes for trivial and BSS‑heavy programs, payload prefix
byte‑identical to `lynx.cfg`, and a padded `lynxdemo` boots to a pixel‑identical
GearLynx frame (same golden SHA‑256 as the unpadded build).

## 1. Motivation

`cfg/lynx.cfg` today writes only the bytes it needs. The `%O` output file is
laid out as the 64‑byte LNX header, followed by the boot loader, the directory,
and the used portion of `MAIN`:

```
[ EXEHDR 64 ] [ BOOTLDR 203 ] [ DIRECTORY 8 ] [ MAIN: used bytes only ]
```

Emulators (Handy, Mednafen) are happy with this because they read the LNX
header, take `__BANK0BLOCKSIZE__` as the bank‑0 page size, and derive cart
addressing from it. But two workflows want the image to be *exactly* the size of
the physical part it targets:

- burning a real EPROM / building a repro cart, where the file must be the exact
  device size, zero‑padded;
- tooling and test fixtures that assume a canonical 128/256/512 KB length.

The `tools/lnx` `bll` command already canonicalizes these same three sizes for
the BLL download path (`BLL_128K/256K/512K` = 131072 / 262144 / 524288, block
sizes 512 / 1024 / 2048; it `calloc`s the full size and zero‑pads). This design
brings the equivalent capability to the *standard* cart path (EXEHDR + bootldr +
directory) so it is available directly at link time via `-C`, without a
post‑link step. The user‑visible knob is the ld65 `fill` attribute, documented
locally in `doc/ld65.html` §5.6.

## 2. Cart size ↔ block size

The physical ROM size on the Lynx is `__BANK0BLOCKSIZE__ × 256` (256 blocks per
bank). This is the same table already printed in `doc/cart.html` §1:

| Config              | `__BANK0BLOCKSIZE__` | Block size | Physical ROM | Typical use                          |
| ------------------- | -------------------- | ---------- | ------------ | ------------------------------------ |
| `cfg/lynx-128k.cfg` | `$0200`              | 512 B      | 128 KB       | Old commercial games (e.g. Warbirds) |
| `cfg/lynx-256k.cfg` | `$0400`              | 1024 B     | 256 KB       | Most homebrew; newer games (Lemmings)|
| `cfg/lynx-512k.cfg` | `$0800`              | 2048 B     | 512 KB       | Largest games (EOTB)                 |

The default `cfg/lynx.cfg` already uses `__BANK0BLOCKSIZE__ = $0400`, i.e. its
header *declares* a 256 KB cart, but it does not pad the file. `lynx-256k.cfg`
therefore differs from `lynx.cfg` only in that it pads to the full 256 KB.

Convention: the 64‑byte LNX header is emulator metadata and is **not** part of
the 256 blocks. The padded blocks region (bootldr + directory + MAIN + fill) is
exactly `__BANK0BLOCKSIZE__ × 256`; the resulting `%O` file is
`64 + __BANK0BLOCKSIZE__ × 256` bytes. A strictly headerless, exact‑device‑size
image (as `lnx bll` produces) remains the job of the `lnx` tool.

## 3. Why the padding must be a separate memory area

The obvious idea — grow `MAIN` and let `fill = yes` pad it — does **not** work,
because the cart pad is far larger than RAM and `MAIN`'s top is already pinned to
a fixed hardware boundary.

Per `doc/memory.html` §1, the C runtime stack sits just below screen buffer 1 at
`$C037` (or `$A057` with collision detection) and grows down — a fixed location
that does **not** depend on the program's size. `runtime/lynx/crt0.s` sets the
stack pointer from `__MAIN_START__ + __MAIN_SIZE__ + __STACKSIZE__`, and with the
stock `MAIN` size of `$BE38 − __STACKSIZE__` the `__STACKSIZE__` term cancels:

```
$0200 + ($BE38 − __STACKSIZE__) + __STACKSIZE__ = $C038
```

so the top of `MAIN` is calibrated to land exactly at `$C038`, the bottom of the
screen buffer, with `__STACKSIZE__` reserving the gap below it. There is no free
RAM above `MAIN` to grow into — the screen buffers occupy `$C038–$FFF7` — so a
128 KB+ pad can never live in RAM. It must be a ROM‑only area. `MAIN` keeps its
current size.

### 3.1 Why not just resize `MAIN`?

A natural first question is why the pad can't simply be absorbed by making `MAIN`
bigger. Two reasons:

- **Capacity.** `MAIN` already extends all the way up to the stack — its top is
  `$C038`, and the program, heap, and C stack all share that one window (program
  and data grow up from `$0200`, the heap continues up, the stack grows down from
  `$C038`). There is nothing above `$C038` but the screen buffers, so even at its
  absolute maximum `MAIN` tops out near 48 KB. The cart images are 128–512 KB,
  leaving 80 KB to 464 KB of padding that has *no RAM address at all*. A memory
  area's file output cannot exceed its address span, and there is no free address
  span to give it, so the pad is inherently ROM‑only.
- **Coupling.** `__MAIN_SIZE__` is load‑bearing: crt0 derives the stack pointer
  from it and the whole RAM layout is pinned to `$C038` through it. Resizing
  `MAIN` to 128 KB would make crt0 compute `$0200 + $20000 + __STACKSIZE__`,
  which wraps past `$FFFF` and corrupts the stack — crt0's stack math would have
  to be rewritten too. Appending a throwaway `ROMFILL` area leaves `MAIN`, crt0,
  and the entire RAM layout untouched.

The padding therefore lives in a dedicated ROM‑only area, `ROMFILL`, appended
after `MAIN` in `%O` order. ld65 writes memory areas that share a `file` in the
order they are declared (`doc/ld65.html` §5.1), so declaring `ROMFILL` last puts
the pad bytes at the end of the image.

For the total to be an *exact* constant, everything before `ROMFILL` must be a
constant number of bytes. `EXEHDR` (64) and `DIRECTORY` (8) already are.
`BOOTLDR` is a fixed loader of `__STARTOFDIRECTORY__` (= `$CB` = 203) bytes; we
mark it `fill = yes` so any slack is written deterministically. `MAIN` is made
`fill = yes` so it always emits its full `__MAIN_SIZE__` bytes rather than only
the used prefix. With all preceding areas constant, `ROMFILL`'s size is a
constant expression and the file lands on the exact target every time.

Note that `fill = yes` on `MAIN` changes only the *bytes written to the file*; it
does not change `__MAIN_SIZE__`, so the crt0 stack calculation is unaffected and
the RAM layout is byte‑for‑byte the same as `lynx.cfg`. It is also safe on the
boot side: the directory entry in `runtime/lynx/defdir.s` sets the loader's copy
length (`len0`) from the real segment sizes
(`__STARTUP_SIZE__ + __ONCE_SIZE__ + __CODE_SIZE__ + __DATA_SIZE__ +
__RODATA_SIZE__ + __LOWCODE_SIZE__`), not from `__MAIN_SIZE__`, so the bootloader
still copies only the used code/data to `$0200` and never reads the fill tail.

## 4. The `ROMFILL` size expression

Bytes in the blocks region before the fill:

```
BOOTLDR  = __STARTOFDIRECTORY__          ; $CB  = 203
DIRECTORY = 8
MAIN     = $BE38 − __STACKSIZE__          ; = $BA38 = 47672 at the default 1 KB stack
```

so

```
__ROMFILLSIZE__ = __CARTSIZE__ − ( ($BE38 − __STACKSIZE__) + __STARTOFDIRECTORY__ + 8 )
```

where `__CARTSIZE__ = __BANK0BLOCKSIZE__ × 256`. Expressing the size purely in
terms of the `SYMBOLS`‑section values (`__CARTSIZE__`, `__STACKSIZE__`,
`__STARTOFDIRECTORY__`) means the fill self‑adjusts if the stack size or loader
size is ever changed — no magic literal to keep in sync.

For reference, at the default 1 KB stack (`MAIN` full = `$BA38` = 47672, overhead
before fill = 47672 + 203 + 8 = 47883):

| Config              | `__CARTSIZE__` | `__ROMFILLSIZE__`    | `%O` file size          |
| ------------------- | -------------- | -------------------- | ----------------------- |
| `lynx-128k.cfg`     | `$20000` (131072) | 83189  (`$144F5`) | 131136 (64 + 131072)    |
| `lynx-256k.cfg`     | `$40000` (262144) | 214261 (`$344F5`) | 262208 (64 + 262144)    |
| `lynx-512k.cfg`     | `$80000` (524288) | 476405 (`$744F5`) | 524352 (64 + 524288)    |

## 5. Proposed configuration files

Each of the three new files is a copy of `cfg/lynx.cfg` with: (a) the correct
`__BANK0BLOCKSIZE__`, (b) a new weak `__CARTSIZE__` symbol, (c) `fill = yes` on
`BOOT` and `MAIN`, and (d) the trailing `ROMFILL` area plus its `FILL` segment.
Everything else — `ZP`, `HEADER`, the segment map, and the `FEATURES` block —
is unchanged.

`cfg/lynx-128k.cfg` (the 256 KB and 512 KB variants differ only in the two
marked lines):

```
SYMBOLS {
    __STACKSIZE__:        type = weak, value = $0400; # 1k stack
    __STARTOFDIRECTORY__: type = weak, value = $00CB; # start just after loader
    __BANK0BLOCKSIZE__:   type = weak, value = $0200; # 512-byte blocks  ->  *** per size ***
    __BANK1BLOCKSIZE__:   type = weak, value = $0000; # bank 1 block size
    __CARTSIZE__:         type = weak, value = $20000; # 128 KB = 512 x 256  ->  *** per size ***
    __EXEHDR__:           type = import;
    __BOOTLDR__:          type = import;
    __DEFDIR__:           type = import;
}
MEMORY {
    ZP:      file = "", define = yes, start = $0000, size = $0100;
    HEADER:  file = %O,               start = $0000, size = $0040;
    BOOT:    file = %O,               start = $0200, size = __STARTOFDIRECTORY__, fill = yes;
    DIR:     file = %O,               start = $0000, size = 8;
    MAIN:    file = %O, define = yes, start = $0200, size = $BE38 - __STACKSIZE__, fill = yes;
    ROMFILL: file = %O,               start = $0000,
             size = __CARTSIZE__ - (($BE38 - __STACKSIZE__) + __STARTOFDIRECTORY__ + 8),
             fill = yes, fillval = $42;
}
SEGMENTS {
    ZEROPAGE:  load = ZP,      type = zp;
    EXTZP:     load = ZP,      type = zp,                optional = yes;
    APPZP:     load = ZP,      type = zp,                optional = yes;
    EXEHDR:    load = HEADER,  type = ro;
    BOOTLDR:   load = BOOT,    type = ro;
    DIRECTORY: load = DIR,     type = ro;
    STARTUP:   load = MAIN,    type = ro,  define = yes;
    LOWCODE:   load = MAIN,    type = ro,  define = yes, optional = yes;
    ONCE:      load = MAIN,    type = ro,  define = yes, optional = yes;
    CODE:      load = MAIN,    type = ro,  define = yes;
    RODATA:    load = MAIN,    type = ro,  define = yes;
    DATA:      load = MAIN,    type = rw,  define = yes;
    BSS:       load = MAIN,    type = bss, define = yes;
}
FEATURES {
    # ... identical to cfg/lynx.cfg (CONDES constructor / destructor / interruptor) ...
}
```

ld65 emits the pad bytes for a `fill = yes` area even when no segment is assigned
to it, so `ROMFILL` needs no segment of its own (verified at link time). `MAIN`
is left with its default `$00` fill value; only `ROMFILL` uses `$42`.

The finished image therefore has two padding regions: the never‑loaded tail of
`MAIN` (from the end of the program up to the RAM window, `$00`) followed by
`ROMFILL` (`$42`) out to the cart size. `ROMFILL`'s value is free to choose — it
is never copied into RAM (the directory's `len0` bounds the loader) and never
executed, so it has no functional meaning, unlike the `$00`/BRK fill conventional
inside executable areas. `$42` ('B') is picked as a visible marker so the pure
padding stands out in a hex dump; `MAIN` keeps `$00` because part of its span can
fall inside the copied `len0`. Change either `fillval` freely.

Per‑size lines:

| File                | `__BANK0BLOCKSIZE__` | `__CARTSIZE__` |
| ------------------- | -------------------- | -------------- |
| `cfg/lynx-128k.cfg` | `$0200`              | `$20000`       |
| `cfg/lynx-256k.cfg` | `$0400`              | `$40000`       |
| `cfg/lynx-512k.cfg` | `$0800`              | `$80000`       |

## 6. Using the configs

Selection is the standard ld65 / cl65 `-C` (`--config`) switch, exactly like the
existing `lynx-bll.cfg` / `lynx-uploader.cfg`:

```
cl65 -t lynx -C lynx-256k.cfg -o game.lnx game.c
```

The cart name, manufacturer, rotation, AUDIN, and EEPROM fields in the LNX header
can still be rewritten per game afterwards with `tools/lnx` (see `doc/lnx.html`).

Program‑size limit is unchanged and worth stating in the docs: the padded cart is
larger, but usable code+data is still bounded by the ~46.5 KB `MAIN` RAM window
(`$BE38 − __STACKSIZE__`). The extra ROM is zero padding, not additional code
space; using it requires the file/directory streaming mechanism, not this cfg.

## 7. Documentation updates (required in the same pass as implementation)

Per `CLAUDE.md`, the code change must land together with the docs. The
customizing guidance from the upstream cc65 "Defining a Custom cc65 Target"
document becomes directly relevant now that users are expected to pick and tweak
these configs, so the transferable parts of it are folded into lynxcc's own docs
rather than linked out to the FPGA tutorial.

1. **`doc/cart.html` §1 "Binary format"** — primary home. Add a subsection,
   e.g. "1.x Generating fixed‑size cart images", that:
   - restates the block‑size → ROM‑size relationship already in the section and
     ties each row to its config file (`lynx-128k/256k/512k.cfg`);
   - explains, at a high level adapted from the customizing document, how a
     lynxcc linker config is structured — the `MEMORY` areas (`start`, `size`,
     `file`, `fill`/`fillval`), how `%O` names the output and how same‑file areas
     concatenate in declaration order, and how `SEGMENTS` map into those areas —
     so a reader can see *why* the fill area produces a padded image;
   - notes the pad byte is `$42` (a free choice — the region is never loaded or
     executed) and that the 64‑byte LNX header sits ahead of the padded blocks
     region;
   - cross‑references `doc/ld65.html` §5.6 (the authoritative `fill` / `fillval`
     reference — do **not** duplicate it) and `doc/lnx.html` (per‑game header
     rewrite; the parallel `lnx bll` sizing).
   - Follow `design/DOC_SVG_STYLE_DESIGN.md` if a layout diagram is added
     (the section already has cart‑layout SVGs to extend).
2. **`doc/lynx.html` / config reference** — wherever `lynx-bll.cfg` and the other
   configs are listed, add the three new configs with a one‑line purpose each.
3. **Header/inc comments** — none of the public `include/*.h` change, but the new
   `.cfg` files carry the standard comment header, and any comment in
   `runtime/lynx/exehdr.s` that describes sizing stays consistent with the
   convention stated here.
4. **`CLAUDE.md` / cross‑refs** — reference this design doc (`design/`
   path) from `doc/cart.html` where appropriate, matching how other pages point
   at their `*_DESIGN.md`.

No functional doc belongs in `MEMORY.md`; only the one‑line index pointer.

## 8. Verification (done)

- Linked trivial and 4 KB‑BSS programs with each config; `%O` sizes are exactly
  131136 / 262208 / 524352 bytes (`64 + block×256`) in both cases — the BSS case
  confirms `MAIN`'s `fill = yes` keeps the pre‑pad length constant.
- The leading payload bytes are byte‑identical to the same program linked with
  `lynx.cfg` (for the 256 KB config, whose block size matches the default), then
  `$00` to the end of the `MAIN` window, then `$42` to the cart size.
- Header block‑size field reads `$0200` / `$0400` / `$0800` for the three configs.
- A padded `lynxdemo` (256 KB) boots on the in‑tree GearLynx harness to a
  pixel‑identical frame — same SHA‑256 as the committed `lynxdemo` golden — so the
  padding does not disturb the loader.
- `cfg/lynx.cfg` and the existing configs are untouched; example goldens are
  unaffected.
