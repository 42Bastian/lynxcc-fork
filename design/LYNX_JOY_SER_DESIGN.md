<!--
SPDX-License-Identifier: CC-BY-4.0
Lynx Game Development SDK documentation, (c) 2026 the lynxcc authors,
licensed under Creative Commons Attribution 4.0 International.
See doc/licenses.html.
-->

# Design: Static joy and ser Libraries (De-driverization, Final Round)

Companion to `LYNX_GFX_DESIGN.md`, which established the pattern (its §9 names this
work). Same premise: this is a Lynx-only tree, each driver type has exactly one driver
that is always present, so the loadable-driver machinery is pure overhead. Same rules:
direct-call static modules under `libraries/core/`, clean API break, no shims, compile
errors (not silent behavior change) as the failure mode.

This round completes the de-driverization: afterwards **no driver kernel remains**, so
`modload.s`/`modfree.s`, the o65 module loader, and `libref.s` leave the library, and
`DRVTYPES` disappears from the build. The same sweep removes the API families that
never worked on the Lynx at all — mouse, extended memory (em), the dbg monitor, and
the output half of conio (§1.3).

## 1. What exists today, and what it costs

### 1.1 joy (lynx-stdjoy)

The entire driver payload is one instruction sequence: `lda JOYSTICK; and #$F3`
($FCB0, mask drops Opt1/Opt2 which belong to `kbhit`). Around those 7 bytes:

| Component | Where | Cost |
|---|---|---|
| Driver header (sig, version, libref, 4-entry jump table) | `lynx-stdjoy.s` | 14 bytes |
| RAM jump vectors (4 × `jmp`) + `_joy_drv` | `joy-kernel.s` | 12 + 2 bytes RAM |
| `joy_install` (sig check, libref patch, vector copy) | `joy-kernel.s` | ~70 bytes |
| `joy_load_driver`/`joy_unload` → `mod_load`/`mod_free` | `libsrc/joystick` | ~1 KB when referenced |
| `joy_static_stddrv` indirection, `.joy` o65 artifact | `joy_stat_stddrv.s`, Makefile | build complexity |

`INSTALL`/`UNINSTALL`/`COUNT` are stubs (Lynx joypad is fixed hardware, count is 1).
The driver's `READ` already ignores the joystick number in A.

### 1.2 ser (lynx-comlynx)

Unlike joy, the comlynx driver has real substance — baud/format setup on timer 4,
256-byte ring buffers each way, and an IRQ handler — all of which survives unchanged.
What goes is the wrapper:

| Component | Where | Cost |
|---|---|---|
| Driver header (sig, version, libref, 9-entry jump table) | `lynx-comlynx.s` | 24 bytes |
| RAM jump vectors (8 × `jmp` + IRQ stub) + `_ser_drv` | `ser-kernel.s` | 27 + 2 bytes RAM |
| `ser_install` (sig check, libref patch, vector copy, IRQ-stub activation) | `ser-kernel.s` | ~90 bytes |
| Self-modifying IRQ stub (`RTS`↔`JMP` patched at install/uninstall) | `ser-kernel.s` `.data` | 3 bytes + patch code |
| Kernel wrappers (`ser_open/get/status/ioctl` ptr1 setup) | `libsrc/serial` | ~30 bytes |
| `ser_load_driver`/`ser_unload` → `mod_load`/`mod_free` | `libsrc/serial` | ~1 KB when referenced |
| `SER_IOCTL` entry | driver | dead — returns `SER_ERR_INV_IOCTL` |

The 9-entry jump table references every driver entry point, so any ser use links the
whole driver plus both 256-byte buffers. (The buffers stay all-or-nothing in the new
design too — any meaningful use opens the port — but the dead install/load path goes.)

### 1.3 The stragglers: mouse, em, dbg, and the conio torso

`DRVTYPES = emd joy mou ser`, but the Lynx has no emd or mou driver, and several
whole API families compiled into `lynx.lib` cannot actually be linked. Each was
reviewed; all four verdicts are *delete*:

**mouse — no hardware, ever.** The Lynx has no pointing device and no mouse driver
ever existed for it. `mouse-kernel.s` imports `mouse_libref`, which nothing in this
tree exports (`libref.s` provides only `joy_libref`/`ser_libref`), so
`mouse_install` — the gateway to the other 12 entry points — is already unlinkable.
The entire stack goes: `libsrc/mouse/` (13 files), `include/mouse.h`,
`include/mouse/mouse-kernel.h`, `asminc/mouse-kernel.inc`, `mou` from `DRVTYPES`.

**em (extended memory) — no hardware to back it.** The em API exists for banked
memory (REUs, VDC RAM, RAM expansions): `em_map`/`em_use` hand out writable 256-byte
pages mapped into the address space, `em_commit`/`em_copyto` write them back. The
Lynx has a flat 64 KB with no MMU and no banking; the only other storage is the
cart ROM — read-only, and already properly served by the file API (`open`/`read`/
`lseek`) — and the tiny audio-cassette-class EEPROM, which has its own `eeprom.s`
API. Nothing on this machine can implement an em driver's contract. Like mouse, it
is also already unlinkable (`em_libref` unresolved). Deleted: `libsrc/em/`
(9 files), `include/em.h`, `include/em/em-kernel.h`, `asminc/em-kernel.inc`,
`asminc/em-error.inc`, `emd` from `DRVTYPES`.

**dbg — depends on conio output that the Lynx never had.** The `dbg.c` debug monitor
is a full-screen conio application (`cputc`, `revers`, `screensize`, window
drawing). The Lynx provides no conio output (next paragraph), so `dbg` has been
unlinkable since the port existed. Deleted: `libsrc/dbg/` (6 files),
`include/dbg.h`.

**conio — delete the whole API.** The output half never existed: every file in
`libsrc/conio/` ultimately imports `_cputc`, `gotoxy`, `screensize`, or `cursor` —
none of which any Lynx code exports — so `cprintf`/`vcprintf`/`cputs`/`cputhex`/
`cscanf`/`vcscanf`/`screensize`/`cursor` are all unlinkable, and `doc/lynx.sgml`
already states "No conio support is currently available for the Lynx". The input
half (`kbhit`/`cgetc`) is a keyboard costume over the Opt1/Opt2/Pause switches,
which are joypad inputs, not a keyboard — they move into `joy_read` (§2), and
`conio.h`, `libsrc/conio/`, `libsrc/lynx/kbhit.s`, and `cgetc.s` are all deleted.
Nothing else in the tree references them (only the equally-deleted `vcscanf` and
`dbg.c` did; stdio does not route through `cgetc` on this target).

*Alternative considered and rejected:* implementing `cputc` over the Lynx graphics text path
to revive full conio. Lynx graphics text draws sprites with no character-cell grid, no
cursor model, and no scrolling; conio semantics would demand a CPU-rendered text
console, contradicting the sprite-only premise of `LYNX_GFX_DESIGN.md`.
`gfx_outtext`/`gfx_outtextxy` are the supported text output.

## 2. New joy architecture: one call for every input

The Lynx has exactly nine digital inputs: d-pad (4), A, B, Opt1, Opt2, Pause. Today
they are split across two APIs: `joy_read` returns $FCB0 masked with `#$F3` (Opt1/Opt2
stripped out), and the conio `kbhit`/`cgetc` pair re-reads $FCB0/$FCB1 to dress the
three switches up as a 3-key "keyboard" with edge-detect, debounce, and chord logic
returning ASCII (`'1'`, `'2'`, `'P'`, `'R'`, `'F'`, …). That split dies: **`joy_read`
returns all nine inputs, and the keyboard costume is deleted** (§2.2).

### 2.1 joy_read

One module, `libraries/core/joy-read.s`:

```asm
; unsigned joy_read (void);
_joy_read:
        lda     SWITCHES        ; Pause switch, bit 0
        and     #$01
        tax                     ; → bit 8 of the result
        lda     JOYSTICK        ; Up/Down/Left/Right, Opt1, Opt2, B, A
        rts
```

10 bytes, leaf, no state, no init — `JOYSTICK`/`SWITCHES` are read-only Suzy
registers, plain `LDA` (no RMW concern under Lynx graphics design §5). The return type widens
`unsigned char` → `unsigned` so Pause fits in bit 8; on cc65's ABI the high byte rides
in X for free, so one call snapshots the complete input state atomically. There is no
joystick argument: the Lynx has exactly one joypad, so the old per-stick selector carried
no information. `JOY_1` and `JOY_2` are both deleted.

`lynx/lynx.h` keeps its six existing masks/macros unchanged (the low byte is raw $FCB0, so
they still match) and gains the three that the `#$F3` mask used to hide:

```c
#define JOY_OPT1_MASK   0x0008
#define JOY_OPT2_MASK   0x0004
#define JOY_PAUSE_MASK  0x0100
#define JOY_OPT1(v)     ((v) & JOY_OPT1_MASK)
#define JOY_OPT2(v)     ((v) & JOY_OPT2_MASK)
#define JOY_PAUSE(v)    ((v) & JOY_PAUSE_MASK)
```

`lynx/joystick.h` rewritten (~30 lines):

```c
unsigned joy_read (void);
```

There is exactly one always-present joypad, so the old `joy_count()` query
(and its `JOY_COUNT` constant) carried no information and was removed.

`joy_install`, `joy_uninstall`, `joy_load_driver`, `joy_unload`, `joy_static_stddrv`,
`joy_stddrv`, `_joy_drv`, and all `JOY_ERR_*` codes are deleted — with no install step
there is nothing left to fail.

### 2.2 kbhit/cgetc deleted

`libsrc/lynx/kbhit.s` (edge/debounce state machine, 6 bytes data + ~90 bytes code) and
`cgetc.s` (chord decoding to ASCII) are deleted along with `conio.h`. Rationale:

- Their entire input source is the three switches `joy_read` now reports directly.
- Edge detection ("pressed this frame") is one XOR against the previous frame's value,
  in the main loop where the polling cadence is actually known. Baking a debounce
  state machine into the library imposed policy; at frame-rate polling the switches
  don't need debouncing at all.
- The chord conventions (`Pause+Opt1` = restart, `Pause+Opt2` = flip) are *game*
  conventions, not hardware — honoring them becomes application responsibility, and
  the `lynx/joystick.h` comment says so.

```c
unsigned now = joy_read ();
unsigned pressed = now & ~prev;          /* edge: new this frame */
if (JOY_PAUSE (pressed)) { ... }
prev = now;
```

## 3. New ser architecture

The comlynx driver body moves into static modules in `libraries/core/`; the kernel,
header, jump table, and install path are deleted. All routines keep their current
logic — this is a re-packaging, with two deliberate fixes noted below.

### 3.1 Surviving API

| Function | Implementation notes |
|---|---|
| `ser_open(params)` | Direct `__fastcall__`: `sta/stx ptr1` absorbed from the old kernel wrapper, then the existing baud-table + format checks (timer 4 setup, `contrl` composition). Fallible: returns `SER_ERR_BAUD_UNAVAIL` / `SER_ERR_INIT_FAILED` / `SER_ERR_OK`. |
| `ser_close()` | **Behavior fix (flagged §6):** the old driver's CLOSE was a stub that returned OK without touching hardware. Now it does what its comment promised: write `contrl` minus both int-enables to `SERCTL`, stop timer 4 (`stz TIM4CTLA`), clear `TxDone`, reset all four ring pointers. |
| `ser_get(&b)` | Unchanged ring-buffer pop; `SER_ERR_NO_DATA` when empty. |
| `ser_put(b)` | Unchanged ring-buffer push + Tx-IRQ kick; `SER_ERR_OVERFLOW` when full. |
| `ser_status(&s)` | Unchanged: copies `SerialStat` (error/overflow bits accumulated by the IRQ). |

Return-code error model is kept — unlike Lynx graphics, these calls are genuinely fallible at
runtime. `ser_ioctl` (dead on Lynx), `ser_install`, `ser_uninstall`,
`ser_load_driver`, `ser_unload`, and `_ser_drv` are deleted.

`ser-error.inc`/`lynx/serial.h` keep only the reachable codes, renumbered contiguously:

```
SER_ERR_OK = 0, SER_ERR_BAUD_UNAVAIL, SER_ERR_NO_DATA,
SER_ERR_OVERFLOW, SER_ERR_INIT_FAILED
```

(`NO_DRIVER`, `CANNOT_LOAD`, `INV_DRIVER`, `NO_DEVICE`, `INV_IOCTL`, `INSTALLED`,
`NOT_OPEN` deleted.)

### 3.2 IRQ hook

Mirror of Lynx graphics design §2.4: the handler is exported directly with
`.interruptor ser_irq, 29` from the core module. The kernel's self-modifying
`RTS`↔`JMP` stub and its install/uninstall patching disappear. **Priority 29 is kept
explicitly** — it must run before default-priority interruptors (e.g. `gfx_vbl_irq`)
because a serial byte must be drained before a frame-swap handler burns cycles. The
handler's first action (poll `INTSET` for `SERIAL_INTERRUPT`, `clc`/`rts` if clear)
already makes it safe when linked but the port never opened: timer 4 is never started,
the bit never sets.

### 3.3 Module split (smart-linking layout)

```
libraries/core/
  ser-core.s    TxBuffer/RxBuffer (512 B .bss), ring pointers, contrl,
                SerialStat, TxDone + the IRQ handler (.interruptor ser_irq, 29)
  ser-open.s    ser_open (baud table, format checks)
  ser-close.s   ser_close
  ser-get.s     ser_get
  ser-put.s     ser_put
  ser-status.s  ser_status
```

Every entry point references core state, so `ser-core.s` (and the interruptor) links
whenever any ser function is used — correct, since any use requires `ser_open` and a
live IRQ handler. A program that only sends never links the `ser_get` code, and a
ser-free program links none of it (verify: interruptor absent from the map, §7).

Buffer size stays 256 per direction: the ring arithmetic relies on natural 8-bit
pointer wraparound. Not tunable without rewriting the pointer logic; documented in
`ser-core.s`.

### 3.4 Hardware rules (Lynx graphics design §5 applied)

- `SERCTL` is write-only (its read view is status bits). Every write already goes
  through the `contrl` shadow — the new modules keep that pattern and state it as an
  invariant in `ser-core.s`. No RMW opcodes target it; Mikey RMW is legal anyway, the
  Suzy ban doesn't apply ($FD8C/$FD90s).
- The Tx/Rx interrupts are level-sensitive (hardware bug, noted in the existing IRQ
  comment): the disable-before-clear dance in the handler is load-bearing; preserved
  byte-for-byte.
- Timer 4 is the hardware-designated baud generator; only ser touches it (Lynx graphics uses the
  VBL timer). `uploader.s` reads `SERCTL` status directly during BLL upload — it runs
  before main, never concurrent with an open port; unchanged.

### 3.5 Pre-existing quirks, documented not fixed

Carried into `lynx/serial.h` comments: Lynx parity includes the parity bit itself in its
calculation (EVEN/ODD are nonstandard on the wire); ComLynx is open-collector, so
every transmitted byte is also received by the sender (loopback — useful for §7);
`SER_PAR_NONE` is rejected (hardware always sends a ninth bit; use MARK/SPACE);
a received break drops all four buffer pointers; only 8 data bits / 1 stop bit.
`lynx/serial.h` keeps only the baud constants comlynx implements (62500, 31250, 9600, 7200,
4800, 3600, 2400, 1800, 1200, 600, 300, 150, 134.5, 110, 75); the RS-232 leftovers
(45.5 … 230400, 19200, 38400, 57600, 115200, 56.875) are deleted.

## 4. The payoff: module loader and libref leave the library

With graphics (done), joy, and ser static, nothing constructs or loads o65 modules and
nothing needs a library back-reference:

- `libsrc/common/modload.s`, `modfree.s`; `include/modload.h`; `asminc/modload.inc` —
  deleted. (~1 KB of loader plus its `open/read/close` pull-in no longer reachable
  from any library path.)
- `libsrc/lynx/libref.s` (`joy_libref`/`ser_libref` := `_exit`) — deleted; the last
  importers die with the kernels.
- `module.mac` (`module_header`) loses its last users. The macpack itself lives in the
  ca65 source (toolchain, out of scope); `asminc/module.mac` is deleted from the
  shipped tree.
- `DRVTYPES` and the `DRVTYPE_template` machinery in `libsrc/Makefile` — deleted
  entirely (emd/mou were never usable on Lynx, §1.3; joy/ser now static). No `.joy`/
  `.ser` artifacts under `target/lynx/drv/`.
- With dbg and the conio output chain gone (§1.3), `lynx.lib` no longer carries any
  object that cannot be linked into a Lynx program.

## 5. Deletions

**Directories (entire):** `libsrc/joystick/` (`joy-kernel.s`, `joy_load.s`,
`joy_unload.s`, `joy_count.s`, `joy_read.s`), `libsrc/serial/` (`ser-kernel.s`,
`ser_load.s`, `ser_unload.s`, `ser_open.s`, `ser_close.s`, `ser_get.s`, `ser_put.s`,
`ser_status.s`, `ser_ioctl.s`), `libsrc/em/`, `libsrc/mouse/`, `libsrc/dbg/`,
`libsrc/conio/` (§1.3).

**`libsrc/lynx/`:** `joy/lynx-stdjoy.s` (replaced by `joy/joy-read.s`),
`ser/lynx-comlynx.s` (split per §3.3), `joy_stat_stddrv.s`, `libref.s`, `kbhit.s`,
`cgetc.s` (§2.2).

**`libsrc/common/`:** `modload.s`, `modfree.s`.

**Headers:** `include/joystick/joy-kernel.h`, `include/modload.h`, `include/em.h`
and `include/em/`, `include/mouse.h` and `include/mouse/`, `include/dbg.h`;
`asminc/joy-kernel.inc`, `asminc/joy-error.inc`, `asminc/ser-kernel.inc` (the
`SER_PARAMS` struct and surviving baud/format constants move to a slim
`asminc/ser.inc`), `asminc/ser-error.inc` (shrinks to the 5 codes, or folds into
`ser.inc`), `asminc/modload.inc`, `asminc/module.mac`, `asminc/em-kernel.inc`,
`asminc/em-error.inc`, `asminc/mouse-kernel.inc`, `include/conio.h` (§2.2).
`lynx/joystick.h` and `lynx/serial.h` rewritten per §2/§3; `lynx/lynx.h` gains the
`JOY_OPT1/OPT2/PAUSE` masks (§2.1).

**Build:** `DRVTYPES` block and `$(foreach drvtype,…)` template in `libsrc/Makefile`.

**Docs:** `doc/lynx.sgml` driver sections (`lynx-stdjoy.joy`, `lynx-comlynx.ser`)
plus its em/mouse "no drivers available" stubs; the keyboard/conio section is
replaced by a note pointing at `joy_read` and the new masks; `doc/funcref.sgml`
loses the `em.h`, `mouse.h`, `dbg.h`, and `conio.h` entries;
`doc/joystick.sgml`/`ser*.sgml` if present.

**Samples:** remove `joy_install (joy_static_stddrv);` from `breakout.c`,
`lynxdemo.c`, `setbpp.c`, `suzybench.c`. No other change — `joy_read` call sites
compile unchanged.

## 6. Behavior changes — flagged loudly

- **`ser_close` now actually closes** (§3.1): disables serial interrupts and stops
  timer 4. Programs that called `ser_close` and kept expecting Rx bytes were broken by
  any reasonable reading of the API; nonetheless this is a runtime-visible change —
  called out in `lynx/serial.h`.
- **`SER_ERR_*` values renumber** (§3.1). Code comparing against the macro names is
  fine; code using bare integers breaks silently — called out in `lynx/serial.h`.
- **`joy_read` reports more bits** (§2.1): Opt1/Opt2 now appear in the low byte
  (formerly masked) and Pause in bit 8. Code testing specific masks is unaffected;
  code treating the whole return as a boolean ("any input?") now also triggers on
  the switches — called out in `lynx/joystick.h`. The widened return type is
  source-compatible (`unsigned char` promotes).
- Everything else is compile-time: deleted functions fail at link/compile, the
  intended failure mode.

## 7. Expected impact

| Item | Today | After |
|---|---|---|
| `joy_read` path | wrapper alias → RAM `jmp` → driver | 10-byte leaf, direct `jsr` |
| joy code+RAM linked | ~150 B code + 14 B RAM (kernel+vectors+driver hdr) | 10 B |
| Input APIs | joy_read (6 of 9 inputs) + kbhit/cgetc (~140 B, stateful) | joy_read, all 9 inputs |
| ser per-call overhead | kernel wrapper + RAM `jmp` | direct `jsr` |
| ser code+RAM linked | whole driver + kernel + 29 B RAM vectors | modules used + 518 B bss (buffers/state, unchanged) |
| Startup | `joy_install` sig-check + vector copy (+ ser equivalent) | nothing |
| o65 loader reachable from lib | ~1 KB via `*_load_driver` | gone |
| API entry points | joystick.h 6 + serial.h 9 | 2 (1 macro) + 5 |
| Unlinkable objects in `lynx.lib` | em (9) + mouse (13) + dbg (6) + conio (9) | 0 |
| Headers shipped that cannot work | em.h, mouse.h, dbg.h, most of conio.h | none |

## 8. Verification plan

1. Rebuild `lynx.lib`; build all four joy-using samples; run `breakout`/`lynxdemo` in
   Handy/Mednafen — input behavior identical.
2. ComLynx self-test (new `samples/serial.c` or minimal test ROM): open 62500 8-mark-1,
   `ser_put` a pattern, poll `ser_get` — open-collector loopback (§3.5) returns each
   byte to the sender; check `ser_status` clean; then `ser_close` and confirm no
   further IRQs fire (INTSET bit stays clear). Emulator first, redeye hardware second.
3. `.map` diffs: (a) joy-only program links 7 bytes of joy and **no** interruptor, no
   buffers; (b) ser program shows `ser_irq` in the interruptor chain and no
   `_ser_install`/`_ser_drv`/`mod_load` symbols; (c) a graphics-only program links neither.
4. Grep audit: zero remaining references to `module_header`, `MOD_CTRL`, `mod_load`,
   `mod_free`, `libref`, `JOY_HDR`, `SER_HDR`, `DRVTYPES`, `_cputc`, `kbhit`,
   `cgetc`, `conio`, `em_`, `mouse_`, `_dbg` anywhere in `libsrc/`, `asminc/`,
   `include/`, `cfg/`, `samples/`.
5. Perniciousness audit: every `SERCTL` store preceded by a `contrl` shadow
   composition; no RMW opcode targets $FCxx (joy reads $FCB0 with plain `LDA`).
6. IRQ-order check: confirm `ser_irq` precedes `gfx_vbl_irq` in the generated
   interruptor table (priority 29 honored) when both link.

## 9. Implementation order

Each step shippable:

1. **joy** (independent, trivial): add `joy/joy-read.s`, rewrite `lynx/joystick.h`, add
   the three masks to `lynx/lynx.h`, delete `libsrc/joystick/`, `lynx-stdjoy.s`,
   `joy_stat_stddrv.s`, `kbhit.s`, `cgetc.s`, `conio.h`, `libsrc/conio/`; fix the
   four samples.
2. **ser**: split `lynx-comlynx.s` into the §3.3 modules with direct entry symbols
   (including the `ser_close` fix and the `.interruptor` export); rewrite `lynx/serial.h`
   and the asm includes; delete `libsrc/serial/`.
3. **Sweep**: delete `modload`/`modfree`/`modload.h`/`modload.inc`/`module.mac`,
   `libref.s`, `libsrc/em/`, `libsrc/mouse/`, `libsrc/dbg/` + their headers; remove
   `DRVTYPES` from `libsrc/Makefile`; update `doc/lynx.sgml`/`funcref.sgml`.
4. Verification pass per §8 with a `.map`-based size report.

## 10. Out of scope / follow-ups

- `co65` and ld65's o65 *output* support are toolchain features, untouched — only the
  library-side module loading dies here.
- A second-player joystick over ComLynx (reading a remote pad via the redeye protocol)
  would be a new feature on top of the static ser core, not part of this cleanup. It
  would need its own entry point, not `joy_read`, which reports only the local pad.
- Lynx graphics design §9's remaining items (text-scale exposure, collision readback) are
  unaffected.
