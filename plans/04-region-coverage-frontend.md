04 — Region coverage front-end
==============================

Status: not started
Depends on: nothing strictly; nicer with 02 and 05
Unlocks: port iv2d slides to umbra

Goal
----

A small umbra front-end for writing implicit 2D shapes the way iv2d does:
as a symbolic function `f(x, y)` whose sign determines inside/outside. The
front-end compiles that description into an `umbra_coverage` instance that
flows through the normal `umbra_draw_build` path.

This is the "user writes `y - (x*x + y*y - r*r)` and gets a disc rendered"
ergonomics from iv2d, ported directly.

Why
---

iv2d's region builder is tight and pleasant to use. umbra already has the
right seam — `struct umbra_coverage` — and four concrete coverages living
at that seam (`_rect`, `_bitmap`, `_sdf`, `_bitmap_matrix`, `_wind`). An
iv2d-style region becomes a fifth instance rather than a whole new subsystem.

Paired with (02), a region-authored coverage gets adaptive dispatch for free.
Paired with (05), it gets analytic sub-pixel α for free. Without either, it
still gives iv2d-style authoring on top of umbra's existing per-pixel path.

Current state
-------------

- `umbra_coverage` has one slot: `build(self, builder, uniforms_layout, x, y) → α`.
- `umbra_coverage_sdf` takes a pre-rasterized SDF bitmap and samples it for
  coverage; it does not know the SDF's math.
- No path today for the caller to describe an SDF symbolically at build time.

Design sketch
-------------

New header: `include/umbra_region.h`. Minimal API modeled on `iv2d_vm`:

    // A region is just a function (builder, x, y) → signed value.
    // f <= 0 means inside; f > 0 means outside. Magnitude encodes a Lipschitz
    // pseudo-distance (not required to be exact, but closer-is-better for
    // anti-aliasing quality).
    typedef umbra_val32 (*umbra_region_fn)(void *ctx, struct umbra_builder*,
                                           struct umbra_uniforms_layout*,
                                           umbra_val32 x, umbra_val32 y);

    struct umbra_coverage_region {
        struct umbra_coverage base;
        umbra_region_fn       eval;
        void                 *ctx;
    };
    struct umbra_coverage_region umbra_coverage_region(umbra_region_fn, void *ctx);

The caller writes:

    static umbra_val32 disc(void *ctx, struct umbra_builder *b,
                            struct umbra_uniforms_layout *u,
                            umbra_val32 x, umbra_val32 y) {
        float const *r = ctx;
        umbra_val32 dx2 = umbra_mul_f32(b, x, x);
        umbra_val32 dy2 = umbra_mul_f32(b, y, y);
        umbra_val32 r2  = umbra_imm_f32(b, (*r) * (*r));
        return umbra_sub_f32(b, umbra_add_f32(b, dx2, dy2), r2);   // ≤ 0 inside
    }

    struct umbra_coverage_region cov = umbra_coverage_region(disc, &radius);

Inside `umbra_coverage_region.build` we call `eval(...)` to get the signed
value, then convert to α via (05) — linear-clip for the MVP, interval-fraction
later.

For iv2d's compositional operators:

    union        == min(a, b)
    intersection == max(a, b)
    difference   == max(a, -b)
    offset       == a - d
    shell        == abs(a) - w

These already exist as umbra ops; no new IR.

Uniforms: any animated parameter flows through the uniforms layout that
`build()` already receives. No new concept.

Files touched
-------------

- new: `include/umbra_region.h`
- new: `src/region.c` (thin; mostly glue + a handful of convenience helpers
  for common shapes)
- `tests/region_test.c`
- optional: one slide in `slides/` ported from an iv2d demo to prove ergonomics

Testing
-------

Correctness:

- port 1–2 iv2d slides (a disc, a rounded rect, a union/difference) and
  render them; byte-compare outputs against iv2d's rendering at a quality
  where both should converge
- compositional: verify `min` is union, `max` is intersection, `max(a, -b)`
  is difference, on pairs of primitives with known overlap

Regression:

- `umbra_coverage_region` goes through the existing `umbra_draw_build` path;
  re-run existing coverage tests to confirm no regression

Expectations
------------

Correctness: same output as iv2d on shapes that both express. Slight
differences expected only from (05)'s α formula vs. iv2d's.

Performance: neutral on existing paths (this is pure addition). New
paths cost whatever their shape costs; no adaptive dispatch yet, so a
full-rect shape costs `O(pixels * ops)` at this stage.

Code quality: ~100 lines of new code in `region.c`, ~60 lines header,
~200 lines test. No existing files change beyond `build.ninja`.

Open questions
--------------

- Do we want typed "region" objects (like iv2d has) or just "a coverage that
  happens to be authored this way"? Leaning on the latter — composes with
  everything that takes an `umbra_coverage`.
- Should convenience helpers for common shapes (disc, rect, rounded rect,
  capsule) ship with this, or be a follow-up? Probably a small starter set
  so the tests have something to reach for.
- Do we expose any Lipschitz-quality hint to (05), or let α fall back to
  the interval-fraction estimator unconditionally?

Non-goals
---------

- No new IR ops.
- No special pipeline for regions — they're just coverages.
- No CSG tree builder. Users compose via ordinary umbra ops.
