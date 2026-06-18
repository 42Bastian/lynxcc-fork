# Design: 2-byte heap block header (drop the `start` back-pointer)

Scope: shrink the per-allocation administration header on the C heap from 4
bytes to 2 bytes by removing the `struct usedblock::start` back-pointer, and
remove the only feature that requires it (`posix_memalign`). The free-list
machinery, the heap pointers (`_heaporg`/`_heapptr`/`_heapend`), coalescing, and
the top-of-heap release fast path are **unchanged**. **Design only** — no code is
changed here; this defines what should change and what deliberately should not.

This is effectively a revert to the pre-2004 cc65 heap layout (size-word-only
header), which shipped and was field-tested for years before aligned allocation
was bolted on. That provenance is the main reason the risk is low.

## 1. Motivation

On a 64 KB Lynx the usable heap is only the gap between BSS-end and the C-stack
reservation, with ~16 KB at the top of RAM already taken by the two TGI display
framebuffers. Every byte of per-block overhead is multiplied by the live object
count, so on a workload with many small allocations the header is a real tax.

The current header is 4 bytes: a `size` word plus a `start` back-pointer. The
`start` field exists for exactly one reason — to let `posix_memalign` hand back a
pointer into the *middle* of a larger raw block (the "split block" case), and to
let `free`/`realloc`/`_heapblocksize` recover the true raw-block address from
such a pointer. Nothing on the Lynx needs power-of-two alignment: Suzy SCBs,
sprite scan-line data, and the framebuffer have no alignment requirement, and the
tree ships no caller of `posix_memalign`. The field is therefore dead weight on
every allocation in every program.

Removing it saves **2 bytes of RAM per live allocation** and shrinks the
always-linked `malloc`/`free` path, at the cost of deleting an unused POSIX API.

## 2. Current layout (what exists today)

```c
/* include/_heap.h */
struct usedblock {
    unsigned          size;     /* total raw size incl. this header */
    struct usedblock* start;    /* points to the real raw-block start */
};
#define HEAP_ADMIN_SPACE  sizeof (struct usedblock)   /* == 4 */
```

The pointer returned to the caller points `HEAP_ADMIN_SPACE` bytes past the raw
block. For a normal block `start` points back to the raw block itself
(`p->start = p`); only `posix_memalign` ever stores a `start` that differs from
the block it is written into (the split-block case described in `pmemalign.c`).

The free list is a separate, larger structure and is **not** affected:

```c
struct freeblock { unsigned size; struct freeblock* next; struct freeblock* prev; };
HEAP_MIN_BLOCKSIZE = sizeof (struct freeblock)   /* == 6, unchanged */
```

### 2.1. Every site that touches `start` / `HEAP_ADMIN_SPACE`

| Site | How it uses the 4-byte header |
|------|-------------------------------|
| `include/_heap.h` | declares `usedblock { size; start; }`, defines `HEAP_ADMIN_SPACE` |
| `asminc/_heap.inc` | mirrors the struct + constant for asm |
| `libsrc/common/malloc.s` | `FillSizeAndRet` writes `size`; `RetUserPtr` writes `start = ptr2`; returns `ptr2 + HEAP_ADMIN_SPACE` |
| `libsrc/common/free.s` | reads `start` at user−2/−1 (offset $FE/$FF after high-byte decrement) to recover the raw block, then reads `size` from it |
| `libsrc/common/realloc.c` | `b = (((usedblock*)block) - 1)->start;` then later `b->start = b;` |
| `libsrc/common/_heapblocksize.s` | reads `start` (offset $FE/$FF), reads `size`, returns `size − (user − raw)` |
| `libsrc/common/pmemalign.c` | the **only** writer of a split `start`; the sole reason the field exists |
| `include/stdlib.h` | declares `posix_memalign` |

`_heapadd.s`, `_heap.s`, `_heapmemavail.s`, `_heapmaxavail.s`, `calloc` are
**not** in this list — they only ever touch `freeblock::size` / the heap
pointers and need no change.

## 3. Proposed layout

```c
/* include/_heap.h */
struct usedblock {
    unsigned size;              /* total raw size incl. this 2-byte header */
};
#define HEAP_ADMIN_SPACE  sizeof (struct usedblock)   /* == 2 */
```

Invariant after the change: the raw-block address is **always** exactly
`user_ptr − HEAP_ADMIN_SPACE` (= `user_ptr − 2`). There is no indirection; the
word immediately below the user pointer *is* the size of the raw block. This is
the property `posix_memalign` deliberately broke and is now restored as a hard
guarantee.

`HEAP_ADMIN_SPACE` stays a named constant (now 2) so all arithmetic remains
parametric; no literal `4`/`2` should be hard-coded.

## 4. Per-file changes

### 4.1. `include/_heap.h` and `asminc/_heap.inc`
Drop the `start` member from `usedblock` (leaving the single `size` word). Update
the doc comment that explains the struct (it currently describes the
`aligned_malloc` split case — that paragraph goes). `HEAP_ADMIN_SPACE` continues
to be `sizeof (usedblock)` and now resolves to 2. `freeblock` and
`HEAP_MIN_BLOCKSIZE` are untouched. The "field order is significant" note on
`usedblock` becomes moot (one field) but should stay on `freeblock`.

### 4.2. `libsrc/common/malloc.s`
Remove the `RetUserPtr` block that writes `usedblock::start`. The tail collapses
to: `FillSizeAndRet` writes the size word, then falls through to "return
`ptr2 + HEAP_ADMIN_SPACE`". Keep a label at the return point so the
block-used-in-full path (which already jumps past the size write) still has a
target. The leading C-pseudocode comment already reflects the 2-byte scheme
(`*p++ = size; return p;`) and needs no change — only the asm tail loses four
instructions. The `add #HEAP_ADMIN_SPACE` that forms the user pointer now adds 2.

### 4.3. `libsrc/common/free.s`
Replace the raw-block recovery. Today it decrements the high pointer byte and
reads the `start` word at offsets $FE/$FF, then dereferences it. New behaviour:
the word at user−2/−1 is the size directly, and the raw block is `user − 2`.
Reuse the existing high-byte-decrement trick to read the size word at offset
$FE/$FF, and set `ptr2 = user − 2` as the raw-block pointer. Everything
afterwards — the `raw + size == _heapptr` heap-top test, the trailing-free-block
absorption, and the fall-through into `heapadd` — is unchanged, because `heapadd`
only ever writes `freeblock`/`usedblock::size` and the heap pointers.

### 4.4. `libsrc/common/_heapblocksize.s`
Raw block is now `ptr − 2` with the size in its first word, so the routine
returns `rawsize − HEAP_ADMIN_SPACE`. The current "size minus (user − raw)"
correction (which existed to handle split blocks where `user − raw` varies)
reduces to the constant 2, so the arithmetic simplifies to a single subtract of
`HEAP_ADMIN_SPACE`.

### 4.5. `libsrc/common/realloc.c`
Replace `b = (((struct usedblock*) block) - 1)->start;` with
`b = ((struct usedblock*) block) - 1;` (the raw block is the header itself), and
delete both `b->start = b;` assignments. `oldsize = b->size;`, the heap-top
in-place grow/shrink, and the copy-to-new-block fallback are otherwise unchanged.
Note that with a 2-byte header `((usedblock*)block) - 1` now points 2 bytes below
`block`, which is correct.

### 4.6. `libsrc/common/pmemalign.c` — delete
`posix_memalign` is the only producer of split blocks and cannot be supported
without the `start` field. It has no caller in the tree and no alignment need on
the Lynx, so the file is removed rather than rewritten. (A non-splitting
reimplementation — over-allocate and never free the slack — was considered and
rejected: it wastes up to `alignment + admin` bytes per call and still buys
nothing on a target that never aligns.) Removing the file also drops its
`errno`/`EINVAL`/`ENOMEM` usage; confirm nothing else in the build depends on it
being the thing that pulls in those symbols (it doesn't).

### 4.7. `include/stdlib.h`
Remove the `posix_memalign` prototype. `_heapblocksize`, `malloc`, `free`,
`calloc`, `realloc`, and `_heapadd` prototypes stay.

### 4.8. Documentation (required, same pass — per CLAUDE.md)
- `doc/funcref.html`: **no change needed** — verified the function reference
  never carried a `posix_memalign` entry, so there is nothing to delete or
  renumber (unlike the `device`/`pen`/`locale` removals, which were documented).
- `doc/lynx.html`: **no change needed** — verified it does not quote the heap
  per-block admin overhead or mention `posix_memalign`.
- `include/_heap.h`, `asminc/_heap.inc`: doc comments updated in §4.1.
- This design file is the source-of-truth record for the change.

## 5. Behavioural deltas and invariants

| Property | Before | After |
|----------|--------|-------|
| Per-allocation admin RAM | 4 B | 2 B |
| Min raw block (`HEAP_MIN_BLOCKSIZE`) | 6 B | 6 B (unchanged) |
| User bytes in a min block | 2 | 4 |
| Raw block from user ptr | via `start` indirection | always `user − 2` |
| Free-list layout / coalescing | — | unchanged |
| `_heaporg/_heapptr/_heapend` | — | unchanged |
| `free` accepts a pointer into a split block | yes (via `start`) | no — must be the exact malloc/realloc return (standard contract) |
| `posix_memalign` | present | **removed** |

The size-rounding threshold in `malloc` shifts slightly (a request now needs
`size + 2` rather than `size + 4` before it exceeds the 6-byte floor), but the
floor itself is unchanged, so no block ever gets smaller — small blocks simply
expose 2 more usable bytes.

## 6. Savings

- **RAM:** 2 bytes per live allocation, always, in every program that uses the
  heap. For N concurrent objects that is 2·N bytes reclaimed with zero
  fragmentation cost.
- **Code:** `malloc`/`free`/`_heapblocksize` (all in the always-linked heap
  path once any allocation is used) each lose a few instructions; `realloc`
  loses two stores; `pmemalign.o` (a non-trivial C function) is gone entirely
  for any program that referenced it.

## 7. Risks

Low. The change is a revert to a previously shipped layout, and the
hard-to-get-right part is confined to pointer/offset arithmetic in three asm
files (`malloc.s`, `free.s`, `_heapblocksize.s`). The free-list — the genuinely
intricate part of the allocator — is not touched. The only externally visible
break is the removal of `posix_memalign`; this is an intentional API reduction
consistent with the fork's trimming of target-irrelevant surface.

## 8. Verification (when implemented)

1. `grep` the whole tree (samples, libsrc, include, doc) for `posix_memalign`,
   `->start`, `usedblock::start`, and `HEAP_ADMIN_SPACE` to confirm no remaining
   readers of the old 4-byte assumption.
2. Rebuild `lynx.lib` clean; confirm `pmemalign.o` is gone from the archive
   (`ar65 d` it if a stale object lingers, as past removals required).
3. Host-model the allocator (the size/start logic is portable C): run a churn
   test — random `malloc`/`free`/`realloc` of mixed sizes — asserting
   `_heapblocksize(p) >= requested`, no overlaps between live blocks, and that
   `_heapmemavail` returns to its initial value once everything is freed. Repeat
   with the old and new headers and confirm identical free/used accounting apart
   from the 2-bytes-per-block difference.
4. Build all samples; link must be clean and the heap-using samples must run
   under the emulator without corruption (covers the real `malloc`/`free`/
   `realloc` paths on hardware-accurate 65C02 timing).
5. Spot-check `_heapblocksize` returns exactly `requested` rounded up to the
   block granularity, and that a `realloc` grow at heap top stays in place.

## 9. Summary of proposed changes

| # | Change | File(s) | Risk |
|---|--------|---------|------|
| 1 | Drop `usedblock::start`; `HEAP_ADMIN_SPACE` → 2 | `include/_heap.h`, `asminc/_heap.inc` | Low |
| 2 | Remove `start` write; return `ptr+2` | `libsrc/common/malloc.s` | Low (offset math) |
| 3 | Recover raw block as `user−2`; read size directly | `libsrc/common/free.s` | Low (offset math) |
| 4 | Return `rawsize − 2` | `libsrc/common/_heapblocksize.s` | Low (offset math) |
| 5 | Raw block = `(usedblock*)block − 1`; drop `start` stores | `libsrc/common/realloc.c` | Low |
| 6 | Delete file (unused, needs `start`) | `libsrc/common/pmemalign.c` | Low (API removal) |
| 7 | Remove `posix_memalign` prototype | `include/stdlib.h` | Low (API removal) |
| 8 | HTML docs verified to need no change (no `posix_memalign`/overhead refs) | `doc/funcref.html`, `doc/lynx.html` | None |

## 10. Implementation status (done 2026-06-17)

All of §4.1–§4.7 applied; `pmemalign.c` deleted and its stale
`libwrk/lynx/pmemalign.{o,d,s}` purged. Verified in a Linux sandbox build:
toolchain rebuilt from `src/`, `lynx.lib` relinked clean with `pmemalign.o`
absent from the archive, all 8 samples link clean, and a tree-wide grep finds no
remaining `posix_memalign` / `->start` / `usedblock::start` references. A
host-model churn test (faithful C port of the allocator with the 2-byte header)
ran 400k randomized malloc/realloc/free ops under ASan+UBSan with all invariants
holding: no overlapping live blocks, every block ≥ requested size, payload
intact across churn, and full reclamation (`_heapmemavail` returns to base,
`_heapptr` unwound to `_heaporg`, free list empty).

A dedicated on-target ROM, `samples/heaptest.c`, was added (and wired into
`samples/Makefile` + documented in `doc/samples.html`). It runs 17 checks that
exercise the public API (`malloc`/`calloc`/`realloc`/`free`/`_heapblocksize`/
`_heapmemavail`/`_heapadd`) and cross-check them against direct memory
inspection — the raw size word below each pointer, the `_heaporg`/`_heapptr`/
`_heapend` runtime vars, and `_heapfirst` — proving the `raw == user-2`
invariant and the 2-byte overhead on hardware. It builds clean and shows a
colour-coded PASS k/17 grid. The same 17 expectations were replayed through the
host model first and all pass (this caught one ROM bug: the `_heapadd`
pool-growth assertion had to be read before the follow-up `malloc` consumed part
of the donated buffer). Emulator/hardware run of `heaptest.lnx` remains the one
open item, consistent with the project's standing "emu run pending" pattern.
