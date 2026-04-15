05 — Interval-fraction α
========================

Status: not started
Depends on: 01
Pairs with: 02, 04

Goal
----

Make iv2d's "α = the fraction of the output interval that is negative" a
reusable helper — both as a CPU-side rect-level decision (used by 02 to
classify tiles) and as a JIT-time builder helper (used by 04 to convert a
signed f into α without a separate quality mechanism).

Why
---

iv2d's sub-pixel α approximation is:

    if hi <= 0:  α = 1
    elif lo >= 0: α = 0
    else:        α = -lo / (hi - lo)

It is dirt cheap, closed-form, and for any smooth Lipschitz-ish SDF it sits
within a small fraction of a pixel of the supersampled ground truth. It is
exactly the formula a quadtree leaf wants to emit when it cannot afford
another subdivision step, and it is also exactly what a shape's coverage
can fall back to when interval bounds are available.

Current state
-------------

- Nothing in umbra emits this closed form today.
- `umbra_coverage_sdf` takes a pre-rasterized SDF and clamps/linearizes it
  into α via whatever it does in `src/draw.c`. Presumably a linear transform
  on distance; not based on an interval bound.

Design sketch
-------------

Two usages, one formula, separate call sites.

Rect-level (CPU), used by (02):

    static inline float iv_alpha(iv f) {
        if (f.hi <= 0) return 1;
        if (f.lo >= 0) return 0;
        return -f.lo / (f.hi - f.lo);
    }

Pixel-level (JIT), used by (04):

    umbra_val32 umbra_interval_alpha(struct umbra_builder *b,
                                     umbra_val32 lo, umbra_val32 hi);

The pixel-level builder emits roughly:

    diff = hi - lo
    raw  = -lo / diff
    a    = clamp(raw, 0, 1)
    a    = sel(hi <= 0, 1, a)        // fully inside
    a    = sel(lo >= 0, 0, a)        // fully outside

This is ~6 umbra ops. Cheap.

Where do `lo` and `hi` come from at pixel time? Two options:

a. **From (03)**: the shape's f is evaluated as an interval over the *pixel*,
   and its lo/hi are the pixel-level lo/hi. This is the purest answer but
   requires (03) to land. Best answer long-term.
b. **From f and a Lipschitz bound L**: if the caller promises `|∇f| ≤ L`,
   then per-pixel `lo ≈ f - L*√2/2`, `hi ≈ f + L*√2/2` for a 1-pixel cell.
   Works today with no new machinery. Good starting point.

For the MVP: option (b). Expose an `umbra_coverage_region_lipschitz(eval,
L, ctx)` variant where the caller provides L (default 1, which is honest for
exact SDFs). Migrate to (a) once (03) is real.

Files touched
-------------

- small addition to `src/region.c` or a new `src/interval_alpha.c`
- `include/umbra_draw.h` or `include/umbra_region.h`: declare
  `umbra_interval_alpha`
- `tests/interval_alpha_test.c`

Testing
-------

Unit:

- the hi ≤ 0 and lo ≥ 0 branches short-circuit to {1, 0}
- middle branch produces the expected fraction on hand-chosen intervals

Quality:

- compare against a supersampled ground truth on a circle SDF over a 512×512
  viewport at radius 200; report mean and max per-pixel α error
- expectation: mean error < 0.01, max error < 0.1 on a Lipschitz-1 SDF;
  record the actual numbers when the test lands

Regression:

- coverage pipelines that don't use interval-fraction α are unaffected
- `umbra_coverage_sdf` is unchanged by default

Expectations
------------

Correctness: α ∈ [0, 1] always; never NaN (diff > 0 in the middle branch by
construction, but add a guard and prove it).

Performance: adds ~6 ops to any region coverage that uses it. Roughly 1 ns
per pixel; net win vs. supersampling.

Code quality: ~60 lines new code, ~120 lines tests. No changes to existing
coverage instances.

Open questions
--------------

- Should the helper live in `umbra_draw.h` (near coverages) or a new
  `umbra_region.h` (with 04)? Leaning 04's header.
- Do we cap α's worst-case error when L is unknown? A sensible fallback is
  to treat the SDF as exact and add an opt-in correction coefficient.
- Non-Lipschitz inputs (sharp-corner SDFs with unbounded gradient near the
  corner) will mis-estimate. Worth documenting, not worth solving here.

Non-goals
---------

- No adaptive quality. That is (02).
- No supersampling. `umbra_shader_supersample` already exists for callers
  who want that trade; this is a cheaper alternative, not a replacement.
