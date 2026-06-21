# Design: declaring zero-page variables directly from C

Scope: a C-level way to place a variable in the Lynx zero page and have the
compiler address it as a zero-page symbol, without writing a companion `.s`
file and without the `#pragma bss-name` / `extern` / `#pragma zpsym` dance.

**Status: IMPLEMENTED 2026-06-18.** Two equivalent surface forms:

1. The post-declarator attribute `__attribute__((zeropage))`, with the macro
   `__zeropage` supplied by `include/zeropage.h` — a *suffix* (cc65 accepts
   attributes only post-declarator). See §4 for hook points, §10 for what was
   verified.
2. The prefix keyword `__zeropage__`, a reserved specifier that sits alongside
   the storage class — added later the same day; see §11.

Both forms fold into the same `SC_ZEROPAGE` symbol flag and share every
downstream behaviour (segment placement, `.importzp`/`.exportzp`, validation).

## 1. What people write today, and why it hurts

To get a fast zero-page global you currently do one of two things.

Either you define the storage in assembly and import it into C:

```asm
; zpvars.s
.zeropage
.export _zeroValue
_zeroValue: .res 1
```

```c
/* user.c */
#pragma bss-name (push, "ZEROPAGE")
  extern unsigned char _zeroValue;
  #pragma zpsym ("_zeroValue")
#pragma bss-name (pop)
```

…or you skip the `.s` file by *defining* the variable inside a pushed
`bss-name` segment, but you still need the `zpsym` incantation in every other
translation unit that references it.

Three things make this painful:

1. It needs a second source file and a second toolchain language for what is
   conceptually one C variable.
2. The `extern` block is fragile boilerplate: the `bss-name push`/`pop` around
   an `extern` does nothing (no storage is emitted for an `extern`), so it is
   cargo-cult; the only load-bearing line is `#pragma zpsym`.
3. `zpsym` takes the **assembler** name (`"_zeroValue"`, with the leading
   underscore), must appear *after* the declaration, and is easy to mismatch.
   A typo silently degrades to absolute addressing instead of failing.

The goal is to collapse all of this to a single, self-documenting C
declaration that works identically for a definition and for an `extern`.

## 2. How zero page actually works in this toolchain

The key architectural facts (verified in the current tree) are what make this
feature cheap:

**ca65 has a built-in `ZEROPAGE` segment with zeropage address size.**
`src/ca65/segment.c` initialises
`ZeropageSegDef = STATIC_SEGDEF_INITIALIZER (SEGNAME_ZEROPAGE, ADDR_SIZE_ZP)`,
and `SEGNAME_ZEROPAGE` is `"ZEROPAGE"` (`src/common/segnames.h`). So *any* label
defined while the `.segment "ZEROPAGE"` is active is automatically a zeropage
symbol as far as the assembler and linker are concerned — its address size is
`zp`, and instructions referencing it assemble to the 2-byte zp form. All four
Lynx configs already map this segment (`cfg/lynx.cfg`,`lynx-bll.cfg`,
`lynx-coll.cfg`,`lynx-uploader.cfg`: `ZEROPAGE: load = ZP, type = zp;`).

**The compiler never forces an address width for a static symbol.**
`g_getstatic`/`g_putstatic` in `src/cc65/codegen.c` emit plain `lda _x`,
`ldx _x+1`, etc. (`GetLabelName` just produces `_x`). ca65 then chooses zp vs
absolute from the symbol's own address size. **This means no code-generator
changes are needed** — placement and import/export address size are the whole
job.

**`SC_ZEROPAGE` (0x8000, `src/cc65/symentry.h`) is the compiler's "this symbol
is zp" bit.** It is consumed in exactly two places:
- `EmitExternals` (`src/cc65/symtab.c`) →
  `g_defimport (Name, Flags & SC_ZEROPAGE)` /
  `g_defexport (Name, Flags & SC_ZEROPAGE)`, which emit `.importzp`/`.exportzp`
  instead of `.import`/`.export` (`src/cc65/codegen.c`).
- Today it is *only ever set* by `MakeZPSym` (`src/cc65/symtab.c`), i.e. the
  `zpsym` pragma. That is the single mechanism this design generalises.

**Global storage placement is driven by `Entry->V.BssName`.** For an
uninitialised file-scope object (a tentative definition), `Compile()`
(`src/cc65/compile.c`) records `Entry->V.BssName = GetSegName (SEG_BSS)` (which
`#pragma bss-name` can have changed). `FinishCompile()` later walks the globals
and, for each, switches to that segment and emits the label + `g_res(size)`.
So "put this global in ZEROPAGE" already has a clean hook: set its `BssName` to
`"ZEROPAGE"`.

**Attributes already flow from declaration to symbol.**
`__attribute__((...))` is parsed by `ParseAttribute` (`src/cc65/declattr.c`),
which appends a `DeclAttr` to `D->Attributes`; `SymUseAttr`
(`src/cc65/symentry.c`) then moves the list onto `Sym->Attr` and sets
`SC_HAVEATTR`; `SymHasAttr` queries it. Today the only attributes are
`atNoReturn` and `atUnused`. This is the pipeline the new feature plugs into.

Putting these together: to make a C variable a real zp variable we must do only
two things — (a) for a *definition*, emit its storage into the `ZEROPAGE`
segment; (b) set `SC_ZEROPAGE` on the symbol so cross-module `extern`s import it
as zp. Everything else (addressing form, linker placement, export address size)
falls out of the existing machinery.

## 3. Proposed surface syntax

Add a declaration attribute, `zeropage` (canonical spelling
`__attribute__((zeropage))`), plus a friendly macro so day-to-day code reads
well.

cc65 accepts `__attribute__` only in the **post-declarator** position (the same
place it takes `noreturn`/`unused`), so `__zeropage` is written as a *suffix* on
the declaration, not a prefix.

Definition (one translation unit owns the storage):

```c
unsigned char fastFlag __zeropage;     /* lives in ZEROPAGE, addressed as zp */
int           scanPos  __zeropage;
```

Use from another translation unit:

```c
extern unsigned char fastFlag __zeropage;   /* imported via .importzp */
```

`__zeropage` is a macro supplied by a new tiny header `<zeropage.h>`:

```c
#define __zeropage __attribute__ ((zeropage))
```

Rationale for picking an attribute over the alternatives (see §7): it reuses an
existing, tested parse path; it names the C identifier, not the mangled `_name`;
and the same token works unchanged on both the definition and the `extern`,
which is exactly the symmetry the current workflow lacks.

The raw `__attribute__((zeropage))` form is always available; the macro is sugar
and is what the docs and samples should show.

## 4. Compiler behaviour

### 4.1 Parsing
Register `"zeropage"` in the `Attributes` table in `src/cc65/declattr.c` with a
handler `ZeropageAttr` that adds a new `atZeropage` enumerator (extend
`DeclAttrType` in `src/cc65/declattr.h`). No new keyword, no scanner change.

### 4.2 Setting the symbol flag
When attributes are transferred to the symbol (`SymUseAttr`,
`src/cc65/symentry.c`), if the list contains `atZeropage`, OR `SC_ZEROPAGE`
into `Sym->Flags`. This is the single point that makes both the `extern` import
and the definition export resolve to the zp form:
- `extern` reference → `EmitExternals` sees `SC_EXTERN` + `SC_ZEROPAGE` and
  emits `.importzp _name`. (This alone replaces the entire `zpsym` block — the
  user's headline complaint.)
- external-linkage definition → `EmitExternals` emits `.exportzp _name`,
  consistent with the symbol's zp address size.

### 4.3 Placing the storage (definitions only)
In `Compile()` (`src/cc65/compile.c`), at the point where the tentative
definition's BSS name is captured, if the symbol carries `atZeropage`, force the
name to `SEGNAME_ZEROPAGE` instead of `GetSegName (SEG_BSS)`:

```c
const char* bssName = (Entry->Flags & SC_ZEROPAGE) ?
                      SEGNAME_ZEROPAGE :
                      GetSegName (SEG_BSS);
```

(Testing `SC_ZEROPAGE` rather than re-scanning the attribute list is
equivalent here and is what the as-built code does, since `SymUseAttr` has
already mapped the attribute onto the flag and the function/typedef guard has
already cleared it where it doesn't belong.)

`FinishCompile()` then emits `.segment "ZEROPAGE"` + label + `.res size`
through the unchanged existing path, and ca65's built-in zeropage address size
for that segment makes the label a true zp symbol locally — so even a `static`
(no-linkage) zp variable is addressed as zp within its own module without any
export at all.

That is the entire functional change: one attribute registration, one flag OR,
one ternary. No `codegen.c` change.

## 5. Validation and diagnostics

The attribute is rejected where it cannot mean anything, with clear errors
rather than silent miscompilation. As built:

- **Functions and typedefs** — handled in `Compile()` right after `SymUseAttr`:
  if the entry is a function type or `SC_TYPEDEF`, emit
  `'zeropage' attribute is not valid here` and clear the inherited
  `SC_ZEROPAGE` so no `.importzp`/`.exportzp` is produced for a code symbol.
- **Local (block-scope) objects** — `ParseOneDecl()` (`src/cc65/locals.c`)
  checks `DeclHasAttr (&Decl, atZeropage)` and emits
  `'zeropage' attribute is only valid at file scope`. Block-scope declarations
  never run through `SymUseAttr`, so the attribute would otherwise be silently
  dropped. **Deviation from the first draft:** block-scope `static __zeropage`
  is *also* rejected, not accepted — applying the attribute inside the local
  sub-parsers (`ParseStaticDecl`/`ParseAutoDecl`) is extra wiring with little
  payoff, and a file-scope `static __zeropage` covers the same need. A
  module-private zp object should be declared `static` at file scope.
- **Initialisers** — `Compile()` errors with
  `'zeropage' variable '%s' cannot have an initializer`. The `ZEROPAGE` segment
  is `type = zp` with no load/run copy, mirroring the assembly reality that a
  `.res` cell in `.zeropage` cannot carry initialised data. Zeroing happens only
  if the runtime clears zero page; do not promise it. (If initialised zp is ever
  wanted it is a separate, larger feature: a zp `DATA`-style segment with a
  run/load copy in startup. Out of scope here.)
- **Address-size consistency** — if one TU declares the symbol `__zeropage` and
  another does not, ld65 raises an address-size mismatch. The fix is to put the
  attribute on the shared `extern` in a header so every user agrees.

## 6. Cost, capacity, and the runtime

Zero page is 256 bytes and is shared with the cc65 runtime, which already
imports `sp, sreg, regsave, regbank, tmp1..tmp4, ptr1..ptr4` (see the
`.importzp` lines in `src/cc65/codegen.c` startup emission) plus whatever the
Lynx libraries reserve. The config's `ZP` memory area bounds the `ZEROPAGE`
segment; over-allocation surfaces as a hard ld65 error
("`ZEROPAGE` segment overflow") — a link-time failure, never a silent wrap.
Guidance for the docs: use `__zeropage` for a handful of hot scalars and
pointers, not arrays or structs; every byte spent here is a byte the runtime and
libraries can't use.

## 7. Alternatives considered

- **A prefix keyword `__zeropage__`** (recognised by the scanner like
  `__fastcall__`). Originally deferred in favour of the attribute, but
  **subsequently added as an equivalent alias** (2026-06-18, see §11) because
  the prefix form reads more naturally — `__zeropage__ int scanPos;` announces
  the placement before the name. It is the double-underscore spelling so it
  never collides with a user identifier; bare `zeropage` was deliberately *not*
  reserved, to avoid breaking existing code that uses it as a name. The keyword
  folds into the same `SC_ZEROPAGE` flag as the attribute, so the two share all
  downstream handling.
- **A dedicated pragma, e.g. `#pragma zpvar (push)` … `(pop)`.** Still a
  stateful push/pop region — the very ergonomics problem being removed — and it
  can't ride along on a single `extern` line. Rejected.
- **Auto-promote `register` file-scope variables to zp.** Overloads a standard
  keyword with a target-specific meaning and silently changes semantics of
  existing code. Rejected.
- **Leave `bss-name` + `zpsym`, just document it better.** Doesn't remove the
  `.s` file requirement for the canonical case, keeps the underscore/ordering
  traps, and keeps the no-op `bss-name` wrapper around `extern`s. Rejected.

The chosen attribute keeps `bss-name`/`zpsym` working untouched for anyone who
relies on them — this is purely additive.

## 8. Documentation updated alongside the code

Per the repo rule that docs track code in the same pass, the following were
updated when the feature landed:

- New header `include/zeropage.h` (the `__zeropage` macro) with a full doc
  comment covering usage and restrictions. No `asminc` counterpart: the feature
  is C-side only (assembly already has `.zeropage`/`.importzp`).
- `doc/cc65.html`: new §8.1 "The `__zeropage` specifier" in the Variable storage
  chapter (documents both the attribute and the `__zeropage__` keyword), a table
  row for `__zeropage` globals, a TOC entry, and a cross-reference from the
  `#pragma zpsym` section (§7.20).
- `examples/zeropage/zeropage.c`: a runnable sample with three `__zeropage` counters shown
  through the TGI text harness; added to `examples/Makefile` (`EXELIST_lynx`) and
  to `doc/samples.html` under a new "Compiler features" heading.
- `doc/funcref.html` was intentionally **not** touched: it is a per-function
  reference and `zeropage.h` declares a macro, not functions.

## 9. Summary

One attribute, two hook points, zero codegen changes:
`__attribute__((zeropage))` (sugar: `__zeropage`) registered in `declattr.c`;
OR `SC_ZEROPAGE` in `SymUseAttr`; and, for definitions, force the tentative
def's segment to `ZEROPAGE` in `Compile()`. The assembler's built-in zeropage
segment and the compiler's existing import/export-address-size path do the rest.
A single C token then replaces both the companion `.s` file and the
`bss-name`/`extern`/`zpsym` block, and reads the same on a definition and on an
`extern`.

## 10. Verification (2026-06-18)

Built `bin/cc65` clean. Compiled a two-module test:

- **Definition TU** — `_fastFlag`, `_scanPos`, `_privCounter` emitted under
  `.segment "ZEROPAGE"`; the external ones exported with `.exportzp`, the plain
  global `_normalVar` left in `.segment "BSS"` with `.export`.
- **Reference TU** — `extern ... __zeropage` produced `.importzp _fastFlag`
  versus `.import _normalVar` for the untagged global.
- **Opcode-level proof** — assembling with a `ca65 -l` listing showed the zp
  variables using 2-byte zero-page opcodes (`inc` = `E6`, `adc` = `65`,
  `sta` = `85`, `dec` = `C6`) while `_normalVar` used the 3-byte absolute
  `inc` = `EE rr rr`.
- **Diagnostics** — all four rejections fire with the expected messages:
  initializer, function, typedef, and block-scope local.
- **Link** — the three modules linked against `lynx.lib` with `cfg/lynx.cfg`
  into a valid `.lnx` (no ZEROPAGE overflow).
- **Sample** — `examples/zeropage/zeropage.c` compiles, assembles and links clean
  (zp placement confirmed in its `.s`). Emulator/hardware run pending, in line
  with the other samples in the tree.
- **Regression** — an existing sample (`breakout.c`) still compiles and
  assembles unchanged.

## 11. The `__zeropage__` prefix keyword (2026-06-18)

A prefix keyword equivalent to the attribute, for declarations that read better
with the placement up front:

```c
__zeropage__ unsigned char fastFlag;        /* definition */
static __zeropage__ unsigned char privA;    /* either order with storage class */
__zeropage__ static unsigned char privB;
extern __zeropage__ unsigned char fastFlag; /* reference -> .importzp */
```

`__zeropage__` and `__attribute__((zeropage))` are interchangeable and may even
appear together (idempotent).

### 11.1 As-built hook points

- **Token** — `TOK_ZEROPAGE` added to `token_t` (`src/cc65/scanner.h`), placed
  after the function specifiers and outside the storage-class/type/qualifier
  ranges (those ranges have no callers, but keeping it out of them avoids any
  future range arithmetic surprise). Keyword `{ "__zeropage__", TOK_ZEROPAGE }`
  inserted in the alphabetically-sorted `Keywords[]` table in
  `src/cc65/scanner.c` (the table is `bsearch`ed, so order matters; it sorts
  between `__near__` and `asm`). Only the double-underscore spelling is
  reserved.
- **Parsing** — `__zeropage__` is orthogonal to the storage class (you want
  `static __zeropage__`), so it is not a slot in `ParseStorageClass`'s switch.
  A small `ParseZeropageSpec()` helper consumes a run of the keyword, and
  `ParseDeclSpec()` (`src/cc65/declare.c`) calls it both *before* and *after*
  `ParseStorageClass`, then folds the result into the storage class as
  `SC_ZEROPAGE`. Position is the same as a storage class — before the type, like
  cc65's existing `static int` (not `int static`) ordering.
- **Reuse** — because the keyword arrives as `SC_ZEROPAGE` inside the
  declaration's storage class, `AddGlobalSym` copies it onto the symbol
  (`Entry->Flags |= Flags`) with no new code, and the §4.2/§4.3 export/import
  and segment-placement logic, plus the §5 validation, all fire unchanged. The
  one validation that needed widening is the block-scope check in
  `ParseOneDecl` (`src/cc65/locals.c`): it now rejects either form
  (`Decl.StorageClass & SC_ZEROPAGE` **or** the attribute). Diagnostic wording
  was made form-neutral ("`'zeropage' is not valid here`",
  "`'zeropage' is only valid at file scope`").

### 11.2 Verified

`__zeropage__` in all three orderings (`__zeropage__ T`, `static __zeropage__ T`,
`__zeropage__ static T`) lands in `.segment "ZEROPAGE"` with `.exportzp`;
`extern __zeropage__` emits `.importzp`; the `ca65 -l` listing shows the keyword
vars using 2-byte zp opcodes (`inc` = `E6`, `dec` = `C6`) versus `EE rr rr`
absolute for the untagged control global; all four diagnostics fire with the
neutral messages; the suffix attribute and a combined keyword+attribute
declaration still compile; and `breakout.c` is an unchanged-output regression.
