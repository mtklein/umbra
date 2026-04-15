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
- Plan 01 landed `struct interval_program`.  Output is the last
  `umbra_store_32` to `(umbra_ptr32){.ix=0}` (SINK); uniforms come in
  as `float const *uniform` indexed by slot on `(umbra_ptr32){.ix=1}`
  (UNIFORM).  Op coverage is narrow — currently only what plan 01's tests
  exercise.  Adding ops as we encounter them in real coverage pipelines
  is part of plan 02's work.

Design sketch
-------------

New files: `src/tile_cover.h`, `src/tile_cover.c`.

Public entry:

    struct tile_cover_programs {
        struct umbra_program     *full;      // shader + coverage + blend  (boundary)
        struct umbra_program     *solid;     // shader + blend, α assumed 1 (interior)
        struct interval_program  *coverage;  // coverage-only, output via SINK store
        // uniforms passed alongside dispatch()
    };

    void tile_cover_dispatch(struct tile_cover_programs const *p,
                             int l, int t, int r, int b,
                             struct umbra_buf buf[],
                             float const *uniform,  // for coverage's interval eval
                             int min_tile);         // stop subdividing below this

Algorithm (straight from iv2d_cover, adapted):

    recurse(l, t, r, b):
        interval alpha = interval_program_run(p->coverage,
                                              (interval){l, r},
                                              (interval){t, b},
                                              uniform);
        if (alpha.hi <= 0.0f) { return; }              // fully outside
        if (alpha.lo >= 1.0f) {
            solid->queue(p->solid, l, t, r, b, buf);   // interior, skip coverage
            return;
        }
        int const w = r - l, h = b - t;
        if (w <= min_tile && h <= min_tile) {
            full->queue(p->full, l, t, r, b, buf);     // boundary; per-pixel eval
            return;
        }
        int const mx = (l+r)/2, my = (t+b)/2;
        recurse(l,  t,  mx, my);
        recurse(mx, t,  r,  my);
        recurse(l,  my, mx, b );
        recurse(mx, my, r,  b );

The coverage IR must store α to `(umbra_ptr32){.ix=0}` (SINK) and read
any uniforms from `(umbra_ptr32){.ix=1}` (UNIFORM) — that's the protocol
`interval_program()` enforces.  `umbra_draw_build` needs to emit the
coverage sub-IR with those pointers, separate from the shader/blend IR
which uses its own buffer slots.

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
  the boundary cost isn't really saved.  K varies by backend (8 on ARM64
  JIT, 32 on most CPU, 64 on wasm), so make it backend-queryable or just
  pick a fixed sensible floor like 16.
- Leave it as an argument; measure.

interval_program_run cost
-------------------------

The quadtree visits at most 4 × (log2(max(w,h) / min_tile)) * (pixels / K)
tiles per dispatch — for a 1024² dispatch with `min_tile=8`, that's up to
~16k interval_program_run calls per frame.  At 1 μs each (the plan-01
aspiration) that's 16 ms — a real budget.  At ~200 ns each, 3 ms — fine.
Before committing to the full dispatcher, bench the real cost of
`interval_program_run` on a circle-SDF coverage.  If it's significantly
above ~200 ns, optimize the loop (fewer allocations, tighter op dispatch)
before plumbing it into draw.c.

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

Expected op additions to interval.c
-----------------------------------

Plan 01 pruned `interval.c` to only the ops its tests exercise.  Real
coverage pipelines will hit more, and each one returns NULL from
`interval_program()` until supported.  The list we'll likely need:

- `op_sel_32` for masked coverage (e.g. coverage composed with a clip).
- `op_eq_f32`, `op_le_f32` for compare-based coverage (wind rule's sign
  checks, polygon edge tests).
- `op_and_32` / `op_or_32` / `op_xor_32` on tri-valued mask intervals for
  mask composition.
- Possibly `op_f32_from_i32` / `op_i32_from_f32` for x/y coord conversions.
- `op_gather_*` is the hard one — bitmap coverage does gathers, and their
  bound is the full range of the bitmap unless we can prove the index is
  always in-bounds of a known sub-region.  Likely land as "bail to MAXIMAL"
  first, so bitmap coverages just take the full-pipeline path.

Add these as we hit them while porting slides, one at a time with tests.

Non-goals
---------

- No new JIT work. The adaptive driver is CPU-side orchestration of existing
  backends.
- No change to the coverage contract.
- No interaction with loops/ifs in coverage (01 will bail on those for now).
