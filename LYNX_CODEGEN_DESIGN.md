# Design: Faster 65C02 Code Generation for the Atari Lynx

Based on analysis of this tree (cc65 2.19, Lynx-only).

**Implementation status:** §2.1 runtime fast paths — done (measured 1–4 cycles per call).
§2.2 peepholes — implemented, then reverted by request: the `Opt65C02SaveRegs` pattern
does not occur in compiler output (cc65 emits the mirror shape `pha/txa/…/tax/pla`), and
the group-loop ordering fix produced zero diffs on a 48-file corpus. §2.3 direct codegen —
done (benefits unoptimized builds; at `-O` the peepholes already produced these forms).
§2.4 jump-table switches — done, including the `CLF_RETAINED` label infrastructure
(measured 76 vs 93 avg cycles on a 6-case switch; constant-time dispatch). §2.5 SMC
runtime — done for `memcpy`/`memset` (8–12% for n ≥ 256/512); `mul`/shifts were examined
and have no SMC opportunity (pure register/zp loops), see §2.6 for the better approach.
§2.6 Suzy hardware multiply/divide — done: `!*`/`!/`/`!%` operators (parser combination in
`hie_internal`, hardware generators `g_suzymul`/`g_suzydiv`/`g_suzymod`, five `tossuzy*`
routines in `libsrc/lynx/`); verified against C semantics on 3144 operand pairs (corner
values plus randoms) in a 65C02+Suzy simulator, including the divide-by-zero contract and
the sign fixups. §2.7 cycle-cost model — not implemented.

## 1. Background

### 1.1 The Lynx CPU and why cycles map to bytes

The Lynx CPU is a **65SC02** core inside Mikey, clocked at 16 MHz / 4 ≈ 4 MHz. cc65 already
models this: `src/common/target.c:211` maps the lynx target to `CPU_65SC02`.

The 65SC02 is the 65C02 instruction set *without* the Rockwell bit instructions
(`RMBx/SMBx/BBRx/BBSx`). Available extensions over the NMOS 6502:

| Instruction | Effect | Saving vs 6502 idiom |
|---|---|---|
| `STZ zp/abs/zp,x/abs,x` | store zero | replaces `LDA #0 + STA` (2 bytes, 2 cycles, keeps A) |
| `BRA rel` | unconditional branch | 1 byte vs `JMP`; relocatable |
| `PHX/PHY/PLX/PLY` | push/pull X,Y | replaces `TXA/PHA` pairs; keeps A intact |
| `(zp)` | indirect without Y | replaces `LDY #0 + (zp),y` (saves the LDY and frees Y) |
| `INC A / DEC A` | inc/dec accumulator | replaces `CLC/ADC #1` (1 byte, 2 cycles, no carry clobber) |
| `TRB/TSB zp/abs` | test-and-reset/set bits | replaces load/AND-OR/store read-modify-write |
| `BIT #imm, BIT zp,x / abs,x` | test without clobbering A | avoids save/restore around mask tests |
| `JMP (abs,X)` | indexed indirect jump | enables compact jump tables |

A Lynx-specific point that shapes priorities: Mikey's DRAM controller uses page-mode access,
so every instruction byte fetched costs bus time, and Suzy/video DMA steal cycles on top.
On this machine **smaller code is almost always faster code** — size and speed optimizations
are aligned rather than in tension. The second Lynx-specific point: the whole address space
is RAM (carts load to RAM per `lynx.cfg`), so self-modifying code is a legitimate technique.

### 1.2 What cc65 already does for the 65SC02

The current support is real but thin:

**Compiler codegen** (`src/cc65/codegen.c`) checks `CPU_ISET_65SC02` at exactly 3 sites,
all in `g_inc`/`g_dec`: `INA`/`DEA` for char ±1/±2 and a short int-increment sequence.
Every other `g_*` routine (≈90 of them) emits plain 6502.

**Peephole optimizer** (`src/cc65/coptc02.c`) has 3 passes, registered in
`codeopt.c:1400-1410`:
- `Opt65C02Ind` — rewrites `(zp),y` → `(zp)` when register tracking proves Y==0.
- `Opt65C02BitOps` — `LDA mem / AND|ORA #imm / STA mem` → `LDA #imm / TRB|TSB mem` (only when A is dead afterwards and both addressing modes match).
- `Opt65C02Stores` — `STA/STX/STY` → `STZ` when the register is provably zero.

**Branch shortening** (`src/cc65/coptind.c:2181-2188`) converts short-distance `JMP` → `BRA`.

**Runtime library**: only 34 of 200 files in `libsrc/runtime/` contain
`.if (.cpu .bitand ::CPU_ISET_65SC02)` fast paths. The hottest entry points are 6502-only:
`pushax` (the single most-called runtime routine, 44 cycles), `ldaxsp`, `staxsp`, `staspidx`,
`popptr1`, `ldauisp`, `pusha`, `enter`, `mul`, `shr`, `asr`, `lshl`, `lshr`.

**Switch statements** (`codegen.c g_switch`, line 4305): always a `CMP #imm / JEQ` cascade —
no jump tables, so an N-case switch costs O(N) compares at runtime.

**No cycle model**: `opcodes.c` records instruction *sizes* only. Optimizer decisions that
trade size for speed have no cycle data to consult.

## 2. Proposed changes, by expected payoff

### 2.1 Runtime library fast paths (highest payoff, lowest risk)

Compiled C on cc65 spends much of its time in `libsrc/runtime/`. These routines run on every
stack access, so cycle savings here multiply across the whole program. Add
`.if (.cpu .bitand ::CPU_ISET_65SC02)` variants — the build already compiles the library
per-target with the right CPU, and 34 files prove the pattern. No compiler changes needed.

Priority list (current cost → estimated new cost):

1. **`pushax`** (44 cycles): after `sp` is decremented, store the low byte through `(sp)`
   instead of `DEY / STA (sp),y`. Saves the `DEY` and one indexed penalty: ≈ 40 cycles.
   Combined with replacing `PHA … PLA` with direct ordering, ≈ 38.
2. **`ldaxsp` / `ldeaxysp`**: final low-byte load via `LDA (sp)` instead of `DEY/LDA (sp),y`.
3. **`staxsp`**: same `(sp)` trick for byte 0.
4. **`staspidx`**: replace `STY tmp1 / … / LDY tmp1` with `PHY / … / PLY` — saves a zero-page
   variable and is interrupt-safe; cycle-neutral (6 vs 7) but removes the `tmp1` dependency,
   enabling later reordering. Evaluate per-routine; PH/PL is not automatically faster.
5. **`popptr1`, `ldauisp`, `popa`-family**: `(sp)` for offset-0 accesses throughout.
6. **`mul`, `shr/asr`, `lshl/lshr`**: not 65C02-specific, but Lynx-relevant — see SMC in §2.5.

Verification is easy: the library builds with `make -C libsrc`, and each routine has a fixed
contract documented in its header comment (several, like `pushax`, note that the optimizer
relies on the exit state of Y — **any variant must preserve those contracts**, or the
compiler's register tracking in `codeinfo.c` must be updated to match).

### 2.2 New and extended peephole passes (`coptc02.c`)

1. **`Opt65C02SaveRegs`** (new): collapse `TXA/PHA … PLA/TAX` → `PHX … PLX` and the Y
   equivalents. Today these sequences appear around `codegen.c:2089-2110` (`g_save`/
   `g_restore`) and at ~10 other emission sites. A peephole catches all of them without
   touching codegen.
2. **`Opt65C02BitOps` extension**: currently requires the `LDA`/`STA` addressing mode and
   argument to match exactly and A to be dead. Extend to accept `INC mem`-adjacent forms and
   `LDX/STX` shapes; also emit `TRB/TSB` when the *value* (not the mask) is the loop
   invariant.
3. **`Opt65C02Ind` ordering fix**: the pass rewrites `(zp),y` → `(zp)` but leaves the now-dead
   `LDY #0` for `OptUnusedLoads` to collect. Confirm pass scheduling in `codeopt.c:1404-1410`
   guarantees the cleanup runs afterwards in the same group (currently the C02 group runs
   once, late); if not, re-run the dead-load pass inside the group.
4. **`Opt65C02BitImm`** (new): use `BIT #imm` to test bits of A without destroying it,
   replacing `PHA / AND #imm / BEQ … / PLA` shapes (common in flag tests).
5. **`Opt65C02Stz` extension**: recognize *chains* — `LDA #0 / STA a / STA b / STA c` →
   `STZ a / STZ b / STZ c` even when A is reused later only as zero (register tracking
   already knows `RegA==0`; the current pass handles this — verify it fires when the `LDA #0`
   itself then becomes dead, and add the removal).

All new passes must populate correct use/chg register info — `opcodes.c` already defines
`STZ/TRB/TSB/PHX/PLX/PHY/PLY/BRA` and `codeent.c` handles `AM65_ZP_IND`
(lines 196, 1458), so the infrastructure exists.

### 2.3 Direct 65C02 emission in `codegen.c`

Peepholes only fix what they can see. Some sequences are better emitted correctly up front:

1. **`g_getind` / `g_putind` with constant zero index**: emit `LDA (ptr)` / `STA (ptr)`
   directly instead of `LDY #0 / LDA (ptr),y`. Removes dependence on the peephole proving
   Y==0 across basic-block boundaries (where tracking gives up).
2. **`g_putstatic` of constant 0** (char/int): emit `STZ label` (+ `STZ label+1`) instead of
   loading A/X with zero, when A/X values aren't needed afterwards (flag-driven: codegen
   knows when the value is dead because assignment-as-expression is rare; conservatively,
   only when `CF_NOKEEP`-style context applies — needs a small flag addition).
3. **`g_inc`/`g_dec` threshold**: `INA` is used for val ≤ 2; raise to ≤ 3 when optimizing
   for speed (3×INA = 3 bytes/6 cycles vs CLC/ADC = 4 bytes/6 cycles — smaller, same speed,
   carry untouched).
4. **`g_save`/`g_restore`/`g_swap`**: emit `PHX/PHY` forms directly under
   `CPU_ISET_65SC02`.

### 2.4 Jump-table switches (`swstmt.c` + `g_switch` + new runtime routine)

For a dense char-typed switch with N ≥ ~6 cases, replace the compare cascade with:

```asm
        ; A = selector, table-driven dispatch
        sec
        sbc     #<minval        ; bias to 0
        cmp     #N
        bcs     default
        asl     a               ; 2-byte entries; selector < 128 guaranteed by N check
        tax
        jmp     (caseTable,x)   ; 65C02 indexed indirect jump — 6 cycles total dispatch
```

Cost is constant ~16 cycles regardless of N, versus up to 5N for the cascade. The case-node
infrastructure (`casenode.c`) already collects sorted values, so density analysis
(`(max-min+1) / N ≤ 2`, say) is a local change in `SwitchStmt`/`g_switch`. Tables go in
`RODATA` (which on the Lynx is RAM, no penalty). This is the largest single win for
interpreter/dispatch-style code. Word-sized switches keep the cascade.

### 2.5 Lynx-specific: self-modifying code in the runtime

Because the Lynx executes from RAM, runtime routines may patch their own operands —
the tree already ships `asminc/smc.inc` with macros for exactly this. Candidates:

1. **`mul` / shift loops**: patch shift counts or use SMC operand patching to remove
   per-iteration indexing.
2. **Pointer-heavy helpers**: patch an absolute address into a `LDA abs,y` inside the loop
   instead of `(ptr),y` (5 cycles → 4, and frees the zero-page pointer).

Guard with a Lynx-only condition (`.if .defined(__LYNX__)` style, via target symbol) rather
than CPU, since SMC validity is a memory-map property, not a CPU property. Interrupt safety:
routines that patch themselves must either be non-reentrant by contract (document) or patch
only in prologue.

### 2.6 Lynx-specific: Suzy hardware multiply and divide

**Requirement:** route the integer multiply and divide runtime routines through Suzy's
hardware math unit instead of software shift-add/shift-subtract loops. This is the largest
remaining single win for arithmetic-heavy code: implementing §2.5 showed that `mul.s` and
the divide routines are pure register/zero-page loops with no SMC opportunity — the loop
itself is the cost, and only hardware removes it.

**Hardware capabilities.** Suzy provides a 16×16→32 multiply (operands in `MATHA/MATHB`
and `MATHC/MATHD`, result in `MATHE..MATHH`; writing `MATHA` starts the operation) and a
32÷16 divide with remainder (dividend in `MATHE..MATHH`, divisor in `MATHN/MATHP`,
quotient in `MATHA..MATHD`; writing `MATHE` starts it). A multiply completes in roughly
44–54 ticks (≈ 11–14 CPU cycles) and a divide in at most ~400 ticks (≈ 100 CPU cycles);
completion is polled via the math-working bit in `SPRSYS` (per-operation status:
`MULTSTAT`/`DIVSTAT`); divide timing is 176 + 14·N ticks where N is the count of leading
zeros in the divisor. The register definitions are already in this tree's
`_suzy.h`/`lynx.inc`. Reference: Lynx hardware docs §12.1
(http://www.monlynx.de/lynx/lynx9.html).

**Selection model: explicit opt-in operators, software by default.** The standard `*`,
`/`, `%` operators keep the stock software routines unchanged. Suzy math is requested
per call site with the fork-specific operators `!*`, `!/`, `!%` (same precedence,
associativity, and type/promotion rules as their standard counterparts). This is
unambiguous to parse: in valid C, `!` can never appear in binary-operator position, so a
`!` where a multiplicative operator is expected — including inside `a * !*p`, where the
`!` is in unary position — collides with no legal program.

Implementation:

- *Scanner*: unchanged. `!*` must NOT be lexed as one token (it would break `!*p`);
  the parser combines the two tokens by context.
- *Parser* (`src/cc65/expr.c`): in the multiplicative-level `hie_internal` loop, if
  `CurTok == TOK_BOOL_NOT` and `NextTok` is `TOK_STAR`/`TOK_DIV`/`TOK_MOD`, consume both
  and select the hardware generator.
- *Codegen* (`src/cc65/codegen.c`): hardware variants of `g_mul`/`g_div`/`g_mod` emitting
  calls to new runtime entries (below). Constant folding is kept, as is the
  power-of-two→shift strength reduction (shifts beat the hardware).
- *Library*: the Suzy routines live under NEW entry names (`tossuzymulax`,
  `tossuzyudivax`, `tossuzyumodax`, `tossuzydivax`, `tossuzymodax`) in `libsrc/lynx/`,
  coexisting with the untouched software `tosmulax` family in `lynx.lib`. No vpath
  override, no runtime flag, no per-call dispatch overhead.

Consequences to document: source using `!*`/`!/`/`!%` is fork-specific (other compilers
reject it), and compiler-generated multiplies (array indexing, pointer scaling) always
use software — only explicit call sites get Suzy. The §2.6 constraints below (contention,
IRQ, polling) therefore only apply at sites the programmer explicitly marked, which makes
the "no math in IRQ handlers" contract auditable by grep.

**Mapping to cc65 runtime entry points** (the new `tossuzy*` entries in `libsrc/lynx/`;
the stock software routines in `libsrc/runtime/` remain the `*`/`/`/`%` implementations):

1. `tossuzyumulax`/`tossuzymulax` (`suzymul.s`): one unsigned hardware multiply serves both —
   cc65's int multiply returns only the low 16 bits of the product, which are identical
   for signed and unsigned operands. No sign handling needed at all. Estimated ~50 cycles
   total including register setup/readback, versus ~150–400 for the software loop: 3–8×.
2. `tossuzyudivax`/`tossuzyumodax` (`suzyudiv.s`/`suzyumod.s`): zero-extend the 16-bit dividend
   into `MATHE..MATHH`, divide, read quotient. **Do not read the hardware remainder**: the
   Lynx hardware documentation (§12.1.4, "Bugs in MathLand") states the remainder register
   has two value-dependent errors — "just don't use it." Modulo must instead be computed
   as `n − (n/d)·d`: one hardware divide, one hardware multiply, one 16-bit subtract.
   Still ≈3–4× faster than the software loop, but roughly twice the cost of the quotient
   path (~44 extra ticks plus register writes).
3. `tossuzydivax`/`tossuzymodax` (`suzydiv.s`/`suzymod.s`): signed division differs from unsigned — negate operands to
   positive in software, divide unsigned in hardware, fix up the result sign (C truncation
   semantics). Software fixup is **mandatory**, not a choice: Suzy's divide is unsigned
   only — its sign-math mode applies solely to multiply (and is buggy there: $8000 is
   treated as positive and 0 as negative, and signed mode destroys input operands
   in-place).
4. `umul16x16r32`/`udiv32by16r16` (used by `lz4`, `tgi` scaling and others): natural fits,
   the hardware is exactly these widths. These are library-internal helpers, not reached
   by the `!*` operators — switching them to Suzy is a separate per-helper decision (via
   the normal vpath override) and is safe for `tgi` since its drawing is synchronous.

**Constraints that the implementation must respect:**

- **Sprite-engine contention**: Suzy's sprite engine uses the same math unit during scaled
  sprite rendering. The routines must only run while the sprite engine is idle. Since
  cc65's TGI driver draws synchronously (the CPU waits for sprite completion), this holds
  by construction, but the limitation must be documented in the routine headers — custom
  asynchronous sprite code combined with C arithmetic would corrupt results.
- **Not interrupt-safe**: the math registers are global hardware state. An IRQ handler
  performing multiplication/division mid-operation corrupts the result. Same contract as
  the rest of the zero-page runtime; document it.
- **Poll before read**: results must not be read until the `SPRSYS` math-working bit
  clears. The multiply is short enough that the poll usually succeeds on first check.
- **Accumulator mode off**: `SPRSYS` accumulate must be cleared, or results accumulate
  into `MATHJ..MATHM` across calls. Side benefit: unsigned non-accumulate multiply is the
  fastest mode (44 ticks vs 54 with sign/accumulate).
- **Hardware non-reentrancy beyond IRQs**: per the hardware docs, even a careful IRQ-side
  save/restore of the math registers fails — writing the inputs back *starts a new
  operation* and disturbs values a repetitive-multiply caller may still need. There is no
  safe save/restore protocol; the only contract is "no math in IRQ handlers."
- **Unsafe-access bit pollution**: the `SPRSYS` unsafe-access bit is broken for math
  operations (documented hardware bug) — every math op may set it spuriously. Code using
  that bit for sprite debugging must reset it after math; document this in the routine
  headers (or clear it in the routines if the write is cheap enough).
- **Divide-by-zero**: hardware returns $FFFFFFFF and sets a flag bit. C behavior is
  undefined, so returning $FFFF is acceptable; document it.

**Verification**: differential simulator tests (as used for §2.1/§2.4/§2.5) need a Suzy
math model added to the simulator, or hardware/emulator validation via Handy/Mednafen
with an exhaustive-corner test ROM (0, 1, $7FFF, $8000, $FFFF operands; division by the
same plus the divide-by-zero behavior, which on Suzy returns $FFFFFFFF and must match
what callers tolerate today).

### 2.7 Infrastructure: a cycle-cost model

Add a per-opcode, per-addressing-mode cycle table to `opcodes.c` alongside size, plus a
`CE_GetCycles()` helper. Two consumers:

1. New/extended peepholes can require "not slower" instead of guessing.
2. A speed-biased pass group activated when `CodeSizeFactor > 100` chooses faster variants
   where size/speed genuinely diverge (rare on Lynx, but e.g. inlined `incsp2` vs `jsr`).

This is the only proposal that touches shared compiler infrastructure; everything else is
additive and CPU-gated.

## 3. What deliberately stays out of scope

No `RMB/SMB/BBR/BBS` — the 65SC02 lacks them; emitting them would crash on hardware even
though `--cpu 65c02` would assemble. No use of `WAI/STP` (WDC-only). No changes to the
calling convention or zero-page layout — `extzp.s` already allocates Lynx zero page, and
ABI changes would break every existing object file and driver.

## 4. Expected impact (estimates)

| Change | Code affected | Estimated speedup |
|---|---|---|
| Runtime fast paths (§2.1) | every stack/pointer access | 5–12% on typical C |
| Jump-table switches (§2.4) | dispatch-heavy code | up to 3–4× on the switch itself |
| New peepholes (§2.2) | general | 1–4% (measured: ~0, see status note) |
| Direct codegen (§2.3) | general | 1–3% on unoptimized builds |
| SMC runtime (§2.5) | block copy/fill loops | measured 8–12% on memcpy/memset n ≥ 256 |
| Suzy hardware math (§2.6) | int multiply/divide/modulo | 3–8× on those operations |

## 5. Verification plan

1. **Correctness**: rebuild `lynx.lib`; assemble-and-compare — for each changed runtime
   routine, a sim65-style unit harness is gone from this tree, so use Handy/Mednafen emulator
   test ROMs built from `samples/lynxdemo.c` plus targeted test programs per routine.
2. **Register-contract audit**: any routine whose header documents exit register state
   (e.g. `pushax`'s Y contract) gets its contract re-validated against `codeinfo.c` tables.
3. **Performance**: cycle-count micro-benchmarks in an emulator with cycle counting
   (Mednafen debugger), before/after per change; compare `.map` sizes as a proxy for fetch
   cost.
4. **Regression**: compile the library and sample at `-O`, `-Oi`, `-Or`, `-Os` and diff
   generated `.s` output between baseline and patched compiler to confirm changes are
   intentional.

## 6. Suggested implementation order

Each step is independently shippable: (1) runtime fast paths — done, (2) `Opt65C02SaveRegs`
+ `Opt65C02Ind` ordering fix — done, reverted (no measurable effect), (3) direct codegen
for `(zp)` and `STZ` — done, (4) jump-table switches — done, (5) cycle model — open,
(6) SMC runtime variants — done, (7) Suzy hardware multiply/divide (§2.6) — done
(parser + codegen + five lynx.lib routines; simulator-verified). Remaining: (5) the
cycle-cost model (§2.7).
