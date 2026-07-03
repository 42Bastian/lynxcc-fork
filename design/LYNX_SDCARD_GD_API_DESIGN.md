<!--
SPDX-License-Identifier: CC-BY-4.0
Lynx Game Development SDK documentation, (c) 2026 the lynxcc authors,
licensed under Creative Commons Attribution 4.0 International.
See doc/licenses.html.
-->

# Design: SD/GD Flash Cart API (`sdcard_gd_*`)

Status: **IMPLEMENTED** (2026-07-03). This document is the source of truth for
the `sdcard_gd_*` subsystem. The first implementation is in **C**
(`libraries/sdcard-gd/lynx-sdcard-gd.c`), a direct port of the reference
`LynxSD.c`; §5 and §8 note the asm-vs-C trade-off left open for a later pass.

Source of truth for adding a runtime API that lets a **lynxcc** program talk to
the microcontroller (MCU) on the RetroHQ Lynx SD / GD flash cartridge — listing
the SD card, reading and writing files, reprogramming the cart's SRAM/flash with
a new ROM image, and powering the card down. The API is named `sdcard_gd_*`
(the RetroHQ "SD/GD" cart is the **g**ame **d**rive variant); the `_gd_` infix
leaves room for a sibling API (e.g. `sdcard_<other>_*`) when a second flash-cart
family is added later.

The protocol and register handshake documented here were reverse-derived from
two existing, working codebases (see §2). Nothing in this document changes the
Lynx memory map, codegen, or any existing library; it only *adds* a new opt-in
subsystem.

## 1. Why add this

**lynxcc** currently has no way for a program to reach the SD card sitting behind
the cartridge MCU. Everything the cart can do — enumerate directories, stream
files, reflash the ROM window, save data — is done through a small byte protocol
over two cartridge I/O registers. That protocol is stable, well understood, and
already shipped in the wild by RetroHQ's own menu loader, so it is a good fit for
a first-class SDK subsystem alongside `cart.html`'s read-only cart access and the
`eeprom_*` save API.

Bringing it in-tree means homebrew authors can write their own loaders, level
streamers, high-score savers, or multi-ROM front-ends without hand-copying the
`LynxSD.c` file that circulates in the community. It also gives us one documented
place for the SD card layout conventions (the `menu/` directory, `.lsd` previews,
`romlist.txt`, block-sizing rules) that are currently only implicit in the menu
source.

## 2. Provenance and scope

The API is derived from two inputs supplied for analysis:

| Input | What it is |
|---|---|
| `menu_1.8_src` | The original RetroHQ **GameMenu** loader — `LynxSD.h` / `LynxSD.c` (the MCU interface) plus `GameMenu.c` (the loader that uses it). Authors: SainT (RetroHQ), GadgetUK. |
| `lynxsd-menu` | Igor Kromin's *Lynx SD Menu Loader 2* rewrite. Same `LynxSD.h` / `LynxSD.c` byte-for-byte, split into `Directory.c` / `Program.c` / `UI.c` / `Preferences.c` / `Main.c`, plus a two-stage boot loader and a documented SD card layout. |

The MCU interface (`LynxSD.c`) is **identical** in both projects, so the wire
protocol below is authoritative. The higher-level behaviour (file formats, block
sizing, SD layout) is drawn from how both loaders *use* that interface.

**In scope:** the 11 MCU commands, the register handshake, the C entry points,
library placement, the new doc page, funcref integration, and the SD card layout
reference.

**Out of scope (call out explicitly so the omissions don't later read as
oversights):**

- The two-stage menu boot loader (`menu.bin` + `menu2stg.bin`) and the first-stage
  `fstg.s` — those are an *application* built on the API, not part of it.
- The homebrew object-loader shim (`gObjectLoader[]`) and BS93 patching. This is
  a loader concern; §7.4 documents it as a recommended helper but it is not a
  core entry point.
- The 512-block / A19-via-aux-pin large-card addressing is specified in the API
  (the `b512BlockCard` argument) but the SDK provides no card-capacity detection;
  the caller must know its card.
- UI, palettes, sprites, joystick — unrelated menu concerns.

## 3. The wire protocol (authoritative)

### 3.1 Registers and handshake

Communication is a byte FIFO exposed through four addresses already known to the
SDK:

| Symbol | Address | Role |
|---|---|---|
| `IODIR`  | `$FD8A` | Mikey I/O direction. Set to `0` (all input) at init. |
| `IODAT`  | `$FD8B` | Mikey I/O data. Bit 4 (mask `$10`, "AUX") is the FIFO ready/busy flag. |
| `CART0`  | `$FCB2` | Cartridge bank 0 data port — **read** side of the FIFO. |
| `CART1`  | `$FCB3` | Cartridge bank 1 data port — **write** side of the FIFO. |

The AUX bit in `IODAT` gates every transfer:

- **Write a byte:** spin while `IODAT & $10` is set (MCU still draining the write
  FIFO), then store the byte to `CART1`.
- **Read a byte:** spin while `IODAT & $10` is clear (no data yet), then load from
  `CART0`.

Multi-byte values are **little-endian** (LSB first): a `u16` is 2 bytes, a `u32`
is 4 bytes, transferred low byte first. Strings are sent NUL-terminated, one byte
at a time including the terminating `\0`.

Initialisation (`sdcard_gd_init`) writes `IODIR = 0` then `CART1 = $AA` — the
magic byte that wakes the MCU comms state machine. No response is read.

### 3.2 Command set

Every command begins by writing a single command byte, then any arguments, then
(for most) reading a status/result. The command byte values are a plain 0-based
enum:

| # | Command byte | Arguments written (LE) | Response read |
|---|---|---|---|
| 0 | `OpenDir`     | NUL-terminated path string | 1 status byte |
| 1 | `ReadDir`     | — | 1 status byte; **if OK**, a 22-byte `SFileInfo` |
| 2 | `OpenFile`    | NUL-terminated path string | 1 status byte |
| 3 | `GetSize`     | — | 4-byte `u32` file size |
| 4 | `Seek`        | 4-byte `u32` absolute offset | 1 status byte |
| 5 | `Read`        | 2-byte `u16` byte count | *count* data bytes, then 1 status byte |
| 6 | `Write`       | 2-byte `u16` byte count, then *count* data bytes | 1 status byte |
| 7 | `Close`       | — | 1 status byte |
| 8 | `ProgramFile` | `u16` start block, `u8` block size, `u16` block count | 1 status byte (blocks until flash done) |
| 9 | `ClearBlocks` | `u16` start block, `u16` block count | 1 status byte (blocks until erase done) |
| 10 | `LowPowerMode` | — | *(none)* |

Status bytes decode to `FRESULT` (§3.3). `GetSize` returns a raw `u32` with no
status byte. `LowPowerMode` powers the SD card down and returns nothing — it is
the last call before launching a freshly programmed ROM.

Notes carried over from the reference implementation:

- `Read` streams the payload *first*, then a trailing status byte — so the caller
  must always drain `nSize` bytes before reading the result.
- `ProgramFile` / `ClearBlocks` block on the MCU side until the flash operation
  completes; the status read is the completion signal. These can take a while for
  large ROMs.
- For a **512-block ("A19 controlled by aux pin") card**, the large-card bit is
  folded into an argument rather than a separate field: `ProgramFile` ORs `$10`
  into the *block-size* byte; `ClearBlocks` ORs `$8000` into the *block-count*
  word. The `sdcard_gd_*` wrappers take a `b512BlockCard` flag and apply this.

### 3.3 Result codes (`FRESULT`)

FatFs-style enum returned as the status byte:

| Value | Name | Meaning |
|---|---|---|
| 0 | `FR_OK` | success |
| 1 | `FR_DISK_ERR` | low-level I/O error |
| 2 | `FR_NOT_READY` | card/MCU not ready |
| 3 | `FR_NO_FILE` | path not found |
| 4 | `FR_NOT_OPENED` | operation needs an open file/dir |
| 5 | `FR_NOT_ENABLED` | subsystem disabled |
| 6 | `FR_NO_FILESYSTEM` | no valid FAT filesystem |

### 3.4 `SFileInfo` and directory-entry attributes

`ReadDir` returns a 22-byte record on success:

```
struct {                 offset  size
    u32  fsize;          //  0     4   file size in bytes
    u16  fdate;          //  4     2   FAT last-modified date
    u16  ftime;          //  6     2   FAT last-modified time
    u8   fattrib;        //  8     1   attribute bits (below)
    char fname[13];      //  9    13   8.3 name, NUL-terminated
}                        // total 22 bytes
```

The layout is packed and byte-exact — the wrapper reads `sizeof(SFileInfo)` bytes
straight into the struct. cc65 does not pad structs, so the record is 22 bytes;
the field order and `fname[13]` length must be preserved. Attribute
bits (`fattrib`):

| Bit | Name | Meaning |
|---|---|---|
| `$01` | `AM_RDO` | read only |
| `$02` | `AM_HID` | hidden |
| `$04` | `AM_SYS` | system |
| `$08` | `AM_VOL` | volume label |
| `$0F` | `AM_LFN` | long-file-name entry (mask) |
| `$10` | `AM_DIR` | directory |
| `$20` | `AM_ARC` | archive |
| `$3F` | `AM_MASK` | mask of defined bits |

`ReadDir` is called repeatedly until it returns non-`FR_OK`, walking the directory
opened by the last `OpenDir`.

## 4. Proposed C API

The 11 MCU commands map to 12 public functions (init has no command byte). All
data-carrying calls are `__fastcall__`, matching the rest of the SDK. Names use
the `sdcard_gd_` prefix chosen for this cart family.

```c
/* sdcard-gd.h — RetroHQ SD/GD flash cart interface */

typedef enum {
    FR_OK = 0, FR_DISK_ERR, FR_NOT_READY, FR_NO_FILE,
    FR_NOT_OPENED, FR_NOT_ENABLED, FR_NO_FILESYSTEM
} FRESULT;

typedef struct {
    unsigned long  fsize;      /* file size            */
    unsigned int   fdate;      /* FAT modified date    */
    unsigned int   ftime;      /* FAT modified time    */
    unsigned char  fattrib;    /* AM_* attribute bits  */
    char           fname[13];  /* 8.3 name, NUL-term   */
} SFileInfo;

void               sdcard_gd_init(void);
void               sdcard_gd_lowpower(void);

FRESULT __fastcall__ sdcard_gd_opendir(const char *pDir);
FRESULT __fastcall__ sdcard_gd_readdir(SFileInfo *pInfo);

FRESULT __fastcall__ sdcard_gd_open(const char *pFile);
FRESULT              sdcard_gd_close(void);
FRESULT __fastcall__ sdcard_gd_seek(unsigned long nSeekPos);
unsigned long        sdcard_gd_size(void);
FRESULT __fastcall__ sdcard_gd_read(void *pBuffer, unsigned int nSize);
FRESULT __fastcall__ sdcard_gd_write(const void *pBuffer, unsigned int nSize);

FRESULT __fastcall__ sdcard_gd_program(unsigned int  nStartBlock,
                                       unsigned char nBlockSize,
                                       unsigned int  nBlockCount,
                                       unsigned char b512BlockCard);
FRESULT __fastcall__ sdcard_gd_clear(unsigned int  nStartBlock,
                                     unsigned int  nBlocks,
                                     unsigned char b512BlockCard);
```

### 4.1 Name mapping from the reference API

| Reference (`LynxSD_*`) | lynxcc (`sdcard_gd_*`) |
|---|---|
| `LynxSD_Init`               | `sdcard_gd_init` |
| `LynxSD_LowPowerMode`       | `sdcard_gd_lowpower` |
| `LynxSD_OpenDir`            | `sdcard_gd_opendir` |
| `LynxSD_ReadDir`            | `sdcard_gd_readdir` |
| `LynxSD_OpenFile`           | `sdcard_gd_open` |
| `LynxSD_CloseFile`          | `sdcard_gd_close` |
| `LynxSD_SeekFile`           | `sdcard_gd_seek` |
| `LynxSD_GetFileSize`        | `sdcard_gd_size` |
| `LynxSD_ReadFile`           | `sdcard_gd_read` |
| `LynxSD_WriteFile`          | `sdcard_gd_write` |
| `LynxSD_ProgramROMFromFile` | `sdcard_gd_program` |
| `LynxSD_ClearROMBlocks`     | `sdcard_gd_clear` |

### 4.2 Backward-compatibility aliases

Following the precedent set by `LYNX_GFX_RENAME_DESIGN.md` and
`LYNX_EEPROM_RENAME_DESIGN.md`, the header should provide `#define` aliases from
the original `LynxSD_*` names to the new ones, guarded by a
`LYNX_NO_SDCARD_GD_COMPAT` macro so a project can compile them out. This lets the
large body of existing community code that calls `LynxSD_OpenFile` etc. build
unchanged, while new code uses the SDK-native names. The `SFileInfo`, `FRESULT`
and `AM_*` names are already generic enough to keep as-is.

Function-like aliases must be used where the original is function-like (the
gotcha recorded during the gfx rename): e.g. `#define LynxSD_open sdcard_gd_open`.

## 5. Library placement

The MCU driver is a small, self-contained, opt-in subsystem — the same shape as
the audio and compress libraries. It should **not** go in the always-linked
`lynx.lib` core, because most ROMs never touch the SD card and should not pay for
the code.

Proposed layout, following `LYNX_SDK_LAYOUT_DESIGN.md` §6:

```
libraries/sdcard-gd/lynx-sdcard-gd.s ; the driver (asm port of LynxSD.c)
include/lynx/sdcard-gd.h             ; public header (§4)
asminc/lynx/sdcard-gd.inc            ; register + command-byte equates for asm callers
```

Build wiring (`libraries.mk`): add a `SDCARD_GD_DIRS = libraries/sdcard-gd` group
that archives into a new `lib/lynx-sdcard-gd.lib`, and register it in the SDK
manifest (`lib/lynx-sdklibs.list`) so `cl65` auto-links it on demand exactly like
`lynx-audio.lib` (per `LYNX_CL65_AUTOLIBS_DESIGN.md`). Everything here is specific
to the RetroHQ SD/GD cart — the driver, header, asminc, and archive are all
`sdcard-gd`-named. A future cart family gets its *own* library
(`lib/lynx-sdcard-<other>.lib`, `include/lynx/sdcard-<other>.h`); the shared
`sdcard_` root lives only in the function-name prefix, not in a shared archive.

Implementation note: the shipped driver is **C**
(`libraries/sdcard-gd/lynx-sdcard-gd.c`), a direct port of the reference
`LynxSD.c` with the entry points renamed to `sdcard_gd_*`. Every other low-level
driver in the tree (`lynx-snd.s`, `eeprom*.s`, joystick, serial) is hand-written
6502 asm for size and to control the tight FIFO spin-loops; porting this one to
asm is left as a future size optimisation (§8), with the C source as the
behavioural reference. The exported link symbols are the `sdcard_gd_*` names
either way.

## 6. SD card layout reference

This is the on-card structure the API operates against, and the second half of
the new doc page. It is drawn from *Lynx SD Menu Loader 2*'s documented layout
and both loaders' file handling.

### 6.1 Paths and case

Paths passed to `sdcard_gd_opendir` / `sdcard_gd_open` are DOS-style with `/`
separators, relative to the card root (e.g. `"menu/prefs"`,
`"_preview/alien.lsd"`). Directory listings return **8.3 upper-case** short names
in `fname`; long names are not returned by `readdir` (see `romlist.txt`, §6.4).
File extensions are compared case-insensitively by the loaders but returned
upper-case.

### 6.2 Recognised ROM file types

The menu treats a directory entry as a launchable ROM when its extension is one
of:

| Ext | Format | Sizing |
|---|---|---|
| `.LNX` | Headered Lynx ROM (64-byte "LYNX" header) | block size read from header byte 5 |
| `.LYX` | Raw/headerless ROM image | block size inferred from file size |
| `.O`   | Homebrew BS93 object file | fixed 8 (512-byte) blocks + object loader |
| `.COM` | Same header as `.O` homebrew | same as `.O` |

Directory entries that are directories are also shown (for navigation), except
the reserved `MENU` and `_PREVIEW` folders. Entries with `AM_SYS` or `AM_HID` set
are skipped.

### 6.3 The `menu/` directory (loader state)

The menu keeps its own state as **files it reads and writes through this very
API** — a good worked example of `sdcard_gd_read` / `sdcard_gd_write`:

| Path | Purpose |
|---|---|
| `menu/prefs`        | Preferences blob (one byte per option), read at boot, written on change. |
| `menu/lastrom`      | 256-byte buffer holding the path of the last launched ROM (auto-launch / "load last"). |
| `menu/homebrew`     | Scratch file the loader overwrites with the patched BS93 object loader before programming `.O`/`.COM` ROMs. |
| `menu/default.pal`  | 32-byte BGR palette for the UI theme; absent ⇒ built-in colours. |
| `menu/*.pal`        | Extra sample palettes (`alien`, `fluoro`, `grey`, `inverse`); copy over `default.pal` to switch. |

### 6.4 Long names — `romlist.txt`

An optional `romlist.txt` in any directory maps 8.3 names to display names:

```
[gateszen.lnx]Gates of Zendocon
[alien.lnx]Alien vs Predator
[AUSTRA~1.O]Australia Day Mini-Demo
```

Short (8.3) name in brackets, long name (≤45 chars) after. Only overrides files
that actually exist; the file is parsed by streaming it a byte at a time with
`sdcard_gd_read`. This is a loader convention, documented for completeness — the
API itself has no notion of long names.

### 6.5 Game previews — `.lsd` files

A preview is a full-screen image plus palette the menu shows on demand:

- **Location:** two schemes. New: an `.LSD` file *beside* the ROM, same base name
  (`alien.lnx` → `alien.LSD`). Legacy: a `_PREVIEW/` directory at the card root
  holding `<base>.lsd` for each ROM (selectable via a preference).
- **Format:** `8365` bytes of Lynx sprite image data followed by a `32`-byte
  palette. The loader opens the file, `sdcard_gd_read`s 8365 bytes into the image
  buffer, then 32 bytes into the palette, then closes.

### 6.6 ROM programming — block sizing rules

`sdcard_gd_program` writes *block count* blocks of *block size* (in 256-byte
units) starting at *start block*, sourced from the current file position of the
open file. The loaders compute the arguments as follows:

**`.LNX` (headered):** read the 64-byte header. Byte 5 is the block-size code
(must be 1, 2, 4, or 8 = 256/512/1024/2048 bytes). File size minus the 64-byte
header, divided by block-size-bytes (rounded up), gives the block count. Seek past
the 64-byte header, then program from block 0.

```
nBlockSizeBytes = nBlockSize << 8;
nBlockCount     = (fileSize - 64 + nBlockSizeBytes - 1) / nBlockSizeBytes;
sdcard_gd_seek(64);
sdcard_gd_program(0, nBlockSize, nBlockCount, 0);
```

Header byte 60 bit 6 (`$40`) is the "use SD-cart EEPROM as a file" flag: when set,
the loader must **not** call `sdcard_gd_lowpower` before launch (the game keeps
talking to the MCU for saves).

**`.LYX` (raw):** no header; pick block size from total file size —
64K→1, 128K→2, 256K→4, 512K→8, else default 4 — then program from block 0 over
the whole file.

**`.O` / `.COM` (homebrew BS93):** a two-step flash. First program a patched
object-loader stub (`menu/homebrew`, one 2048-byte block at block 0), then program
the object body (from file offset 10, in 2048-byte blocks) at block 1. Header
bytes 2–5 supply load address and size, patched into the loader stub. This is
loader-specific and belongs in an example, not the core API (§2 out-of-scope).

### 6.7 Erasing — `sdcard_gd_clear`

`sdcard_gd_clear(startBlock, nBlocks, b512BlockCard)` erases a block range without
a source file. Used to blank cart SRAM/flash before or instead of programming.

## 7. Documentation deliverables

Per `CLAUDE.md`, the implementation pass must land code and docs together. This
design prescribes the **full** doc scope:

### 7.1 New page — `doc/sdcard-gd.html`

A new top-level doc page titled "SD / GD flash cart", modelled on `cart.html`.
Sections:

1. Overview — what the cart MCU is, opt-in library, `#include <lynx/sdcard-gd.h>`.
2. Register handshake and wire protocol (§3 here), with an inline SVG of the
   FIFO/AUX-bit handshake following `design/DOC_SVG_STYLE_DESIGN.md` (viewBox 720,
   theme CSS vars, `<figure>`/`<figcaption>`).
3. The C API, grouped: init/power, directory, file I/O, ROM programming.
4. Result codes and `SFileInfo`.
5. SD card layout (§6 here) — `menu/` files, `romlist.txt`, `.lsd` previews,
   recognised ROM types, block-sizing table.
6. A worked example: open a directory, list ROMs, program and launch one.
7. Compatibility note on the `LynxSD_*` aliases.

Wire the page into the nav bar / index card grid on **all** doc pages (the same
sweep the `cart.html`, `sound.html`, and `lnx.html` additions did).

### 7.2 `doc/funcref.html`

Add a new "Functions by header file" subsection for `lynx/sdcard-gd.h` (a new
§2.x, inserted in the existing case-insensitive header ordering — `sdcard-gd`
sorts before `serial`, so between `peekpoke.h` and `lynx/serial.h`), listing the
12 functions. Add the 12 alphabetical entries to
§3 (`sdcard_gd_clear`, `..._close`, `..._init`, `..._lowpower`, `..._open`,
`..._opendir`, `..._program`, `..._read`, `..._readdir`, `..._seek`, `..._size`,
`..._write`), and renumber the 3.N headers by position, per the funcref-sorting
convention. Update the sidebar index anchors to match.

### 7.3 Header / asminc doc comments

`include/lynx/sdcard-gd.h` carries doc comments on every function (parameters, the
FIFO blocking behaviour of `program`/`clear`, the little-endian/NUL-terminated
argument encoding). `asminc/lynx/sdcard-gd.inc` documents the register and
command-byte equates.

### 7.4 Example program

Add `examples/` demo (subsystem dir, e.g. `examples/cart/` or a new
`examples/sdcard-gd/`) that opens the card root, lists entries with `readdir`, and
either dumps a file or (guarded) programs and launches a ROM — so the API has a
compiled, GearLynx-verifiable user. The homebrew object-loader shim (§6.6) lives
here if implemented, not in the library.

### 7.5 Design-doc references

On implementation, flip this doc's status to **IMPLEMENTED**, and reference it
from `LYNX_SDK_LAYOUT_DESIGN.md` (new library group) and the new
`doc/sdcard-gd.html` footer, using the `design/` path per `CLAUDE.md`.

## 8. Open questions for the implementation pass

1. **Asm vs C driver.** *Resolved for now:* shipped as C
   (`libraries/sdcard-gd/lynx-sdcard-gd.c`) — the proven reference, byte-for-byte
   in behaviour with `LynxSD.c`. Porting the FIFO spin loops to asm to shrink the
   archive remains a future optimisation.
2. **Card-capacity detection.** The API exposes `b512BlockCard` but the SDK
   offers no auto-detect. Leave to the caller, or add a probe helper later?
3. **`menu/homebrew` shim ownership.** Confirmed out of the core library (§2);
   decide whether the example ships the `gObjectLoader[]` bytes or regenerates
   them from an SDK BS93 stub (cf. the `blloader_gen.c` approach in
   `LYNX_LNX_BLL_ROM_DESIGN.md`).
4. **Naming lock-in.** `sdcard_gd_*` assumes future siblings share the `sdcard_`
   root with a family infix. Confirm before other cart families arrive.
