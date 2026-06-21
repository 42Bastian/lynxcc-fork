# Tests

Automated verification for the Lynx Game Development SDK. This formalises the
practices that used to be ad-hoc (host math sweeps, GearLynx framebuffer 0-diff
checks) into a single repeatable gate, driven by `run.sh` and wired into CI.

```
tests/
├── run.sh          # entry point: unit then integration
├── unit/           # host-built C checks (no emulator) — always run
├── integration/    # boot examples on GearLynx, diff screenshot vs golden
├── golden/         # committed reference screenshot hashes
└── emu/gearlynx/   # the headless emulator + MCP harness (NOT shipped)
```

## Running

```bash
make              # from the repo root: builds toolchain + libs + examples
make tests        # or directly: tests/run.sh

tests/run.sh --frames 120          # step longer before the integration capture
tests/run.sh lynxdemo suzy/spritetest   # integration: only these examples
```

`run.sh` runs the **unit** tests first — ordinary host programs that finish in
milliseconds and need nothing but a C compiler. It then runs the **integration**
tests, which boot each built example `.lnx` on the headless GearLynx emulator,
step a fixed number of frames with no input, screenshot, and compare a SHA-256
of the PNG against `golden/`.

## The emulator is optional

GearLynx, its SDL libraries and the Lynx BIOS are **not shipped** (see
`emu/gearlynx/README.md`). When any are missing the integration step prints
`SKIP` and exits 0, so CI runners without the emulator stay green on the unit
tests. Locally — and in the sandbox where GearLynx is present — the integration
tests run for real.

## Regenerating goldens

The golden hashes are tied to the example binaries **and** the pinned emulator
build. When you intentionally change an example's output (or bump GearLynx),
refresh them:

```bash
python3 tests/integration/gearlynx_check.py --update
```

Review the diff before committing — a changed hash with no intended visual
change means a real regression.

## Not shipped

`emu/gearlynx/` is a developer/CI tool and is excluded from SDK release
artifacts (`design/LYNX_SDK_LAYOUT_DESIGN.md` sec. 11); the whole `tests/` tree
is omitted from `make install`/`zip`.
