01 — Interval IR interpreter
============================

Status: not started
Depends on: nothing
Unlocks: 02, 03, 05; tightens 04

Goal
----

Add a second interpretation of `struct umbra_flat_ir` that evaluates each
value as an interval `[lo, hi]` rather than a K-lane float or i32. Given an
input range for `x`, `y`, and any uniforms, produce an interval per IR value
and in particular an interval on the final stored/returned value.

This is a standalone CPU-side pass. It does not change codegen. It does not
affect any existing dispatch.

Why
---

Every subsequent iv2d-style idea eventually needs "for this rect, what range
of outputs can this pipeline produce?" iv2d writes its regions as functions
from interval-in to interval-out. umbra's IR is already the right shape for
this — we just need to define what each op does to an interval.

Once we have it, (02) can prune tiles, (05) can compute α analytically, and
the builder can (eventually) do range-based optimizations.

Current state
-------------

- `src/flat_ir.c` produces a normalized IR from `umbra_builder`.
- `src/interpreter.c` evaluates that IR on K-lane values for the `interp`
  backend. ~1.3 kloc; closely coupled to the K-lane representation.
- No interval/bounds analysis anywhere.

Design sketch
-------------

New files: `src/interval.h`, `src/interval.c`. Kept separate from the K-lane
interpreter — the shapes are different enough that sharing would pretzel both.

Primary type:

    typedef struct { float lo, hi; } iv;

Entry point (first cut):

    struct iv_eval_result {
        iv *v;          // one iv per IR value id; owner must free (or caller-provided)
        _Bool bailed;   // true if we hit an op we can't bound (gather, loop body, etc.)
    };

    struct iv_eval_result umbra_flat_ir_eval_interval(
        struct umbra_flat_ir const *ir,
        iv x, iv y,                 // rect ranges
        iv const *uniforms, int nu, // uniform values (exact intervals for constants)
        iv *scratch);               // caller-provided scratch[ir->insts], or NULL to alloc

Per-op rules:

- `imm_*`                → `iv{imm, imm}`
- `x`, `y`               → caller-supplied intervals
- `uniform_*`            → caller-supplied exact interval
                           (convention: uniforms read from `umbra_ptr32{.ix=1}`;
                           other pointers → constructor returns NULL. The
                           output sink is `umbra_ptr32{.ix=0}`.)
- `add_f32`              → `{a.lo + b.lo, a.hi + b.hi}`
- `sub_f32`              → `{a.lo - b.hi, a.hi - b.lo}`
- `mul_f32`              → min and max of `{a.lo*b.lo, a.lo*b.hi, a.hi*b.lo, a.hi*b.hi}`
- `div_f32`              → if `b` straddles zero: `{-INF, +INF}`; else mul by 1/b
- `fma_f32`, `fms_f32`   → compose add/sub with mul rules (don't reassociate)
- `min_f32`/`max_f32`    → `{min(a.lo,b.lo), min(a.hi,b.hi)}` / symmetric
- `abs_f32`              → if straddles 0: `{0, max(-a.lo, a.hi)}`; else non-neg shift
- `sqrt_f32`             → clamp lo to 0; `{sqrt(max(0,lo)), sqrt(max(0,hi))}`
- `round/floor/ceil_f32` → apply to endpoints
- compares (`lt/le/eq`)  → tri-valued alpha in `{0, 1, maybe}`:
  - `lt(a,b)`: if `a.hi < b.lo` → `{1,1}`; if `a.lo >= b.hi` → `{0,0}`; else `{0,1}`
- `sel_32(m, t, f)`      → if m is `{1,1}` take t; if `{0,0}` take f; else union
- `and_32`/`or_32`/`xor_32` on masks → piecewise lattice op on `{0, 1, maybe}`
- int ops (`add/sub/mul/shl/shr`) → analogous to float rules on i32 endpoints
- conversions (`f32_from_i32` etc.) → apply to endpoints, floor/ceil conservatively
- `load_*`, `gather_*`, `deref_ptr` → bail (return maximal interval); varying loads
  can't be bounded from IR alone
- `loop_begin/loop_end`, `if_begin/if_end` → first cut: bail if encountered. Later:
  interval-analyze the straight-line body with a fixpoint pass for loops.
- `store_*`                → evaluate the stored value, record it; does not produce
  a new IR value

Representation of tri-valued masks: keep them as `iv{0,0} | iv{1,1} | iv{0,1}`.
That lets `sel_32` and `and_32` / `or_32` remain simple piecewise rules.

Files touched
-------------

- new: `src/interval.h`, `src/interval.c`
- new: `tests/interval_test.c`
- `build.ninja`: add the new source and test

Testing strategy
----------------

Unit tests (each op's rule, with straddling cases):

- `mul` of `[-1, 2] * [-3, 4]` = `[-6, 8]`
- `abs` on `[-2, 1]` = `[0, 2]`
- `sqrt` on `[-1, 4]` = `[0, 2]` (domain clamp)
- `div` with denom `[-1, 1]` → maximal
- `sel_32` on a mixed mask → union of branches

End-to-end tests:

- build `f(x,y) = x*x + y*y - r*r` (unit disc SDF, `r` uniform)
- eval over boxes fully inside, fully outside, straddling — check tight conservative
- for a few pipelines in `dumps/`, check that the interval bound contains the K-lane
  interpreter's observed min/max across the same rect (sample many points)

Per CLAUDE.md "here-style" asserts throughout.

Expectations
------------

Correctness: bound is always conservative (contains the true output range);
tight on affine/monotonic paths; loose but safe on pathological cases.

Performance: CPU scalar eval of a typical 20-op shader should take under 1 μs.
Not a concern at this stage — called O(tiles) from (02), not per-pixel.

Code quality: ~300 lines new source, ~200 lines test. No changes to existing
files beyond `build.ninja`.

When this lands we note actual LOC, tightness on the circle SDF over a
representative box, and any ops we had to bail on that we didn't plan to.

Open questions
--------------

- Should the public entry take `iv *scratch` or always allocate? Leaning
  caller-provided so (02) can reuse one buffer across a million tile calls.
- How should we expose "which value is the output"? For (02) the answer is
  probably "the last store's stored value," but coverage pipelines may want
  a specific `umbra_val32 result` handle instead.
- Loops: conservative bail is fine at first, but a common iv2d pattern is a
  loop-free SDF. Confirm by porting a slide.

Non-goals
---------

- No JIT codegen. That is (03), and only if measurements demand it.
- No change to `interpreter.c`'s K-lane path.
- No change to public API.
