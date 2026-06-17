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
the sign fixups. §2.6.1 fused multiply-divide — done: `a !* b !/ c` is recognized
as one operation in `hie_internal` and lowered through `g_suzymuldiv` to
`tossuzy[u]muldivax` in `libsrc/lynx/suzymuldiv.s`; the 32-bit product is divided
in place (no 16-bit truncation of the intermediate, one fewer readback/reload).
Verified 0 mismatches vs a 32-bit C reference over 201k operand triples (corners +
randoms + exhaustive small), signed and unsigned. §2.7 cycle-cost model — done: a
per-opcode/per-addressing-mode 65SC02 cycle table in `opcodes.c` (`Cycles` field +
`GetInsnCycles`), a `CE_GetCycles` code-entry helper, and both consumers wired in — a
"not slower" guard on the `Opt65C02BitOps` peephole, and a speed-biased `Opt65C02StackOps`
pass (gated at `CodeSizeFactor > 100`) that inlines `jsr incsp1`/`jsr incsp2`. The model was
validated at 0 mismatches over 150 instruction/mode pairs against an authoritative 65C02
timing reference; both inlined sites in the sample corpus emit byte-for-byte the runtime
bodies and assemble clean.

## 1. Background

### 1.1 The Lynx CPU and why cycles map to bytes

The Lynx CPU is a **65SC02** core inside Mikey, clocked at 16 MHz / 4 ≈ 4 MHz. cc65 already
models this: `src/common/target.c:211` maps the lynx target to `CPU_65SC02`.

The 65SC02 is the 65C02 instruction set *without* the Rockwell bit instructions
(`RMBx/SMBx/BBRx/BBSx`). Lynx I omits them; the later Lynx II Mikey implements them, so they
are not portable across the hardware family and stay unused — see §3 for the full rationale.
Available extensions over the NMOS 6502:

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
   **Suzy-address hazard** (hardware spec ch. 3.1.2; see `LYNX_TGI_DESIGN.md` §5): one
   instruction performing two Suzy accesses breaks Suzy, so RMW opcodes — including
   `TRB/TSB` — must never target $FC00–$FCFF. The *stock* pass already had this latent
   bug for `*(volatile uint8_t*)0xFCxx |= m`. **Guard done (2026-06-12):**
   `IsSuzyHwAddr()` in `coptc02.c` skips the rewrite for constant operands in
   $FC00–$FCFF (gated on `Target == TGT_LYNX`). Verified: $FC00/$FC28/$FC92/$FCFF stay
   load/modify/store; $FBFF/$FD8B/$FE00 and symbolic RAM args still get `TRB/TSB`;
   sim65c02 unaffected. Any future extension of this pass must route its emission
   through the same guard.
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

#### 2.6.1 Fused multiply-divide (`a !* b !/ c`)

The standalone `!*` returns only the low 16 bits of the product, so `(a !* b) !/ c`
silently overflows whenever `a*b > 65535` — and it reads the product back to the CPU
only to push and reload it before the divide. But Suzy's multiply lands its full 32-bit
product in `MATHE..MATHH`, which *is* the divide's dividend register. So the parser fuses
the chain into one operation: when `hie_internal` (multiplicative level) sees a Suzy `!*`
immediately followed by a Suzy `!/`, with both factors runtime 16-bit ints, it pushes both
factors, parses the divisor, and emits `g_suzymuldiv` instead of two separate generators.
Any constant/long operand, or a following `!%`, falls through to the standard
per-operator path (still correct). This is the legitimate, *polled* form of the
multiply→divide register chaining that the "QbertRoot" hardware joke (hardware docs 12.4)
abuses by racing an unfinished multiply.

`libsrc/lynx/suzymuldiv.s` provides `tossuzyumuldivax` (unsigned) and `tossuzymuldivax`
(signed). Both load the two stacked factors, start the multiply, poll, then start the
divide by **rewriting `MATHE` with its own value** — `MATHE` already holds the product's
MSB and writing it triggers the divide without disturbing the 32-bit dividend (writes to
`MATHE` force nothing to zero). The signed entry reduces all three operands to magnitudes,
runs the unsigned core, and negates the result iff an odd number of operands were negative
($8000 negates to itself → read as the unsigned magnitude 32768, as in `tossuzydivax`).
Benefits: a full 32-bit intermediate (kills the overflow, the main win for fixed-point
scaling) plus the removed readback/reload. Constraints are identical to the other Suzy
math routines (sprite engine idle, not IRQ-safe, unsafe-access bit pollution).

Verified: 0 mismatches against a 32-bit C reference over 201,528 operand triples (corner
values 0/1/$7FFF/$8000/$FFFF, randoms, and exhaustive small cases), for both signednesses;
the old separate-op path diverged on ~199,982 of them (the overflow now fixed). Compiler
output confirmed: signed→`tossuzymuldivax`, unsigned→`tossuzyumuldivax`, a lone `!*` still
emits `tossuzymulax`, and `a !* b !/ c !/ d` fuses the first pair then chains a plain
`tossuzydivax`. Pending: on-emulator/hardware run.

#### 2.6.2 Effect on the shipped library code

Because Suzy math is opt-in only, recompiling library source changes nothing. The
standard `*`/`/`/`%` operators keep lowering to the software `tosmulax`/`tosdivax`
family, and the compiler's *implicit* multiplies — array indexing, pointer scaling,
struct strides — also stay software (§2.6, "compiler-generated multiplies always use
software"). No `.c` source in `libsrc/` silently switches to the hardware unit. The math
unit reaches library code only where it is hand-written in asm or where an internal
helper is deliberately routed over to it. The effect therefore splits into three: what
already uses it, what would change if it were pushed deeper, and the contract the whole
library must hold so neither breaks.

**What already uses it (safe by construction).** Two places in `libsrc/lynx` touch the
math unit: the `tossuzy*` routines themselves (`suzymul/div/mod/muldiv/udiv/umod.s`),
linked only when a program uses `!*`/`!/`/`!%`; and `tgi/tgi-text.s`, where
`tgi_gettextwidth` computes `strlen*8*scale >> 8` as an inline unsigned Suzy multiply and
text scaling writes 8.8 values straight into the sprite's sx/sy fields. Both are safe
because TGI draws synchronously — the CPU waits for sprite completion, so the sprite
engine (which shares the math unit) is provably idle when these run, and the math-working
bit is polled before any result is read. That synchronous-draw assumption is load-bearing
for everything below.

**What would change if it were pushed deeper.** The candidates are the internal helpers
of §2.6 point 4 — `umul16x16r32` / `udiv32by16r16` used by `lz4` and TGI scaling — which
are exact width matches for the hardware. Converting them via a vpath override would give
the documented 3–8× on those ops with no change at the call sites. For TGI scaling that is
safe for the same synchronous-draw reason. For `lz4` it requires a per-helper audit that
the routine is never entered mid-sprite or from an IRQ. The win stays concentrated and the
surface small precisely *because* the switch is not automatic; the cost is that every
converted helper inherits the constraints below and stops being grep-auditable once it is
buried inside a general-purpose routine arbitrary code can call.

**The contract the rest of the library must uphold.** The math unit is global,
non-saveable hardware state (rewriting the inputs starts a new operation — there is no
save/restore protocol), so any library use imposes invariants on the rest of the library:

- *No Suzy math in IRQ handlers.* `irq.s`, `ser/*`, `clock.s`, and `joy/*` do no
  multiply/divide today, and this must stay true: an interrupt doing hardware math mid
  mainline operation would silently corrupt the result.
- *Sprite engine idle.* Holds for TGI because drawing is synchronous. A future
  *asynchronous* sprite library combined with C `!*` arithmetic would corrupt both — the
  one architectural door that deeper adoption closes.
- *Accumulate off / `__sprsys` shadow discipline.* The Suzy routines AND-mask the
  `__sprsys` shadow (`& $3F`) to force sign-math and accumulate off while preserving the
  sprite control bits, and `CLR_UNSAFE` in the shadow resets the unsafe-access bit.
  `tgi-page.s`, `tgi-collision.s`, and `tgi-init.s` all route their SPRSYS writes through
  that same shadow, so the two coexist correctly; any new SPRSYS writer must keep using
  the shadow or it will desync.
- *Unsafe-access bit pollution.* Every math op may spuriously set the SPRSYS unsafe bit
  (hardware bug). `tgi-collision.s` reads SPRSYS for collision state, so collision or
  sprite-debug code running after hardware math must reset that bit — the interaction to
  watch if math use spreads.

**Bottom line.** For the library as it stands the effect is contained and benign: a faster
`tgi_gettextwidth`, free fractional text scaling, and operator-only routines that do not
link unless used. No C library routine changed behavior and the interrupt-driven
subsystems are clean. The cost is a library-wide contract (sprite engine idle, no math in
IRQs, SPRSYS only via the shadow) that is satisfied everywhere today but becomes harder to
guarantee the deeper the hardware unit is folded into general-purpose helpers such as
`lz4`. The conservative course — and what the design already follows — is to keep Suzy
math at explicit, auditable call sites plus the synchronous TGI paths, and to treat each
internal-helper conversion as its own reentrancy audit.

#### 2.6.3 Small-divisor normalization (shift both operands up by 8)

Suzy's divide is not constant-time: `176 + 14·N` ticks where `N` is the count of leading
zeros in the 16-bit divisor (§2.6, "Hardware capabilities"). A *small* divisor is therefore
the *slow* case — divisor `1` carries 15 leading zeros (≈386 ticks), while any divisor
≥256 carries at most 7 (≤274 ticks). Because the quotient of `(n·256) / (d·256)` equals
that of `n / d`, left-shifting *both* operands by 8 leaves the result unchanged but pushes
the divisor up into its high byte, erasing 8 leading zeros and saving `14·8 = 112` ticks
on every divide whose divisor is `< 256`. For arithmetic that repeatedly divides by small
runtime values — fixed-point scales, counts, velocities — this is a ~29% cut on the
worst-case divide for the price of one branch.

**Free for the 16/16 paths, by width.** In `suzyudiv.s`/`suzydiv.s` the dividend is at
most 16 bits, zero-extended into the 32-bit `MATHE..MATHH` group, so `n<<8` is at most 24
bits and can never overflow 32 — the shift is unconditionally safe whenever the divisor is
small. It is also nearly free: shifting `n` left by 8 just writes the same two dividend
bytes one register higher (`MATHG`/`MATHF` instead of `MATHH`/`MATHG`), and shifting the
small divisor writes its byte to `MATHN` instead of `MATHP` — identical store counts. The
only added cost is a one-byte test of the divisor's high byte (`cpx #0` / branch, ~5
cycles) selecting the normalized path, with the existing register-pair zeroing discipline
preserved (write `MATHH` before `MATHG`, `MATHF` before `MATHE`, since each of those writes
forces its pair partner to 0). `suzydiv.s` (signed) applies the test to the already-formed
`|divisor|` magnitude — `|dividend| ≤ 32768`, so `<<8` still fits 24 bits. `suzyumod.s`/`suzymod.s`
carry the same edit (they inline their own divide rather than calling the shared routine):
only the divide is normalized, while the saved original divisor drives the following
`(n/d)·d` multiply, so `MATHD/MATHC` still hold the un-shifted quotient and the remainder is
correct. Divide-by-zero is unaffected (`0<<8` is still 0), and the hardware remainder is
scaled by 256 but never read.

**Not free for fused muldiv — the "numerator too large" case.** In `suzymuldiv.s` the
dividend *is* the full 32-bit product `a·b`; that width is the routine's whole reason to
exist (it kills the `!*` overflow). Shifting `product<<8` overflows 32 bits unless the
product already fits in 24, so the trick needs a *second* guard — `MATHE == 0` (product
bits 24..31 zero) read after the multiply settles, **and** `c < 256` — before it is safe.
That is a narrower, runtime-dependent payoff, so the conservative course is to leave the
muldiv paths unnormalized (or gate strictly on `MATHE == 0`) rather than risk the overflow
the fused routine was built to prevent.

**Largest win is at compile time.** When the divisor is a compile-time constant `< 256`,
`g_div`/`g_mod` can emit the normalized operand placement directly, with *no* runtime
branch at all — the same family of move as the existing power-of-two→shift strength
reduction (§2.6, "Constant folding is kept"). That captures most of the benefit on the
common "divide by a small literal" site for zero added cost; the runtime high-byte test
covers the variable-divisor remainder.

Status: IMPLEMENTED. The narrow/wide split is in `suzyudiv.s`, `suzydiv.s`, `suzyumod.s`
and `suzymod.s` (the muldiv paths are deliberately left unnormalized, per the product-width
caveat above). `muldivtest` now sweeps plain `!/` and `!%` (signed and unsigned) alongside
the fused operator, over a corner table that includes divisors `1`/`127`/`255` (narrow),
`256` (boundary) and `$FFFF`/`30000`/`32767` (wide), comparing each against the stock
software `/`/`%`. A host model of the exact byte stores validated 0 mismatches over all 176
narrow-path corner pairs (unsigned and signed div/mod). Pending: on-emulator/hardware run.

### 2.7 Infrastructure: a cycle-cost model — IMPLEMENTED

A per-opcode, per-addressing-mode cycle table now lives in `opcodes.c` alongside size: the
`OPCDesc` struct carries a `Cycles` field (non-zero = fixed cost for single-encoding insns
such as branches, stack ops, transfers and `jmp`/`jsr`/`rts`/`rti`/`brk`; zero = derive from
the addressing mode), and `GetInsnCycles(OPC, AM)` returns the 65SC02 cost. For the
memory-operand opcodes it classifies by timing group — store, read-modify-write (the
shift/rotate and `inc`/`dec`-memory ops, plus `trb`/`tsb`), or plain read/load/ALU — using the
existing `OF_STORE`/`OF_NOIMP` flags. `CE_GetCycles(CodeEntry*)` wraps it for the optimizer.

The figure is the **guaranteed minimum**: data-dependent penalties the compiler cannot know
statically are excluded by design, so that the cost of a fixed instruction is itself fixed.
Specifically a *taken* conditional branch costs +1 and a page-crossing indexed/indirect-indexed
access costs +1; neither is modelled. Always-paid transfers (`bra`, `jmp`, `jsr`, and the long
conditional `j*` pseudo-insns, costed as inverse-branch + `jmp` = 5) are costed at their real
value. The model was validated at **0 mismatches over 150 instruction/mode pairs** against an
authoritative 65C02 timing reference.

Both consumers are wired in:

1. **"Not slower" guard.** `Opt65C02BitOps` (which rewrites `lda/and|ora #imm/sta` into
   `lda #imm` + `trb`/`tsb`) now compares `CE_GetCycles` of the matched triple against the
   replacement and only fires when the replacement is no slower, instead of assuming it.
2. **Speed-biased pass.** `Opt65C02StackOps` (new, registered with a `CodeSizeFactor` of 101
   so it runs only when the segment factor exceeds 100 — i.e. `-Oi` / `--codesize >100`)
   inlines `jsr incsp1` and `jsr incsp2`, the tiny C-stack-drop leaf routines whose `jsr`+`rts`
   overhead (12 cycles) dwarfs their body. It emits byte-for-byte the bodies of
   `libsrc/runtime/incsp1.s` / `incsp2.s` (so correctness follows from the shipping runtime),
   and only when the cycle model confirms the inline body beats the call: incsp1 19→7 cyc
   (+3 bytes), incsp2 26→17 cyc (+11 bytes). Such bare drops are rare in cc65 output (most
   cleanup goes through `popax`/`addysp`/callee-cleanup), exactly the size/speed divergence the
   design anticipated; the two sites in the sample corpus were inlined correctly and the rest
   of each file is byte-identical bar label renumbering.

This is the only proposal that touches shared compiler infrastructure; everything else is
additive and CPU-gated. (The `Cycles` field and `GetInsnCycles` are target-neutral; only the
`Opt65C02StackOps` consumer is 65C02-gated, since it emits `bra`.)

## 3. What deliberately stays out of scope

**No `RMB/SMB/BBR/BBS` (Rockwell bit instructions).** These are not portable across the
Lynx hardware family: the original Lynx (Lynx I) carries a **65SC02** core that omits them —
the opcodes decode as NOP-like no-ops, so any code relying on them silently does the wrong
thing — whereas the later **Lynx II** Mikey revision implements the full Rockwell bit set and
executes them as intended. Supporting them would therefore force one of two bad outcomes:
either drop Lynx I compatibility entirely, or carry **two execution paths** — runtime CPU
detection plus duplicated runtime routines and peepholes, each with its own `.if`-gated
variant to assemble, test, and keep in sync (exactly the kind of dual-path burden
`CLAUDE.md` warns against, since every such symbol then needs parallel documentation too).

That cost is not justified by the payoff. The Rockwell ops only help a thin slice of the
instruction mix — single-bit set/clear (`SMBx`/`RMBx`) and test-and-branch (`BBSx`/`BBRx`)
on **zero-page** bytes — and most cc65 C variables do not live in zero page. Where the
pattern does occur on the shared 65SC02 baseline, the available `TRB`/`TSB`, `STZ` and `BIT`
instructions (already exploited by `Opt65C02BitOps`/`Opt65C02Stores`, §1.2) capture the
realistic portion of the benefit. The only thing they cannot replicate is the fused
test-and-branch of `BBRx/BBSx`, which saves ~2–3 cycles and 3 bytes per site over
`LDA/AND/branch` — a sub-1–2% whole-program effect even in bit-flag-heavy code, dwarfed by
the runtime-call and software-stack costs the other sections target. A single 65SC02
baseline that runs unmodified on both Lynx I and Lynx II is worth far more than that margin.

No use of `WAI/STP` (WDC-only). No changes to the
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
| Speed-biased stack inline (§2.7) | bare `incsp1`/`incsp2` drops at `-Oi` | 12/9 cyc per site (rare; 2 sites in corpus), +14 bytes total |

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
for `(zp)` and `STZ` — done, (4) jump-table switches — done, (5) cycle model — done
(`opcodes.c` table + `GetInsnCycles`/`CE_GetCycles`, the `Opt65C02BitOps` not-slower guard
and the `Opt65C02StackOps` speed-biased inline pass; model validated 0/150, both consumers
host-tested), (6) SMC runtime variants — done, (7) Suzy hardware multiply/divide (§2.6) —
done (parser + codegen + five lynx.lib routines; simulator-verified). All seven steps are
now implemented.

Cycle-model verification used the existing tooling: a standalone harness links the real
`opcodes.o` and checks `GetInsnCycles` for every legal instruction/mode pair against a
hand-encoded authoritative reference (0/150); the speed pass was A/B'd with
`--disable-opt Opt65C02StackOps` to isolate its effect, the transformed `.s` re-assembled
clean with `ca65`, and a label-canonicalised diff confirmed only the intended sites change.
On-emulator (Handy/Mednafen) cycle-count confirmation of the inlined drops remains pending,
as for the other runtime changes.
