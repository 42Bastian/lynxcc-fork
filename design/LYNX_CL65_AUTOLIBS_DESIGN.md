<!--
SPDX-License-Identifier: CC-BY-4.0
Lynx Game Development SDK documentation, (c) 2026 the lynxcc authors,
licensed under Creative Commons Attribution 4.0 International.
See doc/licenses.html.
-->

# cl65 Automatic SDK Library Resolution Design

Status: **IMPLEMENTED** (2026-06-21). This document is the source of truth for
how `cl65` automatically links the optional SDK subsystem libraries
(`lynx-graphics`, `lynx-audio`, `lynx-compress`, `lynx-math`) without the user
naming them on the link line. It realises §6.6 of
`design/LYNX_SDK_LAYOUT_DESIGN.md` (phase 6 of §13) and records the completed
implementation.

## 1. Goal

A user writes

```
cl65 game.c -o game.lnx
```

and whatever graphics, audio, math or compression code the program references is
linked **automatically** — no `-l` flags — while subsystems it does not touch
cost nothing. The fully-explicit link path stays available for users who want
it.

## 2. Why this is cheap

`ld65` extracts only the referenced object modules from any `.lib` it scans, so
offering it a library that a program never calls into contributes **zero bytes**
to the output. "Automatic" therefore reduces to *"`cl65` always offers `ld65`
the full SDK set, in the right order."* Before this change `cl65` already
appended exactly one default library for the target (`lynx.lib`); this
generalises that single default into an ordered list.

Output is byte-for-byte identical to hand-listing exactly the libraries a
program needs. This was verified for every example: linking a given object file
via `cl65` auto-libs and via the previous explicit `ld65 … lynx-graphics.lib
lynx-audio.lib lynx-compress.lib lynx-math.lib lynx.lib` invocation produces the
same `.lnx`.

## 3. The manifest

`cl65` learns the optional-library set from a data file, not hard-coded names,
so new or out-of-tree libraries do not require recompiling `cl65`. The
`libraries/` build (`libraries.mk`) emits `lib/lynx-sdklibs.list`, one archive
per line, in link order — **dependents first, core last**:

```
lynx-graphics.lib
lynx-audio.lib
lynx-handymusic.lib
lynx-compress.lib
lynx-math.lib
lynx-sdcard-gd.lib
lynx.lib
```

(`lynx-handymusic.lib` is the opt-in HandyMusic BGM+SFX engine, parallel to and
mutually exclusive with `lynx-audio.lib`; see
`design/LYNX_HANDYMUSIC_DESIGN.md`.)

Blank lines and lines beginning with `#` are ignored, so the file may carry
comments.

The manifest filename is fixed (`lynx-sdklibs.list`). It is located via the same
`lib/` search `ld65` uses for archives (§2 of the layout design): the `LD65_LIB`
environment variable, `$CC65_HOME/lib`, the compiled-in `LD65_LIB` default, and
the WinBin `../lib` fallback. No new path logic is introduced. If the manifest
is absent, `cl65` falls back to today's behaviour (append only `lynx.lib`), so
the feature degrades gracefully.

## 4. Link-line assembly

At the link step for the Lynx target, `cl65` appends the manifest entries — in
listed order, after all user object files and libraries — instead of appending
only `lynx.lib`. Because the order is dependents-first with core last, `ld65`'s
single in-order library pass resolves every cross-reference
(`graphics → math → core`, `audio → core`, `compress → libc`; see §6.4 of the
layout design).

User-supplied `-l`/explicit `.lib` arguments are placed *before* the manifest
set (they are added to the linker file list as the command line is parsed) and
are **de-duplicated** against it: when a manifest entry has the same base name as
an archive the user already named, the manifest entry is skipped, so a user can
pin or override a library. The de-duplication compares base names, so a pinned
`path/to/lynx-audio.lib` still suppresses the manifest's `lynx-audio.lib`.

## 5. New `cl65` options

| Option | Effect |
| --- | --- |
| `--no-sdk-libs` | Append only the core `lynx.lib`, not the optional set. Restores the fully explicit `-l` workflow (the §6.2 legibility path). |
| `--sdk-libs <file>` | Use an alternative manifest (e.g. one that adds an out-of-tree library) for projects that extend the SDK. |

These mirror the spirit of cc65's existing library/target-lib switches. An
explicit `--sdk-libs` file that cannot be opened is a hard error; a missing
auto-discovered manifest is not (it triggers the graceful core-only fallback in
§3). `--no-target-lib` still suppresses *all* SDK libraries, core included, and
takes precedence.

## 6. Acyclicity contract

Automatic resolution relies on the SDK libraries forming a DAG (no two optional
libraries mutually reference each other) so one ordered pass suffices. §6.4 of
the layout design establishes this; the manifest's fixed order encodes it. A
future library that would introduce a cycle must instead be merged or refactored
— recorded in that library's design note.

## 7. What does not change

No change to `ld65`, the object format, or the libraries themselves: this is a
`cl65` driver change plus one shipped manifest file. `ld65` invoked directly
still needs explicit `-l`/`.lib` arguments — the convenience lives in `cl65`,
preserving the fully-explicit low-level path.

## 8. Touch points (implemented)

- `compiler/cl65/main.c`: the `--no-sdk-libs` / `--sdk-libs` long options, the
  `FindSDKManifest` / `LibAlreadyListed` / `AppendSDKLibs` helpers, and the
  `Link()` change that appends the manifest set instead of only `TargetLib`.
- `libraries.mk`: the `lib/lynx-sdklibs.list` rule (variable `MANIFEST_BODY`),
  wired into the `all`/`lib` target so the manifest ships in `lib/`.
- `examples/Makefile`: examples now link via `cl65` with no `-l` flags
  (the `SDK_LIBS` variable and the explicit `ld65` link rule are gone).
- `doc/cl65.html`: documents both new options and the auto-libs behaviour.

## 9. Risks

Because `cl65` offers every SDK library by default, a program never sees a
"missing `-l`" error for a shipped subsystem — the linker just resolves it. That
is the intended convenience, but it means a genuine dependency mistake only
surfaces under `--no-sdk-libs`. The integration tests (§10 of the layout design,
phase 7) should run at least one build with `--no-sdk-libs` plus explicit `-l`
to keep the explicit path exercised; see §15 of `LYNX_SDK_LAYOUT_DESIGN.md`.
