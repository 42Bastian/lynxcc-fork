<!--
SPDX-License-Identifier: CC-BY-4.0
Lynx Game Development SDK documentation, (c) 2026 the lynxcc authors,
licensed under Creative Commons Attribution 4.0 International.
See doc/licenses.html.
-->

# Lynx Game Development SDK — Layout & Directory Restructure Design

Status: **DESIGN** (2026-06-21). Source of truth for migrating this tree from a
single-target cc65 compiler fork into a structured **Atari Lynx game
development SDK** — a complete toolkit for building Lynx games, comprising a
toolchain, a core runtime, optional linkable subsystem libraries (graphics,
audio, math, compression), project templates, examples, and tests.

The product this tree builds is the **Lynx Game Development SDK**. The compiler
is one component of that SDK, not the whole project — the restructure exists to
make that framing real in the directory layout.

This document is the plan. No files have been moved yet. Implementation is
staged so the tree stays buildable (full sandbox rebuild green) and the docs
stay in sync at every commit, per `CLAUDE.md`.

---

## 1. Goals and non-goals

**Goals**

- Reorganise the tree so the *compiler* is one component among several, not the
  whole project.
- Make the platform library **modular**: a core library that every program
  links, plus optional subsystem libraries (`audio`, `graphics`, `math`) the
  user opts into per program.
- Make those optional libraries **effortless**: `cl65` pulls in whatever
  subsystem libraries a program actually references, automatically (§6.6).
- Give the new standalone SDK utility (`.lnx` manipulation) a home of its own,
  separate from the cc65-derived compiler.
- Add the scaffolding a real SDK needs: `templates/`, `examples/`, `tests/`,
  `extern/`.
- Do all of the above **without** breaking the working toolchain or its
  `CC65_HOME` discovery contract.

**Non-goals**

- No rewrite of the build system. We stay on recursive GNU Make + the existing
  MSVC `.vcxproj`/`.sln` files. **`cmake/` is dropped from the proposal.**
- No change to code generation, the runtime ABI, or the Lynx hardware drivers
  as part of the *move*. Behaviour-preserving relocation only.
- No renaming of the four data directories the binaries hard-code (see §2).

---

## 2. Hard constraints (these dictate the layout)

The toolchain locates its data directories by name, relative to either
`$CC65_HOME` or the binary's own `../` (the "WinBin" fallback). From the source:

| Binary | Looks up | Source |
| --- | --- | --- |
| `cc65` | `$CC65_HOME/include`, `<bin>/../include` | `compiler/cc65/incpath.c` |
| `ca65` | `$CC65_HOME/asminc`, `<bin>/../asminc` | `compiler/ca65/incpath.c` |
| `ld65` | `$CC65_HOME/lib`, `$CC65_HOME/cfg`, `<bin>/../lib`, `<bin>/../cfg` | `compiler/ld65/filepath.c` |

Consequences that the new layout **must** honour (or else patch C source):

1. `include/`, `asminc/`, `lib/`, `cfg/` keep **exactly those names** and stay
   at the repo root (the `CC65_HOME` directory). They cannot be nested under
   `libraries/`, `runtime/`, or `share/` without editing `incpath.c` /
   `filepath.c`. We keep them at root.
2. **All** binaries — the whole compiler suite *and* any new `tools/`
   utilities — must install into the **single** `bin/` at the root, because
   `cl65` shells out to its siblings and the WinBin fallback resolves
   `../include` etc. relative to the running binary.
3. Header namespacing is free: `#include <lynx/audio.h>` resolves to
   `include/lynx/audio.h` under the existing search path with **no source
   change**. Same for `asminc/lynx/*.inc`. This is why the hybrid include plan
   (§6) costs nothing in the compiler.
4. Optional libraries are just more `.lib` files in `lib/`; `ld65` already
   searches `lib/` for any archive named on the link line. No source change to
   support optional libs — only build-graph and link-line work.

These constraints are the reason the final tree (below) keeps four "data" dirs
at root even though a purist SDK layout would bury them.

---

## 3. Target directory layout

```
lynxcc/                     # repo root == CC65_HOME (dev tree)
├── compiler/               # cc65-derived toolchain (was src/)
│   ├── common/             #   shared support archive -> wrk/common/common.a
│   ├── cc65/  ca65/        #   C compiler, macro assembler
│   ├── ld65/  ar65/        #   linker, librarian
│   ├── co65/  cl65/        #   o65 converter, build driver
│   ├── sp65/               #   sprite/bitmap converter
│   ├── da65/               #   disassembler
│   ├── cc65.sln  *.vcxproj #   MSVC projects (all toolchain binaries)
│   └── Makefile
├── tools/                  # standalone SDK utilities (own binaries)
│   ├── lnx/                #   .lnx header/segment tool  (NEW, see §8)
│   ├── *.vcxproj           #   MSVC projects (tool binaries)
│   └── Makefile
├── runtime/                # C runtime + startup (was libsrc/runtime + crt0)
│   ├── rt/                 #   compiler runtime helpers (libsrc/runtime/*.s)
│   └── lynx/               #   crt0.s, bootldr, exehdr, mainargs, irq
├── libraries/              # SDK libraries (was the rest of libsrc/)
│   ├── core/               #   base platform: cart, load, lseek, clock, eeprom
│   ├── libc/               #   C stdlib (was libsrc/common)
│   ├── graphics/           #   TGI + fonts (was libsrc/lynx/tgi)
│   ├── audio/              #   Mikey sound (was libsrc/lynx/lynx-snd.s + new)
│   ├── math/               #   Suzy hw math (suzy*.s, async math)
│   └── compress/           #   zlib + lz4 (was libsrc/zlib)
├── include/                # PUBLIC C headers  (NAME FIXED at root)
│   ├── *.h                 #   C standard library headers stay flat
│   └── lynx/               #   platform + subsystem headers -> <lynx/...>
├── asminc/                 # PUBLIC asm includes (NAME FIXED at root)
│   ├── *.inc *.mac         #   generic asm includes stay flat
│   └── lynx/               #   lynx-specific .inc -> .include "lynx/..."
├── cfg/                    # linker configs   (NAME FIXED at root)
├── lib/                    # built archives   (NAME FIXED at root)
│   ├── lynx.lib            #   core lib auto-linked by the lynx target
│   ├── lynx-graphics.lib   #   optional, -l lynx-graphics
│   ├── lynx-audio.lib      #   optional
│   └── lynx-math.lib       #   optional
├── bin/                    # ALL built binaries (compiler + tools), single dir
├── templates/              # project starter templates (NEW, see §9)
├── examples/               # sample projects (was samples/), grouped by subsystem (§10)
├── tests/                  # automated tests + harness (NEW, see §10)
│   └── emu/gearlynx/       #   local test emulator (NOT shipped, see §8/§10)
├── extern/                 # placeholder for future vendored third-party code
├── doc/                    # documentation (name unchanged)
├── design/                 # *_DESIGN.md source-of-truth docs (unchanged)
├── Makefile                # top-level orchestrator
└── README.md  CLAUDE.md  LICENSE
```

Deviations from the originally proposed layout, with rationale:

- **`compiler/` holds the entire cc65 suite, including `sp65` and `da65`.**
  They were originally going to move to `tools/`, but that split them from the
  `common.a` they link; keeping the suite whole avoids the cross-directory
  dependency entirely (§5). `tools/` is reserved for the brand-new standalone
  utility `lnx`.
- **`runtime/` is split into `rt/` and `lynx/`** to separate
  compiler-coupled helper routines from Lynx startup/glue; both are
  always-linked and end up in `lynx.lib`.
- **`libraries/core` and `libraries/libc`** are added beyond your
  `lynx/audio/graphics/math` because the platform base and the C standard
  library are real, always-linked components that need a home and are *not*
  optional.
- **`cmake/` removed** (build stays Make-only).
- **`asminc/`, `cfg/`, `lib/`, `bin/` kept at root** — mandatory per §2; the
  proposal omitted them.
- **`doc/` keeps its name** (not renamed to `docs/`).
- **GearLynx is not shipped.** The emulator is a local testing tool only; it
  lives under `tests/emu/` and is excluded from every release artifact. `extern/`
  remains as a placeholder for any genuinely-vendored third-party code added
  later, but holds nothing today.

---

## 4. Old → new mapping (every existing path)

| Today | Moves to | Notes |
| --- | --- | --- |
| `src/cc65 ca65 ld65 ar65 co65 cl65 sp65 da65` | `compiler/` | `git mv`, whole suite kept together |
| `src/common` | `compiler/common` | shared archive, stays put |
| `src/*.vcxproj`, `cc65.sln` | `compiler/` | update relative paths only |
| `libsrc/runtime/*.s` | `runtime/rt/` | compiler runtime helpers |
| `libsrc/lynx/crt0.s bootldr.s exehdr.s bllhdr.s mainargs.s irq.s exec.s defdir.s uploader.s` | `runtime/lynx/` | startup/glue |
| `libsrc/lynx/lynx-cart.s load.s lseek.s open.s read.s clock.s eeprom*.s` | `libraries/core/` | base platform |
| `libsrc/lynx/tgi/*` | `libraries/graphics/` | TGI + fonts |
| `libsrc/lynx/lynx-snd.s` | `libraries/audio/` | Mikey sound |
| `libsrc/lynx/suzy*.s suzyasync.s` | `libraries/math/` | Suzy hw math |
| `libsrc/common/*` | `libraries/libc/` | C stdlib |
| `libsrc/zlib/*` | `libraries/compress/` | + `lz4` glue |
| `libsrc/lynx/joy libsrc/lynx/ser` | `libraries/core/` | static joy/ser drivers |
| `include/*.h` (stdlib) | `include/` (flat) | unchanged path |
| `include/lynx.h tgi.h suzymath.h serial.h joystick.h _suzy.h _mikey.h zlib.h lz4.h` | `include/lynx/` | becomes `<lynx/...>` (§6) |
| `asminc/lynx.inc extzp.inc` | `asminc/lynx/` | `.include "lynx/..."` |
| `asminc/*.mac generic asm inc` | `asminc/` (flat) | unchanged |
| `cfg/*.cfg` | `cfg/` | unchanged |
| `doc/*` | `doc/` | unchanged name |
| `samples/*` | `examples/` | grouped by subsystem; `lynxdemo.c` at root (§10) |
| `tools/gearlynx` | `tests/emu/gearlynx` | local test emulator; NOT shipped |
| `bin/ lib/ libwrk/ wrk/` | unchanged at root | build outputs |
| `.github .travis.yml` | unchanged | CI; update path refs |

Use `git mv` throughout to preserve history (as was done for the `design/`
move). Every `Makefile`, `.vcxproj`, `CC65_HOME`-relative path, doc link, and
`design/*_DESIGN.md` reference is updated in the **same commit** as the move it
describes.

---

## 5. Toolchain move (the whole cc65 suite stays together)

The entire cc65-derived suite — `cc65 ca65 ld65 ar65 co65 cl65 sp65 da65` plus
the shared `common/` support archive — moves as a unit from `src/` to
`compiler/`. This is a **pure directory rename**: the `PROGS` list, the
`common.a` dependency, the `OBJS_template`/`PROG_template` build pattern, and
the single root `bin/` output are all unchanged. There is no cross-directory
`common.a` dependency to manage, which is exactly why we keep the suite whole.

`tools/` is reserved for the new standalone utility `lnx`, which does
**not** depend on `compiler/common`; if a future tool ever needs `common.a`,
that is the point to reconsider publishing it to a stable shared path — not
now.

The **top-level `Makefile` orders `compiler` before `tools`** so the toolchain
binaries exist before anything that invokes them. All binaries install into the
single root `bin/` (mandatory per §2).

MSVC: `cc65.sln` and all its `.vcxproj` files move under `compiler/` intact;
only their relative paths are adjusted. New tool projects, when they exist, get
their own `.vcxproj` files under `tools/`, still emitting to the root `bin/`.

---

## 6. Modular library architecture

This is the core new capability: a small **always-linked core** plus
**opt-in subsystem libraries**.

### 6.1 What is "core" vs "optional"

| Library | Archive | Linked | Contents |
| --- | --- | --- | --- |
| Runtime | (in `lynx.lib`) | always | `runtime/rt` compiler helpers |
| Startup | (in `lynx.lib`) | always | `runtime/lynx` crt0, bootldr, exehdr, irq |
| Core platform | (in `lynx.lib`) | always | `libraries/core` cart, load, lseek, clock, eeprom, joy, ser |
| C stdlib | (in `lynx.lib`) | always | `libraries/libc` |
| Graphics | `lynx-graphics.lib` | opt-in | `libraries/graphics` TGI + fonts |
| Audio | `lynx-audio.lib` | opt-in | `libraries/audio` Mikey sound |
| Math | `lynx-math.lib` | opt-in | `libraries/math` Suzy hw mul/div + async |
| Compress | `lynx-compress.lib` | opt-in | `libraries/compress` zlib + lz4 |

`lynx.lib` (core) is the archive the `lynx` target auto-links, so existing
programs that use only stdlib + core keep building with no link-line change.

### 6.2 Why separate archives at all (cc65 already dead-strips)

`ld65` pulls only the referenced object modules out of any `.lib`, so a single
monolithic `lynx.lib` already produces minimal binaries. Splitting into
multiple archives buys **organisation and an explicit dependency contract**,
not smaller output:

- A subsystem can be versioned, documented, and shipped independently.
- An explicit `-l lynx-audio` on the link line keeps a program's dependencies
  legible and greppable when the user wants that, matching the project's
  existing "auditable by grep" ethos (cf. the `!*`/`!/` Suzy-math contract).
- It establishes the slot where future and community libraries plug in without
  bloating the core archive's symbol table.

The historical trade-off of split libraries — users must name optional libs on
the link line — is removed by the `cl65` auto-resolution design in §6.6: by
default the right libraries are pulled in automatically, with the explicit
`-l` path preserved for those who want it.

### 6.3 Linking mechanism

- Built archives all land in `lib/` (fixed name, §2). `ld65`/`cl65` already
  search `lib/`, so any archive resolves by name with no source change.
- Link order is **dependents-first, core (`lynx.lib`) last**, so `ld65`'s single
  in-order library pass resolves cross-references (e.g. graphics → math → core).
- By default `cl65` pulls the whole SDK library set automatically (§6.6);
  on-demand module extraction means unused subsystems add nothing to the binary.
- Users who want an explicit, minimal link line use `--no-sdk-libs` plus
  `-l <lib>`, or invoke `ld65` directly. `examples/`/`templates/` Makefiles keep
  an optional `LIBS` variable for that manual path and for out-of-tree libraries.
- A program that references a subsystem symbol whose library has been excluded
  gets an undefined-symbol link error — the intended, legible failure mode.

### 6.4 Inter-library dependencies

Document each optional lib's dependencies in its design note and header:

- `graphics` → core (framebuffer, Suzy SCB blitter), optionally `math`.
- `audio` → core (Mikey timers/IRQ).
- `math` → none beyond runtime (pure Suzy register ops).
- `compress` → libc (`memcpy`).

The linker resolves these as long as `lynx.lib` (core) is listed last. No
circular dependencies are introduced by the split.

### 6.5 `libsrc/Makefile` impact

The current single-target `libsrc/Makefile` produces one `lynx.lib`. It is
replaced by per-subtree builds under `runtime/` and `libraries/` whose objects
are partitioned into the archives in §6.1 via `ar65`. A small top-level
`libraries.mk` (or the root Makefile) drives them in dependency order. The
`CC65_HOME := $(abspath ..)` export and `ar65 d` stale-object purge habits
carry over unchanged.

### 6.6 `cl65` automatic library resolution

The goal: a user writes `cl65 game.c -o game.lnx` and the graphics, audio,
math, or compression code they referenced is linked **automatically** — no `-l`
flags — while subsystems they don't touch cost nothing.

This is cheap because `ld65` extracts only the referenced object modules from
each library it scans. So `cl65` can hand `ld65` the *entire* SDK library set on
every link; libraries nothing references contribute zero bytes. "Automatic"
therefore reduces to "`cl65` always offers `ld65` the full SDK set, in the right
order." Today `cl65` already appends one default library (`lynx.lib`) for the
target; this generalises that single default into an ordered list.

**Mechanism**

1. **SDK library manifest.** `cl65` learns the optional-library set from a data
   file, not hard-coded names, so new or out-of-tree libraries don't require
   recompiling `cl65`. The `libraries/` build emits `lib/lynx-sdklibs.list`,
   one archive per line in link order (dependents first, core last):

   ```
   lynx-graphics.lib
   lynx-audio.lib
   lynx-compress.lib
   lynx-math.lib
   lynx.lib
   ```

   The manifest is found via the same `CC65_HOME/lib` + WinBin `../lib` search
   `ld65` already uses (§2) — no new path logic. If the manifest is absent,
   `cl65` falls back to today's behaviour (just `lynx.lib`), so the feature
   degrades gracefully.

2. **Link-line assembly.** At the link step for the lynx target, `cl65` appends
   the manifest entries — in listed order, after all user objects and libraries
   — instead of appending only `lynx.lib`. Because the order is dependents-first
   with core last, `ld65`'s single in-order pass resolves every cross-reference.
   User-supplied `-l`/explicit `.lib` arguments are placed *before* the manifest
   set and de-duplicated, so a user can still pin or override a library.

3. **On-demand extraction does the rest.** A program that calls no audio
   function links no audio module even though `lynx-audio.lib` was offered.
   Output is byte-for-byte identical to hand-listing exactly the needed libs.

4. **New `cl65` options.**
   - `--no-sdk-libs` — append only the core `lynx.lib`, not the optional set;
     restores the fully explicit `-l` workflow (the §6.2 legibility path).
   - `--sdk-libs <file>` — use an alternative manifest (e.g. one that adds an
     out-of-tree library) for projects that extend the SDK.

   These mirror the spirit of cc65's existing library/target-lib switches.

5. **Acyclicity contract.** Automatic resolution relies on the SDK libraries
   forming a DAG (no two optional libs mutually reference each other) so one
   ordered pass suffices. §6.4 establishes this; the manifest's fixed order
   encodes it. A future library that would introduce a cycle must instead be
   merged or refactored — recorded in that library's design note.

6. **What does *not* change.** No change to `ld65`, the object format, or the
   libraries themselves: this is a `cl65` driver change plus one shipped manifest
   file. `ld65` invoked directly still needs explicit `-l`/`.lib` arguments —
   the convenience lives in `cl65`, preserving the fully-explicit low-level path.

**Touch points:** `compiler/cl65/main.c` (link-step argument assembly + the two
new long options), a generated `lib/lynx-sdklibs.list` from the `libraries/`
build, and `doc/cl65.html` (docs-in-sync rule). Implemented in phase 6; the
source-of-truth note is `design/LYNX_CL65_AUTOLIBS_DESIGN.md`.

---

## 7. Headers and asm includes (hybrid namespacing)

Per the chosen hybrid: **C standard library headers stay flat** at `include/`
root (portability, cc65 familiarity), **platform and subsystem headers move
under `include/lynx/`** and are included as `<lynx/...>`.

- Stays flat: `stdio.h stdlib.h string.h ctype.h stdint.h inttypes.h
  stddef.h stdbool.h stdarg.h limits.h errno.h setjmp.h time.h
  iso646.h fcntl.h unistd.h dirent.h 6502.h peekpoke.h cc65.h zeropage.h
  ascii_charmap.h` and the `_heap.h` internal.
- Moves to `include/lynx/`: `lynx.h tgi.h suzymath.h serial.h joystick.h
  zlib.h lz4.h` and the private `_suzy.h _mikey.h`. New subsystem headers
  (`lynx/audio.h`, `lynx/graphics.h`, `lynx/math.h`) are created here.
- `asminc/` mirrors this lightly: Lynx-specific `lynx.inc`, `extzp.inc` move
  under `asminc/lynx/`; generic `.mac` files stay flat.

Because the search root is unchanged, the compiler/assembler need **no source
edit** (§2.3). The cost is a sweep of `#include`/`.include` lines across
`examples/`, `runtime/`, `libraries/`, and every `doc/*.html` and
`design/*.md` that names a moved header — done in the same pass as the move,
per `CLAUDE.md`. A backward-compat shim (a flat `include/tgi.h` that just
`#include <lynx/tgi.h>`) is **not** added; the project's standing convention is
to fix references rather than leave silent aliases, and an undocumented shim
would read as an oversight later.

---

## 8. Tools

*Status: IMPLEMENTED 2026-06-22 (phase 8). `tools/lnx` ships as described below —
`info`/`dump`/`patch`/`create` over the 64-byte header, driven by CLI flags and
an optional per-game JSON config (flags overlay config), covering every header
field including the AUDIN and EEPROM flag bytes. Source-of-truth note:
`design/LYNX_LNX_TOOL_DESIGN.md`. Directory/segment listing and `.lnx`→raw strip
are deferred there as deliberate non-goals.*

`tools/` holds **new** standalone-binary utilities only. The cc65-derived
`sp65` (sprite packer) and `da65` (disassembler) stay in `compiler/` with the
rest of the suite (§5). Critical: do **not** build utilities that duplicate
existing toolchain capability — wrap or rename instead.

| Tool | Status | Purpose | Overlap note |
| --- | --- | --- | --- |
| `lnx` | IMPLEMENTED (phase 8) | inspect/patch `.lnx` headers (`info`/`dump`/`patch`/`create`) — names, rotation, bank sizes, version, AUDIN and EEPROM flags — with per-game header config via CLI flags or JSON; raw→`.lnx` wrap. Segment list/extract and `.lnx`→raw strip deferred (see `design/LYNX_LNX_TOOL_DESIGN.md` §7). | `.lnx` *generation* already happens via `ld65` + `cfg/*.cfg`; `lnx` is a post-build inspector/editor, not a second linker. |

Sprite/bitmap conversion is **not** a `tools/` entry: that is `sp65`, which
already exists in the toolchain (`compiler/`). No duplicate sprite engine and no
separate `sprpck` wrapper are built. The `lnx` tool is deliberately scoped to
post-link manipulation so it does not re-implement `ld65`. Cart/EEPROM image
construction stays handled by `ld65` + `cfg/*.cfg` as today; no `romtool` is
built.

The GearLynx headless emulator + MCP harness (currently `tools/gearlynx`) is
**not** part of the SDK and is never shipped. It is a local testing tool only:
it lives under `tests/emu/gearlynx`, is driven by the integration tests (§10),
and is excluded from all release artifacts (§11). It is not placed in `extern/`,
which is reserved for genuinely-vendored code that *does* ship.

---

## 9. Templates

*Status: IMPLEMENTED 2026-06-22 (phase 7). `templates/basic/` exists as
described; the `src/main.c` skeleton moves a label with the joystick because the
static TGI exposes sprite + text, not a pixel/line primitive.*

`templates/` provides `cl65`/Make starting points so a user can scaffold a new
game without copying an example. Minimum one template, structured to grow:

```
templates/
└── basic/
    ├── Makefile          # CC65_HOME-aware; relies on cl65 auto-libs (§6.6)
    ├── src/main.c        # init TGI, clear, main loop, read joystick
    ├── README.md
    └── .gitignore
```

Later: `graphics`, `audio`, `full-game` variants. Because `cl65` auto-resolves
SDK libraries (§6.6), a template `Makefile` needs no `LIBS` boilerplate to use
graphics/audio/math — it is the canonical example of the `<lynx/...>` include
convention (§7) and the zero-config link path. The optional `LIBS` hook is shown
only in an advanced variant.

---

## 10. Examples and tests

**`examples/`** replaces `samples/`. Today `samples/` is a flat pile of `.c` +
generated `.lnx/.map/.o/.s` + asset files. The programs are sorted into
**subsystem group subdirectories** so the top level stays uncluttered. Each
group holds the example `.c` files directly (the generated `.s/.o/.map/.lnx`
land beside them); the smallest complete program, `lynxdemo.c`, is the starter
and sits at the `examples/` root rather than in a group:

```
examples/
├── Makefile          # iterates the group subdirs
├── lynxdemo.c        # starter: the smallest complete program, kept at the root
├── games/            # breakout.c  invaders.c  raycaster.c  sybil.c
├── suzy/             # muldivtest.c  spritetest.c  suzyasync.c  suzyasyncbench.c
│                     #   suzybench.c  packtest.c  fonttest.c
│                     #   (+ spritetest assets: heart.pcx, heart.pcx.py, and the
│                     #    generated heart_packed.h / heart_literal.h)
├── mikey/            # setbpp.c
├── memory/           # heaptest.c  zeropage.c
└── network/          # comlynx.c
```

Group meanings: `games/` complete playable demos; `suzy/` exercises of the Suzy
sprite engine and hardware-math operators (`!*` `!/` `!%`, synchronous and
async) plus build-time/runtime sprite packing and the scaled TGI fonts;
`mikey/` Mikey display-DMA modes; `memory/` the heap allocator and zero-page
placement; `network/` ComLynx serial. The `sprpack` sample is renamed to
`spritetest`, and its `heart.*` assets live in `suzy/` alongside it.

This grouping is mirrored in `doc/samples.html`: its section headings follow the
same five groups (with `lynxdemo` documented first as the starter), so the
documented structure and the on-disk directory structure stay in step — required
by the docs-in-sync rule in `CLAUDE.md`.

Examples generally rely on `cl65` auto-library resolution (§6.6), so most need
no `LIBS` declaration; an example that deliberately demonstrates explicit
linking (with `--no-sdk-libs` + `-l`) documents that path for §6.

**`tests/`** formalises verification that is currently ad-hoc (host simulators,
GearLynx 0-diff framebuffer checks):

```
tests/
├── unit/          # host-built C/sim checks: Suzy math corpus, heap churn,
│                  #   divisor-normalisation sweeps, runtime ABI
├── integration/   # build each example, run on emu/gearlynx headless,
│                  #   compare framebuffer/screenshot vs golden
├── golden/        # reference framebuffers / expected output
├── emu/gearlynx/  # local GearLynx emulator + MCP harness (NOT shipped)
└── run.sh         # CI entry point; used by .github workflow
```

This turns the existing "GearLynx 0-diff verified" practice into a repeatable
gate and is where the `always full rebuild` discipline is enforced in CI. The
emulator under `tests/emu/gearlynx` is a developer/CI testing tool only — it is
never part of an SDK release (§11).

*Status: IMPLEMENTED 2026-06-22 (phase 7). `unit/` ships `suzymath.c`, a
host model sweeping the Suzy math invariants (software-mod recompute, the d<256
divisor-normalisation identity, the 32-bit-intermediate fused muldiv, signed
sign-fixup); the heap/ABI checks remain to be added in the same `unit/`
pattern. `integration/gearlynx_check.py` boots each example through
`emu/gearlynx/run.sh`, steps a fixed frame count with no input, and compares a
SHA-256 of the screenshot against `golden/<name>.sha256` (a curated cross-
subsystem set); it SKIPS (exit 0) when the emulator/BIOS are absent, so the
unit tests are the always-on CI gate. `run.sh` chains the two and is invoked by
`make tests` and `.github/workflows/ci.yml`.*

---

## 11. Extern and doc

Release packaging stays handled by the existing `make install`/`zip` targets and
`build-windows.ps1` step (§12); they already map the dev tree to the installed
`<prefix>/share/cc65/{include,asminc,lib,cfg}` (or WinBin `bin/../`) layout the
binaries expect, and **exclude `tests/` (including `tests/emu/gearlynx`)** — the
emulator and test harness never ship in an SDK release. A dedicated `packaging/`
directory is **not** added.

- **`extern/`** — placeholder for genuinely-vendored third-party code that
  *ships* (e.g. a future compression reference). It holds nothing today;
  GearLynx is **not** here — it is a non-shipped test tool under `tests/emu/`.
- **`doc/`** — keeps its name (not renamed). The HTML doc build (`doc/Makefile`,
  `doc.css`, `doc.js`) is updated in place as code moves: internal links and
  `design/` cross-references re-pathed, new tool/library pages added. Per
  `CLAUDE.md`, doc updates ride along with each code move, and any new SVG
  diagrams follow `design/DOC_SVG_STYLE_DESIGN.md`.
  - **SDK rebrand sweep.** The shared `site-foot` footer string repeated in
    every `doc/*.html` now reads "lynxcc documentation — a complete Atari Lynx
    game development toolkit." (done as one find-and-replace across all pages),
    replacing the old "a fork of cc65 focused on the Atari Lynx target." The
    remaining "fork of cc65" framing in `history.html` and `intro.html` prose
    still awaits the same **Lynx Game Development SDK** rebrand — the SDK is the
    product; the cc65-derived toolchain is one component of it. This is a
    doc-only branding pass with no code impact; the licensing re-pointing in
    `history.html` is tracked separately in
    `design/LYNX_LICENSE_CONSOLIDATION_DESIGN.md`.
  - **Brand chrome (done).** The `index.html` hero now opens with the SDK
    wordmark (`doc/logo.svg`, also shipped as a Makefile asset) and SDK copy:
    "lynxcc is a complete game development SDK for the Atari Lynx…". The hero is
    full-bleed (no `max-width`/character cap) with the logo centred. A scaled
    `logo.svg` sits in the shared `topbar` brand on every page, and the nine
    command-line tools (`ar65`, `ca65`, `cc65`, `cl65`, `co65`, `da65`, `ld65`,
    `lnx`, `sp65`) are collapsed into a single **Tools** dropdown there
    (`doc.css` `.dropdown*`, `doc.js` toggle) instead of four loose links; the
    `.nav` no longer wraps/scroll-clips, so the menu overlays page content
    cleanly. `funcref.html` uses a javadoc-style two-pane layout — a sticky,
    filterable function index in `.fn-aside` on the left, the reference body in
    `.fn-main` on the right; the redundant alphabetical function list has been
    dropped from the in-page contents `nav.toc` (the sidebar replaces it).

---

## 12. Build system (Make-only)

`cmake/` is dropped. The top-level `Makefile` becomes the orchestrator, with
strict ordering so cross-directory dependencies resolve:

```
all:  compiler  tools  runtime+libraries  examples  doc  [tests]
```

Concretely:

1. `compiler/` — builds `common.a` then the full cc65 suite (`cc65 ca65 ld65
   ar65 co65 cl65 sp65 da65`) into `bin/`.
2. `tools/` — builds the new utility (`lnx`) into `bin/`.
3. `runtime/` + `libraries/` — build the object trees and archive them into
   `lib/lynx.lib` (core) and `lib/lynx-*.lib` (optional), in dependency order,
   and emit the `lib/lynx-sdklibs.list` manifest for `cl65` auto-libs (§6.6).
4. `examples/` — build each example against the freshly built `bin/` + `lib/`.
5. `doc/` — regenerate HTML.
6. `tests/` (CI / on demand) — unit + integration via `tests/emu/gearlynx`.

Existing conventions carry over verbatim: `CC65_HOME := $(abspath ..)` export
for self-hosting, `ar65 d` to purge stale objects when a source is removed,
the `bin/`-relative tool discovery in example Makefiles, and the
`mostlyclean`/`clean`/`zip`/`install` phony targets. The MSVC `.sln`/`.vcxproj`
set adds the new `tools/` projects but
the existing compiler projects are otherwise preserved.

Per project feedback, every change in the migration is followed by a **full
sandbox rebuild** (toolchain + libraries + examples) before the step is
considered done — never a partial build.

---

## 13. Internal restructure phases (moving the tree)

This section is how *we* restructure the tree; for how *users* port their code
from upstream cc65 onto the **lynxcc** SDK, see §14.

Each phase is a self-contained commit that leaves the tree **building green**
and **docs in sync**. Ordering minimises the window where references dangle.

1. **Design first.** Land this design doc. No code move yet. (Cheap,
   reversible.)
2. **`samples/ → examples/`**, sorted into subsystem group subdirectories
   (`games/`, `suzy/`, `mikey/`, `memory/`, `network/`) with the starter
   `lynxdemo.c` at the `examples/` root, including the `sprpack → spritetest`
   rename. Update the examples Makefile (iterates the groups), root Makefile,
   and `doc/samples.html` (section headings follow the same groups). Rebuild +
   run examples on GearLynx. (`doc/` keeps its name — no doc rename.)
3. **`src/ → compiler/`** (the whole cc65 suite, `sp65`/`da65` included),
   `tools/gearlynx → tests/emu/gearlynx`. Update Makefiles, `.vcxproj`/`.sln`,
   `CC65_HOME` refs, `.github`/`.travis`. Pure rename; full rebuild and verify
   `sp65` still produces byte-identical packed assets (the existing GearLynx
   0-diff sprpack/spritetest check).
4. **Header/asm namespacing** (§7): move platform headers to `include/lynx/`,
   asm includes to `asminc/lynx/`; sweep every `#include`/`.include` and doc
   reference. Full rebuild.
5. **Library split** (§6): relocate `libsrc/` into `runtime/` + `libraries/`,
   partition archives into `lynx.lib` + `lynx-*.lib`. This is the largest
   phase — validate that every example links and that GearLynx framebuffers are
   unchanged.
6. **`cl65` auto-libraries** (§6.6): emit `lib/lynx-sdklibs.list`, add the
   manifest-driven link-line assembly and `--no-sdk-libs`/`--sdk-libs` options
   to `compiler/cl65/main.c`, update `doc/cl65.html`, add
   `design/LYNX_CL65_AUTOLIBS_DESIGN.md`. Verify each example links with zero
   `-l` flags and produces identical binaries.
7. **New scaffolding** (IMPLEMENTED 2026-06-22): `templates/basic` and the
   `tests/` harness (host `unit/` + GearLynx `integration/` + `golden/`,
   `tests/run.sh`) wired to CI via `.github/workflows/ci.yml` and the root
   `make tests` target.
8. **New tool** (IMPLEMENTED 2026-06-22): `lnx` under `tools/lnx`, built into the
   root `bin/` via a new `tools/Makefile` (no `compiler/common` dependency) and
   the top-level `Makefile`'s `compiler → tools → libraries` order; MSVC
   `tools/lnx.vcxproj` registered in `compiler/cc65.sln`. Commands
   `info`/`dump`/`patch`/`create` over the 64-byte header, with an optional
   per-game JSON config (CLI flags overlay config) for per-game cartridge
   metadata. Source-of-truth note `design/LYNX_LNX_TOOL_DESIGN.md`; docs in
   `doc/lnx.html` (carded on `index.html`).
9. **License restructure** (§13.1, IMPLEMENTED 2026-06-23): adopt the
   per-component licensing pattern common to game-development SDKs — **MPL-2.0**
   on the fork's own new SDK/toolchain files, **MIT** on `examples/` and
   `templates/`, and **CC-BY 4.0** on `doc/` — register all three in
   `doc/licenses.html`, and capture the policy in a new
   `design/LYNX_LICENSE_POLICY_DESIGN.md` (new design doc per `CLAUDE.md`). No
   code behaviour changes: this phase touches license headers, `LICENSE*` files,
   and the license registry only. Fork-authored files were identified
   mechanically against the `upstream/master` baseline (47 source files took the
   MPL header; inherited cc65 and bundled third-party files were left untouched).

Finally, as a separate **un-numbered** step taken once the numbered phases above
have all landed:

- **Tag the SDK release**: a version bump marking the reference point users
  target when following the §14 migration guide.

Phases 1–4 are behaviour-preserving moves. Phase 5 changes the *link contract*
and gets the most verification; phase 6 makes that contract automatic. Phases
7–9 and the final release tag are additive.

### 13.1 License restructure (phase 9)

*Status: IMPLEMENTED 2026-06-23. The full mapping, the per-file detection
method, the SPDX header texts, the new `LICENSE*` files, and the
`doc/licenses.html` edits are recorded in the source-of-truth note
`design/LYNX_LICENSE_POLICY_DESIGN.md`.*

The SDK is currently distributed entirely under the cc65 zlib-style license
plus the inventory of upstream/third-party notices consolidated into
`doc/licenses.html` (see `design/LYNX_LICENSE_CONSOLIDATION_DESIGN.md`). Phase 9
layers a per-component licensing pattern on top of that baseline, mirroring how
many game-development toolchains license their pieces differently:

| Component | New license | Scope |
| --- | --- | --- |
| SDK / toolchain | **MPL-2.0** | **Only** the new features and files this fork has authored. Existing files keep their current (cc65 zlib-style / Dunning / third-party) notices unchanged. |
| Example games / templates | **MIT** | `examples/` and `templates/` — permissive so users can copy starter code into their own games with no copyleft reach. |
| Documentation | **CC-BY 4.0** | `doc/` — Creative Commons Attribution, the conventional license for prose/reference material. |

Key points that keep this compatible with the existing tree:

- **MPL-2.0 is file-scoped by design.** Its copyleft attaches per *file*, so
  applying it to only the fork's own new sources — without disturbing the
  zlib-style notices on inherited cc65 files or the bundled third-party headers
  (`include/zlib.h`, `include/lz4.h`) — is exactly the granularity MPL expects.
  Inherited and third-party files keep their notices; the package stays a
  compliant mix.
- **Examples/templates as MIT** avoids imposing any SDK license on the games
  users build from them, which is the point of shipping starter code.
- **Docs as CC-BY 4.0** covers `doc/*.html` and the `design/*_DESIGN.md` sources
  that feed them; attribution is satisfied by the existing authorship/footer
  lines.
- **Single registry.** Per the consolidation design's §8 future-components rule,
  each new license is added to `doc/licenses.html` (verbatim license text in its
  own subsection) rather than scattered through the manuals; the root `LICENSE`
  file and per-source headers remain the authoritative copies. SPDX
  `License-Identifier` tags on the fork's new files make the per-file scope
  machine-checkable.

The full mapping (which paths get which header, the verbatim license bodies, and
the `doc/licenses.html` edits) lives in `design/LYNX_LICENSE_POLICY_DESIGN.md`,
authored as part of this phase.

---

## 14. Migrating from upstream cc65 to lynxcc

**lynxcc** is not a drop-in upstream cc65: it is the **Lynx Game Development SDK**,
and code or build scripts carried over from stock cc65 (its Lynx target) need
adjustment. This section catalogs every difference a porter meets and how to
handle it; it is the source for a user-facing `doc/migrating.html` (§14.4).

### 14.1 Differences from upstream cc65

Source- and build-level changes:

| Area | Upstream cc65 | **lynxcc** SDK | Porting action |
| --- | --- | --- | --- |
| Platform includes | `<lynx.h> <tgi.h> <suzymath.h> <serial.h> <joystick.h> <zlib.h> <lz4.h>` | same headers under `<lynx/…>` (§7) | rewrite those `#include` lines (script, §14.2); C-stdlib headers unchanged |
| Libraries | one `lynx.lib` | core `lynx.lib` + optional `lynx-graphics/audio/math/compress.lib` (§6) | usually none — `cl65` auto-pulls them (§6.6); only `ld65`-direct users add `-l` |
| Tree layout | `src/ libsrc/ samples/` | `compiler/ runtime/ libraries/ examples/` (§3) | affects only those who build the toolchain or reference tree paths, not end-user game projects |
| Header shims | — | **none** by design (§7) | a missing-header build error is the intended signal to run the include-rewrite |
| CLI flags | `--target --cpu --memory-model --standard` | hard-wired to Lynx + 65SC02, permanent cc65 dialect. `--memory-model`/`--standard` removed; `-t`/`--target` and `--cpu` kept as validated no-ops (accept only `lynx`, resp. `65C02`/`65SC02`) for backwards compatibility | drop `--memory-model`/`--standard`; `-t`/`--cpu` may stay but have no effect |

API and runtime changes:

- **Fork-only operators.** The Suzy hardware operators `!*`, `!/`, `!%` are
  **lynxcc** extensions — source using them is *rejected* by stock cc65, so the move
  is one-way. Nothing to do porting *into* **lynxcc**; relevant only if porting back
  out.
- **Removed / trimmed APIs.** `assert.h`, `device.h`, `pen.h`, `locale.h`,
  `sys/`, and the user-facing `target.h` are removed; `time.h` is trimmed to
  `clock()`/`sleep()`; the stdio *output* family and the `FILE` stream layer are
  gone (`sprintf`/`snprintf`/`sscanf` kept); cart access is numbered-only
  (`openn`, no `fopen`). Replace or remove these uses.
- **Static drivers.** No dynamic driver loading (`modload`/`mouse`/`em`/`dbg`
  removed); TGI, joystick and serial are direct-call static APIs. Drop any
  driver-load/install calls and use the static entry points.

The `CC65_HOME` discovery contract is **unchanged** (§2): `include/ asminc/ lib/
cfg/` keep their names and root locations, so existing project Makefiles that set
`CC65_HOME` keep working without edits.

### 14.2 Migration aids

- **Include-rewrite script.** Because the header move is mechanical and total
  (no shims), the supported path is a scripted rewrite — a `sed`/Python one-shot
  applying the fixed mapping (`lynx.h tgi.h suzymath.h serial.h joystick.h zlib.h
  lz4.h` → `lynx/<same>`), shipped under `tools/` or alongside `doc/migrating.html`.
- **Prefer `cl65`.** It auto-resolves the optional libraries (§6.6), so most
  projects need no link-line change. `ld65`-direct builds consult the dependency
  table (§6.4) and add the `-l` entries dependents-first, core last.
- **Reference builds.** Port against the updated `examples/`/`templates/`, which
  demonstrate the new include paths and zero-config linking; verify on
  `tests/emu/gearlynx` where available.

### 14.3 Compatibility deliberately preserved

`CC65_HOME`/WinBin discovery and the `include/asminc/lib/cfg` names and locations
(§2); the basic `cl65` workflow; `.lnx` output and cfg-driven memory layout; and
all standard C library headers at their flat paths. A project that uses only the
C stdlib and the core platform, built via `cl65`, may need **no** changes at all.

### 14.4 Deliverable and versioning

- **`doc/migrating.html`** is authored from this section (done 2026-06-24),
  with a **Migrating** nav entry across the whole doc set and a card under the
  **Usage** heading on `index.html`.
- **Tag the SDK release** (a version bump) as the reference point users target
  when following this guide — its own un-numbered final step in the rollout,
  after the numbered phases (§13).

---

## 15. Risks and open questions

- **Auto-libs over-linking masking real gaps.** Because `cl65` offers every SDK
  library by default (§6.6), a program never sees a "missing `-l`" error for a
  shipped subsystem — the linker just resolves it. That is the intended
  convenience, but it means dependency mistakes only surface under
  `--no-sdk-libs`. The integration tests should run at least one build with
  `--no-sdk-libs` + explicit `-l` to keep the explicit path exercised.
- **No compat shims for headers** means a noisy phase-4 diff and any
  out-of-tree user code breaks. Acceptable for a pre-1.0 SDK; flagged so it is a
  deliberate choice, not an accident.
- **Install-tree vs dev-tree parity.** The installed tree must reproduce the
  `include/asminc/lib/cfg` adjacency the binaries hard-code; test an actual
  install (via the existing `make install`/`zip`, §12), not just the dev tree.
- **Repo name.** Directory is still `lynxcc`; renaming the repo to `lynxsdk` is
  a separate, cosmetic decision left to the maintainer.

---

## 16. Documentation & memory sync checklist

For each phase, before it is "done":

- [ ] `include/*.h` / `asminc/*.inc` doc comments updated for moved symbols.
- [ ] `doc/*.html` (function reference, TGI, samples, tool pages) re-pathed and
      re-linked; new tool/library pages added.
- [ ] `design/*_DESIGN.md` cross-references updated to new paths; new tools get
      their own `*_DESIGN.md` in `design/`.
- [ ] `README.md` directory overview updated.
- [ ] Full sandbox rebuild green (toolchain + libraries + examples).
- [ ] Examples re-verified on `tests/emu/gearlynx` (0-diff where applicable).
- [ ] Auto-memory note recorded for the phase.
```
