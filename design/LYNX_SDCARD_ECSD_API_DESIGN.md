<!--
SPDX-License-Identifier: CC-BY-4.0
Lynx Game Development SDK documentation, (c) 2026 the lynxcc authors,
licensed under Creative Commons Attribution 4.0 International.
See doc/licenses.html.
-->

# Design: ElCheapoSD cartridge API (`sdcard_ecsd_*`)

Status: **DESIGN ONLY** (2026-07-03) — approach locked. This document is the source
of truth for a new **lynxcc** subsystem, `sdcard_ecsd_*`, that lets a running Lynx
program drive BennVenn's **ElCheapoSD** flash cartridge through its MCU: enumerate the
SD card's directories, change directory, launch a ROM, boot the last-played ROM, and
read/write the small persistent configuration store. It is enough to **reproduce the
whole ROM menu** on top of the SDK, and it sits beside `sdcard_gd_*` for the RetroHQ
SD/GD cart.

Locked decisions (from review):

1. **Ship a real, fully-documented library.**
2. **The firmware is fixed** — no new MCU commands; only what ships today.
3. **Expose the menu's own building blocks** (§4) under new names, so the menu can be
   rebuilt with this API: directory listing, open/descend, back, boot-last, can-save,
   reset.
4. **Config store = the MCU `CONFIG` byte area** (§5), byte accessors only, named
   `sdcard_ecsd_config_*`, with the menu's own offsets marked out of bounds.

Everything is reverse-derived from the shipping ElCheapoSD ROM menu source
(`ecsd.c`/`ecsd.h`, `ecsdcom.s`, `Main.c`).

## 1. Scope

The API has three groups plus presence detection:

- **Presence** — `sdcard_ecsd_available()` (§3.3).
- **Browse & launch** (§4) — the menu's engine: list a directory, descend into one,
  go back up, launch a selected ROM, boot the last ROM, reset. These are thin renames
  of the shipping `ecsd*` functions.
- **Config store** (§5) — `sdcard_ecsd_config_*`, the persistent MCU byte area.

**Out of scope / impossible on fixed firmware (§6):** reading or writing an arbitrary
*file* on the SD card. A running game can *enumerate* directories and *launch* ROMs,
but cannot pull a data file's bytes or write one.

**Separate save path, not part of this API:** a game persists its own save by writing
the on-board physical 93C46 through the standard `eeprom_*` API. Independently, the MCU
backs that chip up to / restores it from a `SAVES` directory on the card when the active
ROM changes (per-game EEPROM persistence, automatic). `sdcard_ecsd_can_save()` (§4)
merely reports whether that `SAVES` directory exists; it does not itself save anything.

## 2. The transport (authoritative)

### 2.1 Command channel — the MCU's emulated EEPROM

The Lynx reaches the MCU through a **second, MCU-emulated 93C46-style EEPROM** on
cartridge chip-select **A8** (sharing the standard CLK/DI-DO lines with the board's
real EEPROM). Its "registers" are a command mailbox, not memory. `ecsdcom.s` provides
the `_BV` ("bus variant") primitives:

```
unsigned  lynx_eeread_BV (unsigned char cell);
void      lynx_eewrite_BV(unsigned int addr, unsigned int val);
```

Registers **0x00–0x3E** carry an ASCII keyword frame; register **0x3F** is the
doorbell/ack. `SendCommand()` writes the frame, writes `0x55` to 0x3F, polls 0x3F
until it reads `0xAA55`, then reads the 128-byte response (high byte of each word
first). An unknown keyword never acks (bounded-timeout) — the basis of presence
detection (§3.3). Shipping keywords this API wraps: `VERSION`, `DIR…`, `OPEN…`,
`BACK`, `LAST`, `SAVES?`, `CONFIGR`, `CONFIGW`.

### 2.2 Bulk channel — the ROM streaming window (MCU → Lynx)

The Lynx cannot write cart ROM, so bulk data *from* the MCU (the directory listing)
comes back a second way: the MCU writes it into cartridge ROM space, and the Lynx
reads it through the cc65 cart file descriptor **fd 1**:

```c
lseek(1, 0, SEEK_SET);
read (1, dst, len);
```

This is exactly how the menu pulls a directory listing after a `DIR` command. It is a
**read-only** path (the Lynx never writes ROM) — which is why file *writing* is
impossible (§6) and why the listing is the only bulk transfer the API exposes.

## 3. Presence and cart state

### 3.1 Directory model

The MCU holds a **current directory** on the SD card. `DIR` lists it (filtered to Lynx
ROM types + subfolders); `OPEN` on a *folder* entry descends into it; `BACK` goes up
one level. The API mirrors this: there is one implicit "current directory" owned by the
MCU, and the browse calls (§4) move it.

### 3.2 Big-endian byte order

Multi-byte integers streamed by the MCU (entry size, cluster location) are
**most-significant byte first**, matching the raw entry layout in §4.1.

### 3.3 `sdcard_ecsd_available()`

Sends the shipping `VERSION` command; a real ElCheapoSD replies `"FW V0.0x"` with the
`0xAA55` ack, anything else bounded-times-out and returns 0. Cache the result — the
probe is a full mailbox round-trip.

```c
signed char sdcard_ecsd_available(void);   /* 1 = ElCheapoSD present, else 0 */
```

## 4. Browse & launch API (menu engine)

Header `include/lynx/sdcard-ecsd.h`. Each call is a rename of a shipping `ecsd*`
function; behaviour is preserved exactly.

### 4.1 Directory entry format

The MCU streams fixed **64-byte** entries into the ROM window (§2.2):

```c
#define SDCARD_ECSD_FILE         '1'   /* entry.type: a ROM file  */
#define SDCARD_ECSD_DIR          '2'   /* entry.type: a directory */
#define SDCARD_ECSD_MAX_ENTRIES  256   /* firmware listing cap    */

typedef struct {                 /* raw 64-byte on-wire layout          */
    char           lfn[47];      /*  0..46  long file name, NUL-padded  */
    char           sfn[8];       /* 47..54  8-char short (DOS) name      */
    unsigned char  type;         /* 55      SDCARD_ECSD_FILE / _DIR      */
    unsigned long  size;         /* 56..59  file size (big-endian)       */
    unsigned long  location;     /* 60..63  cluster location (big-endian)*/
} sdcard_ecsd_entry;
```

`location` is the opaque locator passed back to `sdcard_ecsd_open()`.

### 4.2 Calls

```c
/* ecsdGetRomList -> list the current directory (ROMs + folders).
   The MCU streams entries into the ROM window; this copies up to `max` of them
   into `buf` and returns the total entry count via `*count`.
   Returns 0 on success, 1 if the directory holds more than SDCARD_ECSD_MAX_ENTRIES
   (the listing is truncated to the cap). */
signed char sdcard_ecsd_dir_list(sdcard_ecsd_entry *buf, unsigned int max,
                                  unsigned int *count);

/* ecsdOpenROM -> select the entry at cluster `location`. The MCU acts on the entry
   TYPE, so the same call does one of two things:
     - directory: the MCU changes into it; call sdcard_ecsd_dir_list() again to read
       the new contents.
     - ROM file:  the MCU maps it for booting; call sdcard_ecsd_reset() to launch it.
   The caller passes `location` from the selected entry either way. */
void        sdcard_ecsd_open(unsigned int location);

/* ecsdBackDirectory -> change up one directory level. Follow with dir_list(). */
void        sdcard_ecsd_back(void);

/* ecsdBootLastRom -> boot the most-recently-played ROM (issues LAST, then resets).
   Does not return. */
void        sdcard_ecsd_boot_last(void);

/* ecsdCanSave -> 1 if the card has a SAVES directory (so the MCU can back up/restore
   the physical EEPROM across ROM changes), else 0. Reports state only. */
signed char sdcard_ecsd_can_save(void);

/* ecsdReset -> reset the Lynx via the reset vector, launching the ROM the MCU has
   mapped (after sdcard_ecsd_open() on a ROM file). Does not return. */
void        sdcard_ecsd_reset(void);
```

### 4.3 Behaviour notes

- **`open()` is dual-purpose** — descend vs launch is decided by the MCU from the entry
  type, not by the caller. A menu inspects `entry.type` only to know whether to
  re-`dir_list()` (folder) or `reset()` (ROM); it passes `entry.location` unchanged in
  both cases.
- **Launching is two steps** — `open()` maps the ROM, `reset()` starts it. `open()`
  alone does not transfer control; `reset()` does and never returns.
- **`boot_last()` and `reset()` do not return** — they hand the machine to another ROM.
- **`dir_list()` reuses the ROM window (§2.2)** — each call clobbers whatever was there;
  copy out what you need before the next call. The `max`/`count` split lets a caller
  size its own entry buffer while still learning the true total (and the truncation
  flag when a directory exceeds the cap).

## 5. Config store — `sdcard_ecsd_config_*`

`CONFIGR`/`CONFIGW` read/write **one byte** of a persistent MCU store at a 16-bit
address carried as nibble bytes; in the menu `addr = 256 + offset`. It is
byte-granular (one mailbox round-trip per byte), persistent across power-off, and a
**single cart-global store** shared by every title and the menu.

### 5.1 Reserved offsets — out of bounds

The shipping menu keeps its 8 settings (`SETTING_COUNT == 8`) at offsets **0–7**. These
belong to the menu and the API refuses them:

| Offset | Menu setting (`Main.c`)        |
|:------:|--------------------------------|
| 0      | sort folders to top            |
| 1      | sort length (approximate sort) |
| 2      | theme number                   |
| 3      | menu sounds                    |
| 4      | show DOS file names            |
| 5      | hide ROM extensions            |
| 6      | scroll / input-repeat delay    |
| 7      | hide system folders            |

The command also rejects `offset > 256`, so the **game-usable window is offsets 8–256**
(249 bytes).

### 5.2 Calls

```c
#define SDCARD_ECSD_CONFIG_FIRST   8      /* first game-usable offset (0..7 = menu; */
                                          /*  0 in a SDCARD_ECSD_MENU_BUILD, see §5.4)*/
#define SDCARD_ECSD_CONFIG_LAST    256    /* last usable offset (firmware ceiling)  */
#define SDCARD_ECSD_CONFIG_BYTES   249    /* LAST - FIRST + 1 (game build)          */

/* Read/write one persistent byte. `offset` matches the menu's config offset
   (MCU address = 256 + offset).
   Returns:  1 success,  0 offset out of bounds (0..7 or > 256),  -1 transport error. */
signed char sdcard_ecsd_config_read (unsigned int offset, unsigned char *out);
signed char sdcard_ecsd_config_write(unsigned int offset, unsigned char val);
```

### 5.3 Notes

- **Reserved 0–7 are hard-rejected**, protecting the menu's settings — unless the caller
  *is* a menu reimplementation, which legitimately owns them; see §5.4.
- **Shared, not per-game** — two titles see the same bytes. A game that must trust its
  data should reserve a couple of bytes for a magic value + checksum and treat a
  mismatch as "no data"; the firmware offers no per-game partition.
- **Slow, tiny** — 249 bytes, one round-trip per byte; write only at explicit save
  points.

### 5.4 Menu reimplementation and offsets 0–7

A program rebuilding the menu needs to read/write offsets 0–7 (the settings table),
which the byte API rejects by default to protect ordinary games. This is resolved at
**compile time**, not with a runtime call: a translation unit that defines
`SDCARD_ECSD_MENU_BUILD` before including the header lowers the reserved floor to 0, so a
menu/loader build can address the whole store while an ordinary game build cannot.

```c
/* include/lynx/sdcard-ecsd.h */
#ifdef SDCARD_ECSD_MENU_BUILD
#  define SDCARD_ECSD_CONFIG_FIRST  0     /* menu owns the settings table (0..7) */
#else
#  define SDCARD_ECSD_CONFIG_FIRST  8     /* games start above the menu's bytes  */
#endif
```

`config_read`/`config_write` bound-check against `SDCARD_ECSD_CONFIG_FIRST`, so the same
two functions serve both builds with no separate menu-only entry points and no way for a
plain game to reach offsets 0–7 by accident. The upper bound
(`SDCARD_ECSD_CONFIG_LAST`) is unchanged. A compile-time switch is preferred over a
runtime "allow menu" call because the distinction is a property of *what the program is*
(a loader vs a game), fixed for the whole binary, and best caught at build time.

## 6. What is *not* possible (fixed firmware)

- **No file read** — only *directory listings* stream into the ROM window, never file
  contents.
- **No file write** — the Lynx cannot write cart ROM and there is no shipping
  write-file command; only the `CONFIG` area is writable.
- **No large storage** — the config window is 249 bytes; the SD card is otherwise
  read-only-enumerable.
- **No per-game isolation in the config store** — one shared area (§5.3). (Per-game
  *EEPROM* saves do exist separately, via the physical 93C46 + the MCU's automatic
  `SAVES` backup; see §1.)

Any of these needs a firmware change (a conversation with the cart author), not a
lynxcc API change; update this doc to point there if that path is taken.

## 7. Library placement, docs, and examples

- `libraries/sdcard-ecsd/` — `lynx-sdcard-ecsd.c` (browse/launch + config wrappers) plus
  the MCU mailbox and `sdcard_ecsd_reset` asm. The `_BV` primitives, `SendCommand`/
  `GetResponse` framing, and the reset routine move out of the menu's private copy into
  this library. Wire it into `libraries.mk` and the SDK lib manifest as
  `sdcard-ecsd.lib`, mirroring `sdcard-gd`.
- Per the repo rule that docs track code in the same pass:
  - `include/lynx/sdcard-ecsd.h` doc comments; `asminc/lynx/` comments for the exported
    asm (`sdcard_ecsd_reset`, the `_BV` mailbox).
  - `doc/sdcard-ecsd.html`, carded on `index.html`, following
    `design/DOC_STRUCTURE_DESIGN.md` and `design/DOC_SVG_STYLE_DESIGN.md`. Sections: the
    two channels (§2) with a command-mailbox SVG; the browse/launch model with an
    open=descend-or-launch flow diagram; the entry format; the config store with the
    reserved-offset table; the "not supported: files" box.
  - `doc/funcref.html` — `sdcard_ecsd_*` and `sdcard_ecsd_config_*` sections, re-sorted
    per the funcref convention.
  - **Example = a minimal menu** under `examples/` (its own `examples/loader/` group is
    natural): `available()` → `dir_list()` → cursor over entries → `open()` +
    (`dir_list()` | `reset()`), `back()`, `boot_last()`, and a settings screen using the
    config store. This doubles as the proof that the API reproduces the menu.
  - Cross-reference `LYNX_SDCARD_GD_API_DESIGN.md` (sibling cart, real file I/O).

## 8. Residual open items (confirm with the cart author)

1. **`dir_list()` filter and counts.** The menu filters to `LNX/LYX/O` and parses a
   `"FILES:nnnn FOLDERS:nnnn"` response. Decide whether the API exposes the file/folder
   split (two counts) or just the total, and whether the type filter is fixed or a
   parameter.
2. **True config size.** The menu caps `offset` at 256; confirm the real ceiling and
   whether `CONFIG` is MCU-flash or SD-file backed; raise `SDCARD_ECSD_CONFIG_LAST` if
   larger.
3. **Reserved region.** Confirm the menu owns exactly offsets 0–7 and addresses < 256
   are firmware-private, so the bounds check and the §5.4 `SDCARD_ECSD_MENU_BUILD`
   switch are correct.
4. **Not-yet-wrapped commands.** `INSPECT` and `SCREENSHOT` exist in the firmware but are
   not needed to reproduce the core menu; leave them out or add later.
5. **Shared MCU framing.** Whether `sdcard_gd_*` and `sdcard_ecsd_*` share one
   MCU-mailbox helper or each keep their own.
