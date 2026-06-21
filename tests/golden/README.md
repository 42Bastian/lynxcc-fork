# Golden reference output

One file per integration example: `<name>.sha256` holds the SHA-256 of the PNG
screenshot captured after the fixed warm-up frames (see `../integration`). The
example path separator is encoded as `__` (e.g. `suzy__spritetest.sha256`).

These hashes are tied to both the example binaries and the pinned GearLynx
build. Regenerate them deliberately with
`python3 tests/integration/gearlynx_check.py --update` and review the diff — an
unexpected change is a regression, not noise.
