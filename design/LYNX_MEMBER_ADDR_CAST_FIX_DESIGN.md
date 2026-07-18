<!--
SPDX-License-Identifier: CC-BY-4.0
Lynx Game Development SDK documentation, (c) 2026 the lynxcc authors,
licensed under Creative Commons Attribution 4.0 International.
See doc/licenses.html.
-->

# Design: fix for the member-address subscript miscompile (ArrayRef)

Scope: a code-generation bug in the C compiler (`compiler/cc65/expr.c`,
`ArrayRef`), inherited from the upstream cc65 2.19 code this fork is based
on; found 2026-07-18 while implementing the raycaster's §9.1 byte-store
optimization (design/LYNX_RAYCASTER_DRAW_DESIGN.md §9.4) and **fixed the same
day**. This document is the source of truth for the bug's shape, the root
cause, the fix, and the regression coverage.

## 1. Symptom

Subscripting the *address of a struct member reached through a pointer* with
a **constant** index compiled to an access through the struct pointer itself:
the member offset silently vanished.

```c
typedef struct { unsigned char a, b, c; unsigned int u, vsize; } S;

void f (S* s, unsigned char v)
{
    ((unsigned char*)&s->vsize)[1] = v;   /* intended: byte at s+5+1      */
}
```

generated (any `-O` level — this is a front-end bug, not an optimizer one):

```asm
        lda     s               ; pointer value, no +5 anywhere
        sta     ptr1  ...
        ldy     #$01
        sta     (ptr1),y        ; writes ((unsigned char*)s)[1] = sprctl1!
```

The write landed at struct offset 1 instead of 6. Every flavour of the
pattern was affected the same way:

- writes *and* reads: `v = ((unsigned char*)&s->vsize)[1];` misread s+1;
- the cast is not required: `(&s->u)[1] = v;` (int elements) dropped the
  `offsetof(S, u)` too and stored at `s + 1*sizeof(int)`;
- any constant subscript, any member, any `-O` level.

Not affected (these were always correct, which is what made the bug easy to
miss):

- **variable** subscripts: `((unsigned char*)&s->vsize)[k]` — the non-constant
  path materializes the pending offset before adding the index;
- explicit pointer arithmetic: `*((unsigned char*)&s->vsize + 1)`;
- a two-step pointer: `p = (unsigned char*)&s->vsize; p[1] = v;`;
- struct **objects** (not pointers): `((unsigned char*)&gs.vsize)[1]` — the
  constant-base-address path folds `_gs+5` correctly;
- plain member access `s->vsize` and ordinary `p[const]` on a bare pointer.

In the raycaster this zeroed every wall sprite's height (the store aimed at
`vsize`'s high byte hit `sprctl1` at offset 1), producing a wall-less frame —
a very indirect symptom of a compiler bug.

## 2. Root cause

cc65's expression descriptors carry a *pending* constant offset next to a
value that lives in the primary register: for `E_LOC_EXPR`, the effective
address is "primary + `IVal`". Three steps set the trap:

1. `StructRef` (`expr.c`), for `s->vsize` with `s` loaded in the primary,
   records the member displacement as that pending offset:
   `Expr->IVal += Field->V.Offs;` — deliberately deferred so that a chain
   like `s->a.b.c` folds into one displacement.
2. Unary `&` (`hie10`) just retypes the lvalue as a pointer rvalue
   (`ED_MakeRVal`) — the descriptor still says "primary + IVal", which is
   exactly right for an address value. A cast changes only the type.
3. `ArrayRef` (`expr.c`), in its constant-subscript branch for
   pointer-typed operands, ended with:

   ```c
   if (ConstBaseAddr || ED_IsLVal (Expr)) {
       LoadExpr (CF_NONE, Expr);
       ED_MakeRValExpr (Expr);
   }
   /* Use the offset */
   Expr->IVal = Subscript.IVal;      /* <-- plain assignment */
   ```

   For the guarded cases the assignment is fine: `LoadExpr` has just
   materialized everything and `ED_MakeRValExpr` cleared `IVal`. But when the
   operand is *already* a pointer rvalue in the primary — precisely what step
   2 produced — neither call runs, the pending member offset is still in
   `IVal`, and the assignment **overwrites it** with the scaled subscript.
   The member displacement is never emitted.

The variable-subscript path never had the problem because it calls
`LoadExpr` on the base first, which emits the pending `+offset` (an
`incax5`-style add) before the index addition — that is why `[k]` worked
while `[1]` did not.

## 3. The fix

`compiler/cc65/expr.c`, `ArrayRef`, constant-subscript pointer branch: keep
the assignment in the guarded arm (where `IVal` is provably clean) and *add*
in the already-in-primary arm:

```c
if (ConstBaseAddr || ED_IsLVal (Expr)) {
    LoadExpr (CF_NONE, Expr);
    ED_MakeRValExpr (Expr);
    Expr->IVal = Subscript.IVal;      /* IVal just cleared: assign */
} else {
    Expr->IVal += Subscript.IVal;     /* pending member offset: add */
}
```

When `IVal` happens to be 0 in the second arm (a bare pointer rvalue), `+=`
and `=` are identical, so the fix cannot regress previously-correct code —
the only behavioural change is that a nonzero pending offset now survives.
The combined displacement then flows out through the normal mechanisms
(`ldy #offset` on the indexed store/load, or a fused `adc #offset`), so no
code-generator or optimizer change is needed.

The fork's array-typed branch directly above already used `+=` for the same
reason, which is a good consistency check on the shape of the fix. Upstream
cc65 rewrote this whole area after 2.19; this fork takes the minimal,
reviewable correction instead of a backport.

## 4. Verification

1. **Repro matrix** (`tests/compiler/member_addr_cast.py`, wired into
   `tests/run.sh` between the unit and integration stages): constant-index
   write through a cast (must hit `+6`), constant-index write without a cast
   on int elements (must hit `+5`), constant-index read (`+6`), and the
   struct-object constant-base form (`garr+19`/`+20`). It also asserts the
   bug's signature (offset-less `ldy #$01` in the cast-write body) is gone.
   The test compiles with the freshly built `bin/cc65 -Ors --codesize 500`
   and pattern-checks the asm; it skips (exit 0) when `bin/cc65` is absent.
2. **No collateral codegen drift**: after rebuilding the toolchain,
   libraries and every example with the fixed compiler, all curated GearLynx
   goldens still pass (`gearlynx_check.py`; the standing `mikey/setbpp`
   drift predates the fix), i.e. no previously-correct pattern changed its
   output.
3. **The original victim, un-worked-around**: the raycaster's wall-height
   store is back to the direct one-liner
   (`((unsigned char*)&s->vsize)[1] = slicelut[pi];` — the §9.1 workaround
   pointer is gone), and its 90-frame-from-reset GearLynx capture is
   byte-identical to the §9 reference frame. The store the bug used to
   corrupt is now itself a live regression canary in a shipped example.

## 5. Follow-through

- `design/LYNX_RAYCASTER_DRAW_DESIGN.md` §9.4's miscompile note points here
  and records that the workaround has been removed.
- `tests/README.md` documents the new `tests/compiler/` stage.
- The two-step-pointer workaround remains a valid (never-wrong) style, but
  is no longer *required* anywhere in the tree.
