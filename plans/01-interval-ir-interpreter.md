01 — Interval IR interpreter
============================

Status: complete — see Retrospective below.
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


Retrospective
-------------

Landed across several commits in April 2026.  The design shifted meaningfully
from the sketch above during review — worth noting for future plans.

### What we built vs what was sketched

**Type names.**  `iv` became `interval`, `iv_eval_*` became
`interval_program_*`, `iv_point` became `interval_exact`.  Terser names read
fine in isolation but the long forms hold up better in prose and stack well
with `interval_is_finite`, `interval_program_run`, etc.

**Lifecycle.**  The sketched `struct iv_eval_result` with a caller-provided
scratch buffer was replaced with an `umbra_builder`-style encapsulation:
`struct interval_program *interval_program(ir)` → `interval_program_run(p,
x, y, uniforms)` → `interval_program_free(p)`.  The constructor memcpys the
`ir_inst` array so the caller can free the flat IR (and its builder) as soon
as construction returns.  This mirrors how the rest of umbra manages
lifetimes.  Plan 02's tile dispatcher should reuse a single `interval_program`
across many tiles, amortizing the per-run allocation.

**Construction vs run separation.**  The sketched `bailed` flag on the run
result became a NULL return from the constructor.  The constructor vets the
whole IR up-front (unsupported ops, wrong SINK / UNIFORM pointers, missing
sink store all return NULL), and run() can then assume a well-formed
program.  This keeps `run()` hot and branch-free in the common case.

**Sink / uniform convention.**  Not in the sketch: output goes to
`umbra_store_32(b, umbra_ptr32{.ix=0}, …)`; uniforms are read from
`umbra_uniform_32(b, umbra_ptr32{.ix=1}, slot)`.  Enforced at construction.
Distinct pointers for inputs and outputs make pipelines obvious at a glance
and make the "no sink store" failure mode explicit.

**Uniform shape.**  The sketch passed uniforms as intervals (`iv const*`);
we landed with `float const *uniform` — exact point values, no count.
Matches backend convention (unchecked indexed load), so short arrays are UB
and caught by asan.  Uniforms are exact at dispatch time anyway; there's no
case where passing a range makes sense.

**Output exposure.**  Landed with the sketch's preferred answer: run()
returns the interval of the last `umbra_store_32` to SINK.

**Op coverage is narrower than sketched.**  We started with the sketch's
full list, then pruned every op not exercised by tests during review ("if
it's untested it's dead code").  Current support: `x`, `y`, `join`,
`imm_32`, `uniform_32`, `store_32`; f32 `add/sub/mul/div/min/max/fma/fms/
sqrt/abs/floor/ceil`; `lt_f32`, `le_f32`; `and_32`, `sel_32`; and the
`_imm` variants `add/sub/mul/div/min/max/lt/le`.  The `le_f32`/`and_32`/
`sel_32` ops were added later to support rect coverage intervalization.
Integer arithmetic, other bitops, int compares, loads, gathers, variables,
loops, ifs — all still make the constructor return NULL.  Added as real
coverages force them.

**`op_square_f32` specialization.**  Not in the sketch; added during review
when `interval_mul(x, x)` slop on circle SDFs came up.  Expanded to a full
family: `op_square_f32`, `op_square_add_f32` (`x*x + y` fused), `op_square_
sub_f32` (`y - x*x` fused), plumbed through every backend and with builder
rewrites for `mul(v, v) → square(v)` and `add(square(v), z) → square_add`.
Single biggest bound-quality win; not discovering it in the plan cost a
later round-trip.

**`sqrt` policy changed mid-flight.**  Sketch said "clamp lo to 0".  We
landed that initially, then switched to `interval_monotone(x, sqrtf)` (let
NaN propagate) once `interval_square` eliminated the straddle-zero-from-
rounding-slop case that the clamp was defending against.  NaN propagation
is contagious and correctly triggers `!interval_is_finite(out)`, which is
what callers want.

**fma/fms fused at interval level.**  Sketch said "compose add/sub with mul
rules".  We landed with genuinely single-rounded `fmaf` at (4 or 8) corner
combinations, matching backend behavior bit-exactly.  Added
`test_fma_f32_single_rounding` (and `test_square_*_single_rounding`) to
prove every backend emits real fused-multiply, not two-op mul+add.

**Strict interval rounding: considered and rejected.**  The sketch didn't
raise it; we researched it after shipping and decided against.  Round-to-
nearest is unsound in principle (0.5-ulp-per-op slop) but:
  - slow to fix (outward-rounding `nextafter` per op),
  - inflates exact answers that round-to-nearest got right,
  - worst: inflates intervals that touch zero exactly, so
    `interval_square([-1, 1]) = [0, 1]` becomes `[-ε, 1]` → breaks sqrt
    (NaN), flips tri-valued `lt(_, imm(0))` from "definitely ≥ 0" to
    "maybe".  Actively worse for dispatch pruning, the whole point.
The rationale is recorded in `src/interval.c` next to the ops it would
affect.

### Side yields

- `src/count.h` with `#define count(arr) (int)(sizeof(arr) / sizeof(0[arr]))`,
  plus a sweep across src/, tests/, tools/, slides/ replacing the
  `sizeof X / sizeof *X` boilerplate.  Landed during plan 01 work because
  the pattern kept appearing in new tests.
- Three legal self-fold peepholes (`min_f32(v, v) → v`, `max_f32(v, v) → v`,
  `lt_f32(v, v) → imm(0)`) — found while auditing what other correlation
  patterns `interval_square` might be missing.

### LOC

Plan: ~300 src, ~200 test.  Actual: ~320 lines in interval.c + 30 in
interval.h + ~370 in interval_test.c, plus the square-op plumbing across
the builder / interpreter / two JITs / metal / spirv (~200 lines), plus
test coverage for square ops in bb_test.c (~90 lines).  Significantly more
than planned, all in the square detour — which was worth it.

### Lessons

1. **Encapsulate with a type when you can.**  The caller-provided-scratch
   API in the sketch was less principled than the umbra_builder pattern.
   Future plans that propose an API should first ask "does this shape
   match the rest of the project?"

2. **Construction-time vetting.**  Separating "can we handle this IR?"
   from "handle it" simplifies the hot path.  This pattern could apply to
   other passes.

3. **Consider correlation explicitly when designing interval math.**
   The `x*x` slop was a foreseeable issue and the fix (specialized op)
   was a well-known technique in prior art (iv2d's SQR).  Plan drafts
   should include a "where does the interval bound lose precision" pass
   before locking scope.

4. **Strict rounding isn't universally better.**  For sound-proof-style
   use cases it's required; for dispatch pruning it's harmful.  Note the
   use case when making precision/rigor tradeoffs.

5. **Pruning is part of the work.**  Landing with tested code only, then
   adding ops as needed by callers, kept interval.c small and coverage
   100% on functions.  Would recommend for similar "implement many rules"
   passes.
