# cc65 for the Atari Lynx

This is cc65 2.19 stripped down to a single target — the Atari Lynx — and
modified for faster 65SC02 code generation. All other machine targets,
libraries, and configs have been removed. It includes the macro assembler,
C compiler, linker, librarian, and the Lynx runtime library and drivers.

Upstream cc65 documentation: [cc65.github.io/doc](https://cc65.github.io/doc)

## Lynx code-generation improvements

See [LYNX_CODEGEN_DESIGN.md](LYNX_CODEGEN_DESIGN.md) for the full design,
measurements, and verification plan.

Implemented so far:

- **Runtime library fast paths** (§2.1): 65C02 variants of the hottest runtime
  routines (`pushax`, `ldaxsp`, `staxsp`, `staspidx`, `pusha`, `enter`),
  saving 1–4 cycles per call on every stack/pointer access.
- **Direct 65C02 emission in the compiler** (§2.3): `(zp)` indirect addressing
  for zero-index loads/stores and `STZ` for constant-zero stores, emitted up
  front in `codegen.c` (mainly benefits unoptimized builds).
- **Jump-table switches** (§2.4): dense char-typed switches dispatch via
  `JMP (table,x)` instead of a compare cascade, with new `CLF_RETAINED` label
  infrastructure. Constant-time dispatch; measured 76 vs 93 average cycles on
  a 6-case switch.
- **Self-modifying-code runtime variants** (§2.5): `memcpy`/`memset` patch
  their own operands (the Lynx executes from RAM), measured 8–12% faster for
  n ≥ 256/512.
- **Suzy hardware multiply/divide** (§2.6): the fork-specific operators `!*`,
  `!/` and `!%` route int multiply/divide/modulo through Suzy's hardware math
  unit (estimated 3–8× on those operations). The standard `*`, `/`, `%`
  operators are unchanged; only explicitly marked call sites use the hardware,
  so the "no math in IRQ handlers" contract is auditable by grep. Long
  operands fall back to software; powers of two still become shifts; the buggy
  hardware remainder register is avoided (`!%` computes `n - (n/d)*d`).
  Implemented as `tossuzy*` routines in `libsrc/lynx/` plus the parser
  combination in `expr.c` and hardware generators in `codegen.c`; verified
  against C semantics on 3144 operand pairs in a 65C02+Suzy simulator. Note:
  source using these operators is fork-specific — other compilers reject it.
- **Cycle-cost model** (§2.7): a per-instruction 65SC02 cycle table in the
  optimizer (`GetInsnCycles`/`CE_GetCycles`) alongside the size model, validated
  0/150 against an authoritative reference. It backs a "not slower" guard on the
  `trb`/`tsb` peephole and a speed-biased pass (`-Oi` / `--codesize` > 100) that
  inlines the small `incsp1`/`incsp2` stack drops when the model shows the inline
  body beats the call. Default `-O` output is unchanged.

Explored and reverted: additional 65C02 peephole passes (§2.2) — the patterns
do not occur in compiler output, zero diffs on a 48-file corpus. All seven
design steps (§2.1–§2.7) are now implemented.
