# Compiler correctness audit — design

Status: DESIGN ONLY — no harness or tests implemented yet.

## 1. Motivation

The ArrayRef constant-subscript bug (fixed in `compiler/cc65/expr.c`, see
`design/LYNX_MEMBER_ADDR_CAST_FIX_DESIGN.md`) shipped in upstream cc65 2.19
and survived every example program until the raycaster tripped it. It was
subtle in the characteristic way compiler bugs are: one path (constant
subscript) diverged from its sibling (variable subscript), and only a specific
source shape reached it. One such bug found implies more remain — both
inherited from 2.19 and introduced by this fork's own changes. This document
designs a systematic audit to find them.

## 2. Where the bugs live (threat model)

Ranked by prior probability of a latent miscompile:

**A. Fork-introduced changes.** `git log V2.19..HEAD` on the compiler shows
the risky surface:

- `0d92ee308` "initial optimisations" — `codegen.c` +212 lines,
  `codeseg.c` +94, `codeent.c`, `codelab.c`. Rewrote codegen paths and
  segment/label handling; largest untested delta.
- `d2908f2a6` / `ad4095a6c` — Suzy `!*` `!/` `!%` operators and the fused
  `a !* b !/ c` muldiv. New expression-tree shapes flowing through every
  downstream pass; sequencing, side effects in operands, and interaction
  with the async Suzy math library (`suzyasync.s`) are all novel territory.
- `3675bcc1d` — §2.7 cycle-cost model (see `design/LYNX_CODEGEN_DESIGN.md`)
  wired into two consumers, plus 150 new lines in `coptc02.c` and new
  opcodes. Cost-model bugs usually cause slow code, not wrong code, but the
  new 65C02 peepholes can cause both.
- `f93733c8f` — Opt65C02BitOps guarded against TRB/TSB into $FC00–$FCFF.
  The guard exists for *one* pass; every other pass that synthesises
  read-modify-write or widens accesses needs the same scrutiny.
- `503cbf87e` — `__zeropage` variables: new storage class through codegen,
  register allocation, and peepholes that pattern-match zp addressing modes.

**B. Upstream bugs fixed after 2.19.** cc65 2.19 is from 2020; upstream
master has since fixed many miscompiles in files this fork still shares
(`expr.c`, `codegen.c`, `copt*.c`, `stdexpr`, promotions). These fixes are a
free bug list: each one either applies here or provably doesn't.

**C. The 102 inherited peephole passes** (`static OptFunc` in `codeopt.c`).
Peepholes miscompile by assuming a flag, register, or stack state the
pattern doesn't actually guarantee. They are individually small and
individually auditable, but exhaustive review is poor value; differential
testing (below) exercises them far more cheaply, with review reserved for
passes that testing implicates.

## 3. Techniques

### T1. Differential optimization testing (primary weapon)

Compile the *same* self-checking program at multiple optimization settings
and compare results at runtime:

- levels: no `-O`; `-O`; `-Oi`; `-Ors` (register vars exercise a whole
  extra allocator path); optionally `--codesize` extremes
- oracle: the unoptimized build. Any divergence between levels is a bug in
  the compiler (or, rarely, UB in the test program — the generator's job is
  to prevent that)

This needs no external ground truth and directly exercises all 102+ opt
passes and the fork's codegen changes.

### T2. Random program generation ("lynxsmith")

A Python generator (`tests/compiler/gen/`) emitting well-defined C in the
cc65 subset:

- types: `unsigned char/int/long`, `signed char/int/long`, pointers,
  arrays, structs (nested, with member arrays), unions, bitfields, enums
- operations: full arithmetic/logical/shift/compare set, compound
  assignment, pre/post inc/dec, ternary, casts, `&`/`*` chains, constant
  and variable subscripts (the ArrayRef lesson: always generate *both*
  shapes of every construct that has a constant/variable split)
- fork extensions: `!*` `!/` `!%`, fused muldiv, `__zeropage` globals,
  `register` locals
- UB avoidance by construction: unsigned wrap where overflow possible,
  masked shift counts, guarded division, no out-of-bounds indexing
- every function folds its results into a running CRC; determinism required

Second oracle: the same source twin-compiled with host cc. The generator
emits explicit casts on every operation so 16-bit cc65 semantics and host
integer promotion agree (`(unsigned int)(x + y)` style). Two independent
oracles (host run, unoptimized cc65 run) disagreeing with *each other*
flags a harness/semantics bug rather than a compiler bug.

### T3. Execution harness on headless GearLynx

No sim65 in this fork, and the Suzy operators need real hardware emulation
anyway — GearLynx (`tests/emu/gearlynx`, MCP on :7777) is the executor:

- one ROM batches many test functions (~100+ random functions per cart);
  each writes its CRC into a result table at a fixed RAM address, then the
  cart writes a completion magic. Batching amortises the build+boot cost,
  which would otherwise dominate
- runner: `debug_reset`, load ROM, `step_frame` until magic or timeout,
  `read_memory` the table, compare across builds/oracles
- pure-compute cart config (no gfx/audio init) for speed and determinism

### T4. Localisation and reduction of a failure

When builds disagree:

1. binary-search the function batch to the offending function
2. bisect passes with `--disable-opt <name>` / `--enable-opt` to name the
   culprit pass (or prove it's base codegen)
3. diff the per-function `-S` output between settings
4. reduce the C source (heuristic reducer in the harness: drop statements
   while the mismatch persists; C-Reduce if available)

### T5. Targeted review of fork diffs

Checklist-driven manual review of the section-A commits, looking
specifically for the classic peephole/codegen failure modes: flag liveness
across removed instructions, stale Y/X register assumptions, `StackPtr`
tracking after resequenced pushes, label reference counts after code
deletion (`codeseg.c`/`codelab.c` changed in `0d92ee308`), hardware-range
safety ($FC00–$FDFF) for every pass that widens or synthesises accesses,
operand evaluation order and side effects around the fused muldiv, and
volatile accesses surviving every pass untouched.

### T6. Upstream fix mining

Enumerate upstream cc65 commits after the 2.19 tag touching shared files
whose messages indicate miscompile fixes; classify each as
applies / doesn't apply / already diverged. Applicable ones become directed
test cases first (prove the bug exists here), then ports.

### T7. Directed stress corpus

Hand-written cases for constructs the ArrayRef class marks as fragile, kept
in `tests/compiler/corpus/`: member-address arithmetic in all shapes,
post-increment inside complex lvalues, char promotion in shifts and
comparisons, signed `/` and `%` rounding at boundary values, long (32-bit)
arithmetic through the runtime helpers, register variables reused across
loop bodies, `volatile` reads/writes to plausible hardware addresses,
bitfield insert/extract widths 1–15 crossing byte boundaries, and deep
struct/array/pointer mixes.

## 4. Infrastructure and layout

```
tests/compiler/
    member_addr_cast.py        (existing)
    gen/                       lynxsmith generator + semantics notes
    corpus/                    directed .c cases (T7 + minimised finds)
    harness/                   batch-cart builder, GearLynx runner, reducer
```

`tests/run.sh` gains an audit stage: fixed-seed generator run (small N,
CI-friendly) + full corpus. Larger seed sweeps run on demand. Every
minimised find becomes a permanent corpus file — execution-checked, not
asm-shape-checked, so it survives legitimate codegen changes (unlike the
deliberately shape-pinned `member_addr_cast.py`).

A static asm scan rides along: after each build, grep generated code for
absolute accesses in $FC00–$FDFF that the source didn't ask for
(generalising the `f93733c8f` guard into a checked invariant).

## 5. Bug workflow

find → reduce → corpus regression test (fails before fix) → fix →
full sandbox rebuild + all examples → design note (new
`design/*_DESIGN.md` or addendum to this one) → doc sync per `CLAUDE.md`.

## 6. Phasing

1. **Harness**: batch cart + GearLynx runner + O-level differential over
   the existing examples' compute kernels and the T7 corpus.
2. **Directed corpus** (T7) + upstream fix mining (T6) — cheapest bugs
   first, and T6 finds arrive with a repro recipe attached.
3. **Generator** (T2) with host-cc twin oracle; fixed-seed CI stage.
4. **Fork-diff review** (T5), prioritised by anything phases 1–3 implicate.
5. **CI integration** and a standing nightly-scale seed sweep.

## 7. Risks

- *Emulator inaccuracy*: a GearLynx bug would masquerade as a miscompile.
  Mitigation: two-oracle rule; any confirmed find is verified by reading
  the disassembly before touching the compiler.
- *Host-oracle semantic drift* (promotion, char signedness): mitigated by
  explicit-cast generation and by treating the unoptimized cc65 build as
  the authoritative oracle when the two disagree.
- *UB in generated programs*: false positives; the generator is
  conservative by construction, and any suspected find is checked for UB
  during reduction.
- *Throughput*: builds are the bottleneck, not emulation — batching and
  compile-only differential (asm diff without execution for quick triage)
  keep iteration fast in the sandbox's 45 s command windows.
