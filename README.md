# lynxcc — Atari Lynx Game Development SDK

**lynxcc** is a complete SDK for building Atari Lynx games. It bundles a C and
assembly toolchain, an always-linked runtime, opt-in subsystem libraries
(graphics, audio, math, compression), cartridge tooling, project templates,
worked examples, host and emulator tests, and full HTML documentation — the
whole path from source to a bootable `.lnx` cartridge.

The **compiler toolchain is a component of the SDK**: a fork of cc65 2.19
narrowed to the single Atari Lynx target and tuned for faster 65SC02 code
generation, with all other machine targets, libraries and configs removed. It
provides the macro assembler, C compiler, linker, librarian and related
binaries. The SDK builds its own Lynx runtime, libraries, tools, templates,
tests and documentation on top of that compiler core.

Upstream cc65 documentation: [cc65.github.io/doc](https://cc65.github.io/doc)

## Repository layout

The tree is organised as a Lynx Game Development SDK
([design/LYNX_SDK_LAYOUT_DESIGN.md](design/LYNX_SDK_LAYOUT_DESIGN.md)):

- `compiler/` — the cc65 toolchain suite (cc65, ca65, ld65, ar65, co65, cl65,
  sp65, da65).
- `tools/` — standalone SDK utilities outside the compiler suite. `tools/lnx`
  inspects and patches the 64-byte `.lnx` cartridge header (per-game name,
  manufacturer and rotation, via flags or a JSON config); see
  [design/LYNX_LNX_TOOL_DESIGN.md](design/LYNX_LNX_TOOL_DESIGN.md).
- `runtime/`, `libraries/` — the always-linked runtime plus the core and opt-in
  subsystem libraries (graphics, audio, math, compression), archived into
  `lib/lynx.lib` + `lib/lynx-*.lib`.
- `include/`, `asminc/`, `cfg/`, `lib/`, `bin/` — the data and output
  directories the binaries discover via `CC65_HOME`.
- `templates/` — copyable project starters; `templates/basic` is the minimal
  one-file game.
- `examples/` — sample programs grouped by subsystem.
- `tests/` — host unit tests plus GearLynx integration tests; run with
  `make tests` (see [tests/README.md](tests/README.md)).
- `doc/`, `design/` — HTML documentation and the source-of-truth design notes.

## License

The SDK uses a per-component licensing pattern on top of the inherited cc65
zlib-style baseline; every source file carries an `SPDX-License-Identifier` tag.

- **Inherited cc65 sources** keep their original zlib-style notices (root
  [LICENSE](LICENSE)); this includes the static Lynx graphics modules and the
  ComLynx serial modules, which were extracted and adapted from Karri Kaksonen's
  cc65 Lynx drivers and keep his copyright. The John R. Dunning and bundled
  third-party (zlib/inflate, lz4) notices are unchanged.
- **The fork's own toolchain, runtime, library and tool sources authored from
  scratch** are licensed under the **Mozilla Public License 2.0**
  ([LICENSE-MPL-2.0.txt](LICENSE-MPL-2.0.txt)).
- **`examples/` and `templates/`** are licensed under the **MIT License** (a
  `LICENSE` file in each directory) so you can copy starter code into your own
  games freely.
- **Documentation** (`doc/`, `design/`) is licensed under **CC-BY 4.0**
  ([doc/LICENSE](doc/LICENSE)).

All notices are inventoried in [doc/licenses.html](doc/licenses.html); the policy
and exact file mapping are in
[design/LYNX_LICENSE_POLICY_DESIGN.md](design/LYNX_LICENSE_POLICY_DESIGN.md).

## Lynx code-generation improvements

See [LYNX_CODEGEN_DESIGN.md](design/LYNX_CODEGEN_DESIGN.md) for the full design,
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
  Implemented as `tossuzy*` routines in `libraries/math/` plus the parser
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
