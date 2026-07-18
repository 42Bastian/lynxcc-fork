# lynxsmith — semantics notes

Rules that keep a generated program (a) free of undefined behaviour and
(b) bit-identical between the two oracles — 16-bit-int cc65 on the Lynx
and a 32-bit-int host compiler — per
`design/LYNX_COMPILER_AUDIT_DESIGN.md` sec. T2.

**Every operation is explicitly cast.** The generator never relies on the
usual arithmetic conversions, because those differ between a 16-bit and a
32-bit `int`. Each binary operation is emitted as
`(T)((U)(a) op (U)(b))`: the operands are converted to an unsigned
*compute domain* `U` (`u16` for 8/16-bit results, `u32` for 32-bit), the
operation happens there, and the result is cast to the target type `T`.
Unsigned arithmetic wraps identically everywhere, so overflow is defined
and equal on both oracles.

**Signed values are bit-pattern casts of unsigned results.** Converting
an out-of-range unsigned value to a signed type is implementation-defined
in C, not undefined; both cc65 and every relevant host compiler (gcc,
clang, with `-fwrapv` for arithmetic) implement two's-complement
truncation. The host build pins this with `-fwrapv`; the unoptimized cc65
build remains the authoritative oracle if a host ever disagrees
(design sec. 7).

**Signed right shift is arithmetic on both oracles** (implementation-
defined, consistently two's-complement arithmetic shift); the corpus uses
it deliberately, the generator only shifts in the unsigned domain.

**Shift counts are masked** to `width-1` before every shift.

**Division and modulo divisors are forced nonzero** by emitting
`((U)(b) | 1u)`; generated `/ %` stay in the unsigned domain (the
signed-division edge cases are directed-corpus territory —
`corpus/sdiv_bounds.c` and `corpus/known/sdiv_pow2.c`).

**Array indexes are masked to the array bounds** (sizes are powers of
two) at every subscript, and the generator emits BOTH constant and
variable subscripts of every array it declares — the ArrayRef lesson:
each construct with a constant/variable split must be generated in both
shapes.

**Everything is initialized at declaration**; there are no reads of
indeterminate values. Loops have fixed small trip counts, so termination
and determinism are structural.

**Struct member offsets are never observed.** cc65 packs structs, hosts
pad them; generated code accesses members only by name, never through
byte-offset tricks across members (those live in the corpus, written
layout-agnostically).

**Fork-extension mode** (`--suzy`) adds the Suzy `!*` `!/` `!%`
operators and fused `a !* b !/ c` chains on 16-bit operands with guarded
divisors, and `__zeropage` globals. Those files are marked `no-host`
(the syntax does not exist on the host); they are still differential-
tested across all -O levels on the emulator, next to the software-C
equivalent computed in the same function.
