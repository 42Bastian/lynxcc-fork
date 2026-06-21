# Lynx Suzy math: asynchronous (non-blocking) start / poll / harvest

Status: IMPLEMENTED (2026-06-14). Runtime in `libraries/math/suzyasync.s`, API in
`include/suzymath.h`; samples `examples/suzyasync/suzyasync.c` (perspective starfield) and
`examples/suzyasyncbench/suzyasyncbench.c` (overlap-vs-sync-vs-soft timing). Library and both
ROMs build clean via `make lib` / `make examples`. The C call sites were checked
to pass operands exactly as the asm expects (divisor in A/X, dividend/factors on
the C stack — identical to the operator routines), and a host model of all seven
routines' algorithms validated 0 mismatches over 2,679 corner-pair checks
(normalization, sign fixup, mod recompute, 32-bit muldiv). Emulator/hardware run
pending, as for the other Suzy-math features.

Adds a **non-blocking** way to use the Suzy hardware
math unit: *start* a multiply, divide or modulo and return immediately, run
unrelated CPU work while Suzy computes, then *poll* for completion and *harvest*
the result. This is the natural extension of the synchronous `!*`/`!/`/`!%`
operators (`LYNX_CODEGEN_DESIGN.md` §2.6) — same register protocol, same
hardware contracts, but the wait loop is handed back to the programmer so the
~100 CPU cycles a divide costs can be spent on something else instead of spun
away. It is intended as an explicit, intrinsic-level API; it does **not** change
the operators or any existing routine.

This document is design only. Nothing here is implemented yet. It can ship either
as a standalone unit (`libraries/math/suzyasync.s` + `<lynx/suzymath.h>`) or be folded in
as `LYNX_CODEGEN_DESIGN.md` §2.6.4; the section numbering below assumes the
latter for cross-referencing.

## 1. Motivation and the one case that actually pays

The synchronous routines spend their whole lifetime in a poll loop:

```
        sta     MATHE           ; start divide
@wait:  lda     SPRSYS
        bmi     @wait           ; ~176..386 ticks burned doing nothing
        lda     MATHD           ; harvest
```

Suzy and the 65C02 run concurrently — the CPU is free the instant the start
register is written. The poll loop throws that concurrency away. The async API
keeps it:

```
        suzy_udiv_start(n, d);   /* writes MATHE..MATHP, returns now      */
        ... unrelated CPU work that touches NO Suzy state ...
        q = suzy_udiv_result();  /* poll-if-needed, read MATHD/MATHC      */
```

**Only the divide is worth overlapping.** The economics decide which operations
deserve an async form:

| op | hardware time | CPU cycles to cover | worth async? |
|----|---------------|---------------------|--------------|
| multiply | 44 ticks (~11 cyc) | ~11 | **No** — already done before the next few instructions retire; the call/return overhead exceeds the win. |
| divide | 176 + 14·N ticks (~44–100 cyc) | up to ~100 | **Yes** — the primary target. ~100 cycles of useful work fit in the shadow of one small-divisor divide. |
| fused `(a·b)/c` | multiply + divide, polled between | ~110 | **Partly** — the divide half overlaps; the multiply half + the inter-op poll cannot (see §5). |
| modulo | divide + multiply + subtract | divide half only | **Partly** — same as fused: only the divide overlaps; the `(n/d)·d` tail runs at harvest (see §4). |

So the headline feature is **asynchronous divide**. Async multiply is offered
only for completeness/symmetry (and is documented as "rarely worth it"); async
mod and async muldiv overlap their divide phase and finish the rest at harvest.

## 2. Why this can't be an operator

`!*`/`!/`/`!%` are expression operators: the compiler evaluates the whole
expression to a value at the point of use, so a result is always consumed
immediately and there is nowhere to "return early." Splitting the operation
across two program points (start here, harvest there, arbitrary code between)
is a control-flow shape an operator cannot express. The async path is therefore
an **intrinsic function API**, declared in a new header `<lynx/suzymath.h>`, lowering
to hand-written runtime entries — no `expr.c`/`codegen.c` changes at all. The
operators and their `tossuzy*` routines are untouched.

## 3. The state machine and the new contract

A single math unit means **exactly one async operation may be in flight at a
time**, process-wide. The model is a three-state machine:

```
        IDLE  --start-->  RUNNING  --(MATHWORKING clears)-->  DONE  --harvest-->  IDLE
```

* **start** (`suzy_*_start`): set up the SPRSYS shadow as needed, write the
  operand registers exactly as the synchronous routine does, write the trigger
  register (`MATHA` for multiply, `MATHE` for divide), and **return without
  polling**. For signed ops it also records the sign-fixup state (§6).
* **poll** (`suzy_math_busy`): read `SPRSYS`, test `MATHWORKING` (bit 7, `$80`).
  Cheap enough to inline.
* **harvest** (`suzy_*_result`): read the result registers and apply any sign
  fixup. Defensively re-polls so that calling it too early degrades to
  synchronous rather than returning garbage (§7).

This adds one contract on top of the §2.6 list, and it is the load-bearing one:

> **Between `start` and the matching `result`, the overlapped code must touch no
> Suzy state at all** — no TGI draw, no sprite launch (`SPRGO`), no other Suzy
> math (`!*`/`!/`/`!%` or another async start), and no `SPRSYS` write. Any of
> these either corrupts the in-flight operation or is corrupted by it, because
> the sprite engine shares the very same math registers.

The existing §2.6 contracts carry over unchanged: not interrupt-safe (no Suzy
math in IRQ handlers — and now also *no Suzy math, period,* in any IRQ that can
fire during the overlap window), sprite engine must be idle, accumulate off,
unsafe-access bit may be set spuriously, divide-by-zero returns `$FFFF`. The new
contract is the natural consequence of voluntarily holding the math unit open
across a span of CPU code instead of for the few dozen ticks of a polled call.

The "one in flight" and "no Suzy in the gap" rules are **auditable by grep** for
the same reason the operators are: every async site is an explicit
`suzy_*_start` / `suzy_*_result` pair, so a reviewer can scan the code between a
pair for forbidden Suzy use.

## 4. Proposed C API (`<lynx/suzymath.h>`)

```c
/* ---- completion check (inline, no call overhead) ---- */
/* Nonzero while the unit is still computing. */
#define suzy_math_busy()   (SUZY.sprsys & MATHWORKING)   /* SPRSYS bit7 */

/* ---- divide: the primary async use ---- */
void     suzy_udiv_start (unsigned n, unsigned d);
unsigned suzy_udiv_result (void);          /* quotient; defensively polls */
void     suzy_div_start  (int n, int d);
int      suzy_div_result (void);

/* ---- modulo: overlaps the divide, finishes (n/d)*d at harvest ---- */
void     suzy_umod_start (unsigned n, unsigned d);
unsigned suzy_umod_result (void);
void     suzy_mod_start  (int n, int d);
int      suzy_mod_result (void);

/* ---- multiply: provided for symmetry; rarely worth it ----
** No signed variant exists: cc65's int*int keeps only the low 16 bits of
** the product, and the low word of a two's-complement multiply is identical
** for signed and unsigned operands (the sign only affects the discarded high
** bits). So signed multiply IS the unsigned routine, reinterpreted as int --
** no pre-negation, no result fixup, no carried sign state. Suzy's SIGNMATH
** mode stays off (it is unnecessary here and buggy). Hence the aliases: */
void     suzy_umul_start (unsigned a, unsigned b);
unsigned suzy_umul_result (void);          /* low 16 bits, signed==unsigned */
#define  suzy_mul_start  suzy_umul_start
#define  suzy_mul_result suzy_umul_result
/* (A FULL 32-bit signed product would need magnitude reduction + a parity
** fixup like muldiv (§5/§6); that is out of scope -- !* returns 16 bits.) */

/* ---- fused (a*b)/c: all 3 at start, divide half overlaps (see §5) ---- */
void     suzy_umuldiv_start (unsigned a, unsigned b, unsigned c);
unsigned suzy_umuldiv_result (void);
void     suzy_muldiv_start  (int a, int b, int c);
int      suzy_muldiv_result (void);
```

Typical use, the case worth writing:

```c
#include <lynx/suzymath.h>

suzy_udiv_start(distance, speed);   /* start the slow divide            */
update_score();                     /* ~80 cycles of NON-Suzy work      */
ticks = suzy_udiv_result();         /* unit is already idle: free read  */
```

ABI: the `start` routines take the two operands the same way the `tos*` routines
receive theirs (one in `A/X`, one on the C stack) or as a thin `__fastcall__`
wrapper; the `result` routines take no operand (except the deferred divisor `c`
for muldiv, see §5). This is a codegen-free, library-only addition: the
intrinsics are ordinary external functions.

### 4.1 Modulo is two-phase

The hardware remainder registers are buggy and unused (§2.6); modulo is computed
as `n − (n/d)·d`. Only the **divide** can overlap unrelated CPU work. The
multiply and subtract must run *after* the quotient settles, so they live in
`suzy_*mod_result`:

* `suzy_umod_start(n,d)` saves `n` and `d` (zero page / BSS, §6), normalizes the
  small divisor (§2.6.3), and starts the divide.
* `suzy_umod_result()` polls the divide done, then — exactly as `suzyumod.s`
  does today — writes the saved divisor into `MATHB/MATHA` to start the
  `(n/d)·d` multiply, polls that (~44 ticks), and returns `n − (n/d)·d`.

So async mod hides the long divide but pays a short, fixed `~44-tick + subtract`
tail at harvest. That is still strictly better than the fully synchronous mod,
and the API shape is identical to the divide; the cost asymmetry is documented,
not hidden.

## 5. Fused muldiv async

`suzymuldiv.s` chains multiply→divide by polling between them and rewriting
`MATHE` with its own value to trigger the divide on the settled 32-bit product.
The async form splits the chain at the only place the CPU can be released — after
the divide is started:

* `suzy_umuldiv_start(a,b)` writes the factors, starts the multiply, **polls the
  multiply** (short, ~44 ticks — unavoidable, it gates the divide), then starts
  the divide by rewriting `MATHE`, and returns. The divisor `c` is *not* needed
  until the divide is already running, so it is supplied to the `result` call.
* `suzy_umuldiv_result(c)` — wait: the divisor must be in `MATHN/MATHP` *before*
  `MATHE` is rewritten. Two clean options:
  1. **Divisor at start** (recommended): `suzy_umuldiv_start(a,b,c)` takes all
     three, does multiply+poll+start-divide, returns; `result()` just harvests.
     The overlapped window is the divide only — exactly the part worth hiding.
  2. **Divisor at result**: `start(a,b)` does multiply+poll and stops with the
     product sitting in `MATHE..MATHH`; `result(c)` writes the divisor and
     triggers the divide, then polls. This overlaps *nothing* (the divide hasn't
     started during the gap), so it is rejected — it would only overlap the
     multiply, which is pointless.

Therefore the fused async form takes all three operands at start (option 1); the
header above should read `suzy_umuldiv_start(a,b,c)` / `suzy_umuldiv_result()`.
Signed muldiv reduces all three to magnitudes at start and stores the parity bit
for harvest (§6). The 32-bit-product overflow fix and the §2.6.3 caveat (do not
shift-normalize the muldiv divide unless `MATHE==0 && c<256`) are inherited
unchanged.

## 6. Sign and operand state must survive the gap

This is the real implementation wrinkle and the reason async signed math is not
free. The synchronous signed routines keep their sign decision in registers/zero
page *across a poll loop that never returns to the caller*. Async hands control
back to arbitrary C between start and harvest, so any state the harvest needs
must be parked in memory that the overlapped code won't clobber — not in A/X/Y,
not in the shared `tmp*`/`ptr1` scratch the C code may reuse.

State to persist, per op:

* **signed divide / mod / muldiv:** a 1-byte "negate the result?" flag (for
  div: signs differ; for muldiv: odd count of negative operands; for mod: sign
  of the dividend).
* **modulo (either signedness):** the original `n` and the original `d` (16 bits
  each), needed for the `(n/d)·d` and the final subtract at harvest. Signed mod
  also needs `|n|` (it forms the magnitude at start).

Proposed home: a small dedicated block in the Lynx extra-zero-page / BSS area
(e.g. `__suzy_async`), **not** the general `tmp1..tmp3`/`ptr1` runtime scratch,
precisely because the overlapped C code is free to use that scratch. Cost is a
handful of bytes reserved while an async op is outstanding. Because only one op
is ever in flight (§3), a single shared block suffices — no stack, no
re-entrancy. Unsigned divide and unsigned multiply need **no** carried state
(they already use no zero-page temporaries), which is a second reason the
unsigned divide is the cleanest, cheapest async op.

Note one simplification the async divide enjoys: the synchronous divide needs no
`SPRSYS` setup at all (sign-math/accumulate affect only multiply), so
`suzy_udiv_start` writes only the math registers and the start trigger — there is
nothing to save or restore across the gap. Multiply and muldiv do touch the
`__sprsys` shadow; for those the shadow write happens at start and any restore at
harvest, which is safe as long as the overlapped code obeys the "no SPRSYS write"
contract (§3).

## 7. Safety of early/late harvest

* **Harvest too early** (before `MATHWORKING` clears): each `*_result` routine
  begins with the same `lda SPRSYS / bmi` spin the synchronous routines use, so
  a premature harvest simply blocks until done — it degrades to synchronous,
  never returns a half-computed value. `suzy_math_busy()` lets the programmer
  avoid the spin when they have real work to interleave; the defensive spin is
  the floor, not the expectation.
* **Harvest never** (start with no matching result): the unit is left in DONE
  with stale registers. Harmless until the *next* `start`, which overwrites them.
  The only hazard is the forbidden-Suzy-use contract (§3) — e.g. a TGI draw in
  the gap — which is a programmer error the grep-audit is meant to catch.
* **Two starts without a harvest:** the second start overwrites the first; the
  first result is lost. Defined but a logic bug; "one in flight" forbids it.

## 8. Verification plan

Extend `examples/muldivtest/muldivtest.c` (or a sibling `asyncmathtest.c`) with:

1. **Correctness sweep.** For every corner pair already in `muldivtest`'s table,
   run the async form (`start` → fixed dummy non-Suzy delay → `busy` poll →
   `result`) and compare against the stock software `/`, `%`, `*` and the
   long-math muldiv reference. The async result must equal the synchronous
   result bit-for-bit, across both signednesses, including the §2.6.3 narrow
   divisors (1/127/255), the 256 boundary, and the wide cases.
2. **Overlap-integrity test.** During the gap, run an independent computation
   with a known answer (e.g. a checksum loop that touches no Suzy state) and
   assert *both* the math result and the checksum are correct — proving the
   overlapped code and the in-flight unit don't disturb each other.
3. **Early-harvest test.** Call `result` immediately after `start` with no gap;
   confirm the defensive spin yields the correct value (degrades to sync).
4. **Timing demonstration.** Cycle-count the synchronous divide loop vs.
   start + N cycles of real work + result, showing the divide latency is hidden
   when the interleaved work is ≥ the divide time.
5. **Contract negative-check (manual/doc).** A commented example that violates
   the gap contract (a TGI draw between start and result) and shows the
   corruption, kept as a cautionary sample, not a passing test.

Host pre-validation as in §2.6: model the exact byte stores and the
divide/multiply latencies, confirm 0 mismatches before the emulator/hardware
run. The async path shares the operand-placement code with the synchronous
routines, so the new surface to validate is the state-carry (§6) and the
poll/harvest split, not the arithmetic itself.

## 9. Open questions / decisions deferred to implementation

* **Header home:** new `<lynx/suzymath.h>` vs. folding the prototypes into
  `<lynx/lynx.h>`. A dedicated header keeps the fork-specific surface contained and
  matches the "opt-in, auditable" spirit of the operators.
* **muldiv arg placement:** §5 recommends all-three-at-start; confirm against
  real call sites (fixed-point scaling) that the divisor is always available
  early — it normally is.
* **Reserved-state size:** finalize the `__suzy_async` block (worst case is
  signed mod: flag + `n` + `d` + `|n|` ≈ 7 bytes) and where it lives in the Lynx
  zero-page/BSS map.
* **Multiply async:** keep it (symmetry, and a caller who already has the args
  may still interleave a few instructions) or drop it to discourage misuse.
  Leaning keep-but-document.
* **Optional `suzy_math_abort()`:** a no-op helper that just marks the unit IDLE
  for static checkers; the hardware needs no abort (the next start overwrites).
```
