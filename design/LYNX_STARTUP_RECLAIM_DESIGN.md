<!--
SPDX-License-Identifier: CC-BY-4.0
Lynx Game Development SDK documentation, (c) 2026 the lynxcc authors,
licensed under Creative Commons Attribution 4.0 International.
See doc/licenses.html.
-->

# Design: Reclaiming the one-shot startup code

Status: **IMPLEMENTED** (2026-07-09). Full variant (A + B + C). This document is
the source of truth for the change.

## Implementation notes (2026-07-09)

Implemented as designed, with one deliberate refinement to §C/§4: rather than a
custom ld65 load/run *memory area* split (whose `run` address cannot be a
computed end-of-BSS on this target), `ONCE` simply moves to the end of the
segment list, after `BSS`. ld65 then runs it at `__ONCE_RUN__` (= end of BSS,
the heap origin) while packing its bytes into the image with no BSS hole. The
relocator's source address is exported per-cfg as `__ONCE_PHYS__`:

- **Plain carts** (`lynx.cfg`, `lynx-coll.cfg`, `lynx-bll.cfg`,
  `lynx-uploader.cfg`): `ONCE` packs straight after `DATA`, so after the
  verbatim copy it physically lands at `__BSS_RUN__`; the cfg sets
  `__ONCE_PHYS__ = __BSS_RUN__` and the relocator copies it up to
  `__ONCE_RUN__`.
- **Cart-size carts** (`lynx-128k/256k/512k.cfg`): `MAIN`'s `fill = yes` spans
  the BSS gap, so `ONCE` is already loaded *at* `__ONCE_RUN__`; the cfg sets
  `__ONCE_PHYS__ = __ONCE_RUN__` and the relocator is a harmless self-copy.

The single directory copy length is unified to
`len0 = __ONCE_PHYS__ + __ONCE_SIZE__ - __MAIN_START__` (`runtime/lynx/defdir.s`);
for the plain carts this equals the old sum-of-segment-sizes, for the padded
carts it also spans the filled BSS gap ahead of `ONCE`. The BLL header length
word (`runtime/lynx/bllhdr.s`) uses the same expression plus its 10-byte header.

Files: `runtime/lynx/crt0.s` (split + relocator + `_exit`-push tail-jump),
`runtime/lynx/defdir.s`, `runtime/lynx/bllhdr.s`, `libraries/libc/_heap.s`
(origin → `__ONCE_RUN__`), all seven `cfg/lynx*.cfg` (ONCE after BSS +
`__ONCE_PHYS__` export). The relocator uses a byte-wide descending copy guarded
by `.assert __ONCE_SIZE__ < $100`.

Verification (from-source sandbox rebuild): `STARTUP` shrank from 107 to 27
resident bytes; the 137-byte one-shot body now lives at `__ONCE_RUN__` and is
reclaimed. GearLynx goldens 7/8 (`mikey/setbpp` is the pre-existing
toolchain-rebuild screenshot drift, and uses no heap); `heaptest` reports 17/17
"ALL CHECKS PASSED" with `_heaporg == __ONCE_RUN__`, proving the reclaimed bytes
are usable; a `lynx-128k` cart and an end-to-end `lnx bll --lnx` ROM both boot
byte-for-byte to the default `lynxdemo` screenshot hash; host unit tests
927343/0. `_exit` resolves to a fixed permanent address in `STARTUP`
(`$0203`), well below the heap origin.

Source of truth for shrinking the resident footprint of `runtime/lynx/crt0.s` so
that only the reset entry vector and the `_exit` trap remain permanently in
memory, while the one-time hardware/runtime initialisation sequence is moved into
a segment that the C heap grows over and reclaims after `main()` starts.

Companion to `LYNX_SDK_LAYOUT_DESIGN.md` (segment/memory partitioning) and the
memory map documented in `doc/memory.html`. Those describe the *current* fixed
layout; this document changes *where the startup code lives and when its bytes
are freed*. No public API, call contract, or codegen change.

## 1. Why

On the Lynx the whole program image is copied verbatim to RAM at `$0200` and run
in place. `crt0.s` emits a single `STARTUP` segment that sits at the very bottom
of `MAIN` and contains two very different kinds of code welded together:

1. **One-shot init** — mask IRQs, reset the CPU stack, set bank switching,
   disable every timer and the serial IRQ, clear pending interrupts, set up the C
   software stack pointer, program the Mikey and Suzy hardware registers, seed the
   RAM shadow regs, `jsr zerobss`, `jsr initlib`, `jsr callmain`. This runs
   exactly once, before `main()`, and is dead weight forever after.

2. **The `_exit` trap** — `sei` + `bra` self-loop that `main()` returning and the
   stack-overflow checker (`stkchk.s`) both branch to. This must stay resident
   for the entire run.

Because `STARTUP` is placed first in `MAIN`, underneath `CODE`/`RODATA`/`DATA`/
`BSS`, its bytes are trapped beneath live segments: neither the heap (growing up
from the end of `BSS`) nor the C stack (growing down from `$C037`) can ever reach
back down to `$0200` to reuse them. So the ~90–110 bytes of one-shot init sit
resident and unreclaimed for the life of the program. (Exact size to be read from
the link map during implementation; the number is illustrative here.)

This is not a pressing shortage — `MAIN` has tens of KB free — so the change is
justified on cleanliness and correctness-of-layout grounds, and the design
deliberately records the cost side honestly (§7) so the reclaim half can be
declined without losing the split half.

## 2. Constraints

Any solution must respect four hard facts about this target:

- **C1 — Entry is fixed at `$0200`.** The bootloader jumps to the start of
  `MAIN`, so the first bytes executed must be the start of `STARTUP`. The reset
  entry cannot move.
- **C2 — `_exit` must stay resident.** It is the landing pad for `main()`
  returning (via `callmain`) and for `stkchk.s`. It must occupy an address that
  is never overwritten.
- **C3 — Verbatim copy, no free load≠run.** The bootloader copies the file image
  to `$0200` byte-for-byte; there is no platform loader step that relocates
  segments or fills `bss` gaps. File order therefore equals memory order, and a
  `type = bss` segment cannot sit *between* two file-backed segments without
  either padding the cart image by the full BSS size or performing an explicit
  runtime copy. This is the constraint the other cc65 targets (C64/Atari) do not
  have, and it is why their "ONCE overlaps the heap" configs work for free and
  ours cannot.
- **C4 — `BSS` must stay below the heap.** `BSS` holds live static storage for
  the whole run; the heap must grow *above* it, not through it.

## 3. Design overview

Three independent pieces. Pieces A and B are cheap and self-contained; piece C is
the part that actually frees the memory and carries the real complexity.

### A. Split `crt0.s` — permanent stub vs. one-shot body

`STARTUP` shrinks to the entry vector and the trap only:

```asm
        .segment "STARTUP"
        jmp     __boot          ; reset entry at $0200 -> one-shot init
_exit:  sei                     ; resident trap (main() return + stkchk)
noret:  bra     noret
```

The entire one-shot init sequence moves under label `__boot` into the reclaimable
segment (§C). This satisfies C1 (entry still first at `$0200`) and C2 (`_exit`
lives in the permanent `STARTUP`). Cost: one 3-byte `jmp`.

### B. Return path — nothing reclaimable may outlive `main()`

The current code ends the init sequence with `jsr callmain`, and `callmain` ends
with `jmp _main` (it never returns to its caller; `main()`'s `rts` returns to
whatever address the init code left on the stack — today the `_exit` label
sitting right after `jsr callmain`). If the init body is moved into reclaimable
memory, that return-address instruction would have to survive `main()` — but by
then the heap has overwritten it.

Fix: have the one-shot body push `_exit` itself and *tail-jump* into `callmain`,
so the only surviving return target is the permanent `_exit`:

```asm
        lda     #>(_exit-1)     ; 6502 rts returns to pushed-addr + 1
        pha
        lda     #<(_exit-1)
        pha
        jmp     callmain        ; callmain jmp's _main; main()'s rts -> _exit
```

After this, no instruction in the reclaimable region needs to exist once `main()`
is running. `callmain`, `zerobss`, `initlib`, `condes` themselves stay in their
current permanent segments (`CODE`/`libc`); only the crt0 *driver* sequence
relocates.

### C. Reclaim the one-shot body via the heap

The one-shot body goes into the `ONCE` segment — the segment cc65 already
reserves for run-once code (`initlib` and the `initheap` constructor already live
there). To make `ONCE` reclaimable it must (a) occupy addresses the heap will
grow into, and (b) the heap bottom must point at its base.

Because of C3+C4, `ONCE` cannot simply be reordered above `BSS` in the image
(that would put a `bss` hole mid-file). Instead `ONCE` gets a **split load/run
address** via ld65:

- **load** address: packed into the file image immediately after `DATA`
  (contiguous, no hole — satisfies C3 for the on-disk image).
- **run** address: the top of the static area, at the current heap origin (end of
  `BSS`), so it sits in the region the heap will consume.

A small **permanent** relocator in `STARTUP` (the `__boot` target) copies `ONCE`
from its load address to its run address, then `jsr`s the relocated copy. The
heap origin is then set to the `ONCE` run base so the first `malloc` begins
overwriting the spent init code. This satisfies C4 (the heap still starts above
`BSS`; `ONCE`'s run image lives exactly at the old heap origin) and C2/C3 (the
relocator and `_exit` are permanent; the file image stays hole-free).

The relocator is the honest cost: it is resident code and it runs a copy loop at
boot, buying back part of the space and time the reclaim saves. See §7.

## 4. Config changes (`cfg/lynx.cfg`)

`ONCE` moves from its mid-`MAIN` position to a split load/run definition placed
after `DATA`/`BSS`, and gains `define = yes` so its run base is exported:

```
SEGMENTS {
    ...
    STARTUP:   load = MAIN, type = ro,  define = yes;   # entry + _exit only
    ...
    DATA:      load = MAIN, type = rw,  define = yes;
    BSS:       load = MAIN, type = bss, define = yes;
    ONCE:      load = MAIN, run = MAIN, type = ro, define = yes, optional = yes;
}
```

The exact `run`/`load` expressions (packing `ONCE`'s load image after `DATA` while
running it at `__BSS_RUN__ + __BSS_SIZE__`) are worked out against ld65's segment
placement during implementation; the intent above is binding, the syntax is not.

The `CONDES` constructor table stays in `ONCE` (unchanged) — it is run-once by
construction and rides along with the reclaimed body.

## 5. Heap origin change (`libraries/libc/_heap.s`)

The heap currently anchors to the end of `BSS`:

```asm
__heaporg:  .word __BSS_RUN__+__BSS_SIZE__
__heapptr:  .word __BSS_RUN__+__BSS_SIZE__
```

These become the `ONCE` run base (`__ONCE_RUN__`), so allocation starts over the
spent init code. `__heapend` (set by the `initheap` constructor from `sp -
__STACKSIZE__`) is unchanged. When `ONCE` is empty (a program with no
constructors and — after this change — never, since the crt0 body is always
present) the symbol must still resolve; `ONCE` is therefore effectively mandatory
here, and `_heap.s` keeps `__BSS_RUN__ + __BSS_SIZE__` as the documented fallback
only if the segment is ever made truly optional again.

Ordering safety: `__heaporg`/`__heapptr` are link-time constants in `DATA`; no
allocation happens until `main()` calls `malloc`, and the entire `ONCE` body has
run and returned (through the `_exit` push) before `main()` begins. So the heap
never overwrites init code that is still executing.

## 6. Variants

The change is deliberately staged so the cheap half can ship alone:

- **Minimal (A + B).** Split `crt0.s`; keep the one-shot body in a normal
  (non-reclaimed) `ONCE` or in `CODE`. Costs one `jmp`, gains clarity and the
  clean `_exit`-only `STARTUP`, reclaims nothing. Zero layout risk.
- **Full (A + B + C).** Adds the load/run split, relocator, and heap-origin
  change. Reclaims the one-shot body (~90–110 bytes minus the resident relocator)
  at the cost of a boot-time copy and added layout complexity.

## 7. Tradeoffs and risks

- **Net savings are modest.** The reclaimed body is offset by the permanent
  relocator; realistic net is on the order of a few dozen bytes on a machine with
  tens of KB free. The value is layout hygiene, not capacity.
- **Boot cost.** The relocator adds a one-time copy at startup (negligible, but
  nonzero).
- **Layout fragility.** The load/run split is the sharpest edge: get the ld65
  expressions wrong and either the image gets a `bss` hole (C3 violation → wrong
  RAM addresses after the verbatim copy) or `ONCE`'s run image overlaps live
  `BSS`. This must be verified by inspecting the link map and by a byte-diff of
  the produced `.lnx`, not assumed.
- **`stkchk` interaction.** `stkchk.s` branches to `_exit`; confirm it still
  resolves to the permanent `STARTUP` copy and not a relocated address.

## 8. Verification plan

Per the repo workflow (full sandbox rebuild; GearLynx boot check; docs in sync):

1. Full toolchain + library + examples rebuild from source.
2. Read the link map to confirm: `STARTUP` contains only the entry `jmp` +
   `_exit`; `ONCE` load image is contiguous after `DATA` with no `bss` hole; the
   heap origin equals `ONCE`'s run base.
3. GearLynx boot every example: clean boot, correct first-frame screenshot
   hashes against the goldens (screenshot-hash goldens, per the test harness).
4. A heap-stress example (allocate enough to grow the heap over the old `ONCE`
   run region) must still run correctly, proving the reclaimed bytes are truly
   free and that no still-needed code was clobbered.
5. Confirm `main()` returning lands on the resident `_exit` (trap spins, no
   run-off), and force a stack overflow to confirm `stkchk` → `_exit`.

## 9. Documentation to update (same pass as code)

Per `CLAUDE.md` docs-track-code:

- `doc/memory.html` — the `MAIN` segment list/diagram and the note that `STARTUP`
  is resident-forever; add the reclaimed-`ONCE` region to the heap map.
- `LYNX_SDK_LAYOUT_DESIGN.md` — segment table / ordering.
- `runtime/lynx/crt0.s` header comment — describe the split and the `_exit`-only
  `STARTUP`.
- `libraries/libc/_heap.s` comment — heap now anchors on `__ONCE_RUN__`.
- This file — flip Status to IMPLEMENTED with an implementation-notes block.
