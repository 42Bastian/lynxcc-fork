<!--
SPDX-License-Identifier: CC-BY-4.0
Lynx Game Development SDK documentation, (c) 2026 the lynxcc authors,
licensed under Creative Commons Attribution 4.0 International.
See doc/licenses.html.
-->

# Design: Multicart menu loader

Status: **IMPLEMENTED** (2026-07-11). This document is the source of truth for
the multicart mechanism: the cart layout, the relocatable runtime loader, the
`multicart_run()` hand-off, and the `lnx multicart` + `lynxdir` build flow.

Companion to `LYNX_LNX_BLL_ROM_DESIGN.md` (the single-program `lnx bll` cart
wrapper and its generated BLL loader) and `LYNX_STARTUP_RECLAIM_DESIGN.md` (the
ONCE one-shot reclaim that every bundled program's `crt0` performs). The public
face is `include/lynx/multicart.h` and `doc/multicart.html`.

## 1. What a multicart is

A **multicart** is a single bootable `.lnx` cartridge image that bundles a
**menu** program together with several independent **game ROMs**. At power-on the
cart's mini-loader runs the menu; when the player picks a game, the menu calls
`multicart_run(n)`, which pulls game *n* off the cartridge — over the top of the
menu — and runs it. Launching a game overwrites the menu, so there is no return
path without a reboot: **one game per power-on**, exactly as the LynxJam 2024
multicart worked on real hardware.

The menu and every game are ordinary Lynx programs, each compiled and linked as a
**BLL object** (`cl65 -C lynx-bll.cfg`). The menu knows nothing about the games
beyond their slot number. `lnx multicart` writes a `lynxdir` `.mak` describing the
layout, and `lynxdir` assembles the final cart image.

```
cl65 -C lynx-bll.cfg   →  menu.o, game1.o, game2.o, game3.o
lnx multicart          →  multicart.mak
lynxdir multicart.mak  →  multicart.lnx  (+ multicart.lyx)
```

## 2. Cart layout

`lnx multicart` emits a `lynxdir` layout with three fixed regions in block 0:

- `#NEWMINI_FB68` — the NewMini mini-loader at `$FB68`. At power-on it loads and
  runs the **first** (`#EPYX`) directory entry: the menu.
- `#EPYX menu.o` — the menu, EPYX-encoded, loaded and executed by the mini-loader.
- `#DIROFFSET 896` then `#BLL` game list — the **game directory** at byte offset
  `$0380` (896) within block 0, followed by the BLL-encoded game images. Each
  directory entry is 8 bytes in Bastian Schick's `new_bll` `NEWHEAD` format; the
  order of the `#BLL` files sets the game numbers `multicart_run()` takes (0-based).

Block size is 2048 bytes, which reaches the whole 512 KiB a Lynx cart addresses
without bank switching — the multicart target, since bundling several games needs
the space. `#DIRSTART 203` places the directory-bearing region after the loader
scratch.

```
block 0
+--------------------------------------------------------------+
| NewMini loader ($FB68) | EPYX menu image | ...               |
|                          game directory @ $0380 (8B entries) |
|                          BLL game 0 | BLL game 1 | BLL game 2 |
+--------------------------------------------------------------+
```

### 2.1 `new_bll` directory entry (8 bytes)

| Offset | Field         | Meaning                                        |
|:------:|:--------------|:-----------------------------------------------|
|   0    | `StartBlock`  | first cart block of the file                   |
|  1..2  | `BlockOffset` | byte offset within that block (little-endian)  |
|   3    | `ExecFlag`    | new_bll exec flag (unused by our loader)       |
|  4..5  | `DestAddr`    | RAM load address (little-endian); 0 = append   |
|  6..7  | `FileLen`     | image length in bytes (little-endian)          |

A `DestAddr` of 0 means "continue after the previous file" (the loader keeps a
running destination pointer); every bundled program is a full BLL image with a
real `DestAddr` of `$0400` (`__MAIN_START__`).

## 3. The relocatable runtime loader

`runtime/lynx/multicartldr.s` is a small, self-contained loader — a trimmed
reimplementation of `new_bll`'s `includes/file.inc` file loader. The power-on
bootloader (`runtime/lynx/bootldr.s`) only ever loads directory entry 0; this
loader instead takes a **file number** in `A` and indexes the game directory at
`MULTICART_DIROFFSET`.

It is **not** linked into a program. It is assembled and linked on its own with
`cfg/lynx-multicartldr.cfg`, which locates `CODE` at `$0040` and emits a
headerless raw image. `tools/lnx/gen-multicartldr.sh` (top-level
`make multicart-loader-gen`) captures those bytes as the committed generated blob
`libraries/core/multicartldr_gen.s` — the `_multicart_loader` array plus a
`multicart_loader_size` symbol. Regenerate that blob after any change to the
loader source.

Because the captured bytes carry the absolute addresses the linker resolved at
`$0040`, the blob is **position-dependent**: it only runs correctly at `$0040`.
It uses zero page `$00..$0D` as scratch — free, because the menu is being torn
down. The loader reads bank 0 through `RCART0`, strobing the cart block-address
shift register (`SYSCTL1` / `IODAT`) to select each block. It bakes in the
resting `IODAT = $1B` value every lynxcc program runs with, rather than trusting
an unknown menu zero-page state at hand-off.

## 4. The `multicart_run()` hand-off

`multicart_run(romNum)` (`libraries/core/multicart.s`,
`include/lynx/multicart.h`) performs the hand-off from a running menu to a game:

1. Stash `romNum`, `sei` (the loader runs with interrupts masked).
2. Clear the timer-interrupt enables on TIM0 (HBL) and TIM2 (VBL) so a pending
   frame IRQ cannot run menu code we are about to overwrite. The game's `crt0`
   re-initialises the timers from scratch.
3. Blank the palette for a clean hand-off (no garbage flash while the game loads).
4. Copy the `_multicart_loader` blob down to `$0040`.
5. `jmp $0040` with `A = romNum`.

The loader then opens the directory entry, reads the game image over the top of
the menu at its `DestAddr` (`$0400`), and jumps to it. The game boots through its
own `crt0` — including the ONCE relocation and `zerobss` — exactly as a
standalone BLL program would. `multicart_run()` never returns.

### 4.1 Copying a loader larger than a page-half

The blob is copied to `$0040` with an **ascending** count-up loop:

```
    ldy #0
@cpy:
    lda _multicart_loader,y
    sta $0040,y
    iny
    cpy #<multicart_loader_size
    bne @cpy
```

This is deliberate and load-bearing. The loader blob is ~187 bytes — larger than
128. A descending `dey` / `bpl` loop (the usual small-copy idiom) treats any
starting index ≥ `$80` as already negative, so `bpl` fails on the very first
iteration and copies **nothing**; `jmp $0040` then runs stale menu bytes and the
console hangs executing garbage in low memory. The ascending `iny` / `cpy` /
`bne` form copies the full blob for any size up to 256 bytes; an
`.assert multicart_loader_size <= $100` guards the one-page bound.

> **Historical note.** This copy loop was the original multicart crash. Earlier
> the failure was mis-attributed to the ONCE startup-reclaim optimisation
> overwriting the loaded game. It is not: the game never reached RAM at all,
> because the loader itself was never copied. A bundled game's own `crt0` ONCE
> relocation lands entirely within its BSS region (`__ONCE_SIZE__ < $100`, packed
> at `__BSS_RUN__` and moved up to `__ONCE_RUN__`) and does not touch neighbours.
> No dead-space padding between games is required.

## 5. Files

| File                                       | Role                                             |
|:-------------------------------------------|:-------------------------------------------------|
| `runtime/lynx/multicartldr.s`              | loader SOURCE (linked alone at `$0040`)          |
| `cfg/lynx-multicartldr.cfg`                | link config that locates the loader at `$0040`   |
| `tools/lnx/gen-multicartldr.sh`            | regenerates the committed blob                   |
| `libraries/core/multicartldr_gen.s`        | GENERATED blob (`_multicart_loader`)             |
| `libraries/core/multicart.s`               | `multicart_run()` — the hand-off                 |
| `include/lynx/multicart.h`                 | public API + layout constants                    |
| `tools/lnx/multicart.c` / `.h`             | `lnx multicart` — writes the `lynxdir` `.mak`    |
| `cfg/lynx-multicart.cfg`                   | (reserved) multicart-specific link config        |
| `examples/multicart/`                      | menu + three demo games + Makefile               |
| `doc/multicart.html`                       | Reference page                                    |

## 6. Verification

Built from source in the sandbox and driven under headless GearLynx (1.2.18):
the menu renders, and selecting GAME 1 / 2 / 3 loads and runs each bundled game
(the `1`/`2`/`3` screens) with the CPU executing in the game's `$0400` region.
With the descending copy loop the CPU instead froze at `$0047..$0059` executing
garbage in the never-populated loader region — the reproduction that pinned the
root cause.
