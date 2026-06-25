<!--
SPDX-License-Identifier: MPL-2.0

This Source Code Form is subject to the terms of the Mozilla Public License,
v. 2.0. If a copy of the MPL was not distributed with this file, You can
obtain one at https://mozilla.org/MPL/2.0/.

Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.
-->

# Golden reference output

One file per integration example: `<name>.sha256` holds the SHA-256 of the PNG
screenshot captured after the fixed warm-up frames (see `../integration`). The
example path separator is encoded as `__` (e.g. `suzy__spritetest.sha256`).

These hashes are tied to both the example binaries and the pinned GearLynx
build. Regenerate them deliberately with
`python3 tests/integration/gearlynx_check.py --update` and review the diff — an
unexpected change is a regression, not noise.

## License

The `*.sha256` files are single-line digests that cannot carry a comment without
breaking the test reader, so they have no in-file SPDX tag. Per the Mozilla
Public License's Exhibit A, this README is the "LICENSE file in a relevant
directory" that supplies their notice: the golden data files are part of the
Lynx Game Development SDK and licensed under the **Mozilla Public License,
v. 2.0** (`SPDX-License-Identifier: MPL-2.0`). See `doc/licenses.html` and
`design/LYNX_LICENSE_POLICY_DESIGN.md`.
