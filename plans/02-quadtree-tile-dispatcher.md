02 — Quadtree tile dispatcher
=============================

Status: not started
Depends on: 01
Pairs well with: 04, 05

Goal
----

A dispatch-time driver that, given a program whose coverage output is the
signal of interest, recursively subdivides the dispatch rect and dispatches
only the tiles that matter. Interior tiles take a fast path (no coverage
multiply). Exterior tiles are skipped. Boundary tiles run the full
pipeline at pixel granularity.

Direct lift of `iv2d_cover`, with the rects themselves becoming umbra
`queue()` calls rather than callback invocations.

Why
---

This is where the performance story lives. A pipeline rendered over a full
window spends most of its lanes on pixels that are either trivially inside
(α=1, no coverage math needed) or trivially outside (α=0, no work at all).
Interval-pruning the quadtree turns O(pixels) work into O(boundary pixels)
work plus O(log) decisions.

Current state
-------------

- Dispatch is a flat `queue(p, l, t, r, b, buf[])` call. One rect, one run.
- `src/draw.c` (851 lines) builds the composed shader + coverage + blend
  pipeline, flattens it to IR, and hands it to a backend.
- `struct umbra_coverage` is a trait with `build()` that emits (x,y) → α.
- `umbra_coverage_rect`, `umbra_coverage_bitmap`, `umbra_coverage_sdf`,
  `umbra_coverage_bitmap_matrix`, `umbra_coverage_wind` already exist.
- `src/dispatch_overlap.c` tracks write ordering; it does not select tiles.

Design sketch
-------------

New files: `src/tile_cover.h`, `src/tile_cover.c`.

Public entry:

    struct tile_cover_programs {
        struct umbra_program *full;     // shader + coverage + blend  (boundary tiles)
        struct umbra_program *solid;    // shader + blend, α assumed 1 (interior tiles)
        struct umbra_flat_ir  *coverage_ir;  // coverage-only IR, for interval eval
        int coverage_value_id;          // which value in coverage_ir is α
    };

    void tile_cover_dispatch(struct tile_cover_programs const *p,
                             int l, int t, int r, int b,
                             struct umbra_buf buf[],
                             int min_tile);   // stop subdividing below this

Algorithm (straight from iv2d_cover, adapted):

    recurse(l, t, r, b):
        iv alpha = interval_eval(coverage_ir, x=[l,r], y=[t,b], ...)
        if alpha.hi <= 0:
            return                          # fully outside
        if alpha.lo >= 1:
            solid.queue(l, t, r, b, buf)    # fully inside, skip coverage
            return
        w = r-l; h = b-t
        if w <= min_tile && h <= min_tile:
            full.queue(l, t, r, b, buf)     # boundary tile; per-pixel eval
            return
        mx = (l+r)>>1; my = (t+b)>>1
        recurse(l, t, mx, my)
        recurse(mx, t, r, my)
        recurse(l, my, mx, b)
        recurse(mx, my, r, b)

Integration with `umbra_draw`:

- `umbra_draw_build` already builds a combined IR from shader + coverage +
  blend. It needs to additionally build:
  - the coverage-only sub-IR (just the coverage's `build`), and
  - a "solid" variant (shader + blend, α=1)
- These three go into a `tile_cover_programs` instance that the caller
  can dispatch via `tile_cover_dispatch` instead of the single `queue()` call.

Tile floor:

- `min_tile == 1` mirrors iv2d's pixel-level behavior.
- `min_tile == K` (the SIMD lane count) might be a better default — below K,
  the boundary cost isn't really saved.
- Leave it as an argument; measure.

Files touched
-------------

- new: `src/tile_cover.{h,c}`
- `src/draw.c`: build the 3-program bundle, not just one program
- `include/umbra_draw.h`: add the bundle type and a `_tile` entry alongside
  the existing `umbra_draw_build`
- `tests/tile_cover_test.c`
- `slides/`: port one or two slides to `_tile` dispatch to exercise it
- `tools/bench.c` (or similar): benchmark before/after

Testing
-------

Correctness:

- solid fill over a circle-shaped coverage, compared pixel-for-pixel against
  the non-adaptive path
- random rects with holes (a coverage that returns 0 in a checkerboard)
- smallest cases: 0-area rects, 1×1 rects, strips narrower than min_tile

Invariants:

- union of all dispatched tiles equals the non-skipped pixels
- no tile is dispatched twice
- overlap tracking (`dispatch_overlap.c`) still sees every write

Performance:

- circle of radius R in an N×N viewport: expect interior work ∝ R², boundary
  work ∝ R, skipped work ∝ (N² - πR²). Measure on a slide that is mostly
  interior (simple shape, big rect) and one that is mostly boundary (thin
  outline).

Expectations
------------

Correctness: pixel-identical to the flat `queue()` path. Same coverage
semantics, same blend path, just fewer dispatches.

Performance: meaningful speedup (2–10×) on pipelines with mostly-interior or
mostly-exterior coverage. Slight slowdown (5–15%?) on pipelines where every
tile straddles the boundary, from interval-eval overhead. Overall win for the
realistic mix.

Code quality: ~300 lines in `tile_cover.c`, smallish addition to `draw.c`,
no changes to existing public API, new public API additive.

Open questions
--------------

- Does the fast-path "solid" program need to be built by every caller, or can
  `draw.c` derive it automatically from the shader + blend?
- How does this interact with `dispatch_overlap`? Presumably record each tile
  separately; need to verify we don't regress the barrier logic.
- `min_tile`: default? Probably K (lane count) until measurements say otherwise.
- Is there a path for partial-coverage tiles to skip even the coverage IR at
  runtime (since we already know α is in `[lo, hi]`)? Likely yes — use a
  specialized "α is linear in x,y within this tile" pipeline when hi - lo is
  small. Defer to a follow-up plan.

Non-goals
---------

- No new JIT work. The adaptive driver is CPU-side orchestration of existing
  backends.
- No change to the coverage contract.
- No interaction with loops/ifs in coverage (01 will bail on those for now).
