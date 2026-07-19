# Upstream fix mining — classification (T6)

Part of the compiler correctness audit
(`design/LYNX_COMPILER_AUDIT_DESIGN.md` sec. T6). The candidate list is
produced by `mine.py` (see its header for the clone recipe); this file is
the human-curated classification. Regenerated 2026-07-19 against upstream
`cc65/cc65` master (`547d9235`): **286** post-V2.19 fix-keyword commits
touch files this fork still shares, **167** of them in codegen/optimizer
files where wrong-code bugs live.

Statuses:

- **CONFIRMED** — reproduced live in this fork by the audit harness;
  execution-checked repro exists in the corpus.
- **applies?** — the buggy code plausibly exists here; not yet probed with
  a directed test. These are the next candidates for corpus work.
- **covered** — a directed corpus shape probes it and passes (either the
  fork never had it, this fork's own changes fixed it, or our shape
  misses it — the corpus file notes which shape ran).
- **n/a** — parser/diagnostic/build-system only; cannot miscompile.

## Confirmed and FIXED in this fork

| Upstream commit | What | Repro | Status |
|---|---|---|---|
| ~`85e73e91` chain (2020-07) | signed `/ 2^k` strength-reduced to arithmetic shift in `g_div` — floor, not C truncation toward zero; `-7/2 == -4`, `-300/256 == -2`. | `corpus/sdiv_pow2.c` | **FIXED 2026-07-19** — `g_div` adds `(2^k)-1` to negative 8/16-bit dividends before the shift; signed 32-bit uses the runtime divide. See `doc/compilerbugs.html`. |
| `d628772cd1` (2021-02) | signed char compared with unsigned numeric constants gives wrong results | `corpus/scharcmp_uconst.c` | **FIXED 2026-07-19** — direct port of the upstream fix into `hie_compare`: the char-vs-constant fast path now requires a signed constant; mixed signedness compares full-width unsigned. |
| `c8956ce19b` (2022-03) | signed long compared with smaller unsigned types gives wrong results (`-2L < 40000u` false) | `corpus/slongcmp_mixed.c` | **FIXED 2026-07-19** — fork equivalent of the upstream fix: `g_typeadjust` now applies the usual arithmetic conversions (a smaller unsigned operand converts to plain signed long; only an unsigned long makes the operation unsigned), and `hie_compare`'s constant fold and unsigned strength reduction follow the converted common type. Also corrects signed-long `/`/`%` with smaller unsigned operands. |
| `1450f146a5` (2021-05) | `[]`/`()`/`.`/`->` after a postfix `++`/`--` — this fork REJECTED `p++[0]` with a parse error (accepts-valid bug, not a miscompile) | `corpus/upstream_shapes.c` (`p++[0]` shape) | **FIXED 2026-07-19** — upstream port: postfix `++`/`--` moved into `hie11`'s postfix-operator loop. |

## Confirmed live in this fork (repros in `corpus/known/`)

None currently open. When a new find is confirmed it gets a `known-bug`
repro here, fixing it follows the sec. 5 workflow, and when the repro
stops diverging the harness flags its `known-bug` marker as stale.

## Probed and passing (`corpus/upstream_shapes.c` and friends)

| Upstream commit | What | Result |
|---|---|---|
| `29154646` (2020-11) | char-type bit-shift codegen | covered — uncast char shifts 0..7 both signs pass |
| `b2c1a77bb3` (2020-12) | 16-bit compares when high bytes known equal | covered |
| `bd8eae67f1` (2021-04) | local struct field access via address of struct | covered (this fork's ArrayRef fix, `design/LYNX_MEMBER_ADDR_CAST_FIX_DESIGN.md`, covers the shapes tried) |
| `eadaf2fef8` (2021-02) | deferred post-inc/dec in unevaluated context (`sizeof (i++)`) | covered |
| `6e61093e79` / `f1c715c455` (2021) | pointer subtraction sign/rare cases | covered |

## High-priority candidates not yet probed (applies?)

Ordered by how directly the fix touches wrong-code paths this fork
exercises. Each should get a directed corpus shape (fails-before-fix if it
reproduces), then a port decision.

| Upstream commit | What | Files |
|---|---|---|
| `4d5fe38540` (2020-09) | OptStackOps when the pushed TOS is accessed before the op | coptstop.c |
| `bf5d8c44e4` (2026-03) | OptStackOps compares values from the wrong instruction when a runtime call has an asm label | coptstop.c |
| `d711b6d6fa` (2026-03) | Lhs load insns shared with Rhs removed by Rhs optimizers (#2947) | coptstop.c |
| `c9b885b1` (2026-03) | ldaxysp removal as "load" for only A or X side (#2942/#2461) | codeinfo/copt |
| `d996e20c` (2024-09) | #2461 "always wrong even where it seemed to work" | copt* |
| `f43cfd1a` (2024-09) | CPU-flag-use check after a removed instruction only looked one insn ahead | codeinfo.c |
| `5ef420af5a` (2022-03) | OptCmp1 with certain label patterns | coptcmp.c |
| `78263cd24b` / `4e411e8526` (2023-02) | OptStackOps label migration on replaced ops (toscmpbool) | coptstop.c |
| `96d55e3703` (2024-02) | char-size bitwise XOR/OR/AND with complicated rhs | expr/codegen |
| `55ae350fed` (2021-02) | `Opt_staxspidx` invariant | coptstop.c |
| `6974c1ff12` (2021-03) | arithmetic-conversion codegen in addition/subtraction | expr.c |
| `23aa562094` (2024-02) | subtraction evaluation of identifiers at different memory locations | expr.c |
| `2915464667`-adjacent `e0c12c90` (2020-11) | g_asr/g_asl ROL/ROR char shifts ≥ 6 | codegen.c |
| `f1ed5b7057` (2025-04) | typo in `g_typeadjust` (#2611) | codegen.c |
| `8111946731` (2023-10) | array subscript with a bit-field index | expr.c |
| `2c3ca15d90` (2022-11) | marking unevaluated subexpressions of `?:` | expr.c |
| `fb6bc275bc` (2020-08) | evaluation-flag propagation to subexpressions in assignments/calls | assignment.c |
| `b2c1a77bb3` follow-ups: `de630a1245`, `a7d6eb9190`, `810e17edfe` (2020-09) | processor-flag tracking for non-JSR/RTS entries, BIT/TRB/TSB state tracking | codeinfo.c |

Caveat for all `copt*` entries: this fork's `0d92ee308` ("initial
optimisations") rewrote parts of these files, so each fix must be checked
against the FORK's code, not 2.19's — some may be inapplicable or need
adaptation (classify as "already diverged" where so).

## Not applicable (examples)

Parser/diagnostic/preprocessor-only fixes (wrong error messages, `#pragma
charmap` timing, `_Pragma` comments, wide-char constants, `#include`
macro expansion, warning switches...) and fixes to files the fork deleted
(`--standard` handling, target/CPU option code) — listed by `mine.py` but
they cannot produce wrong code on a running cart. Individual entries are
not enumerated here; re-run `mine.py` for the full table.
