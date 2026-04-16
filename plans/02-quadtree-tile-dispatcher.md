02 — Quadtree tile dispatcher
=============================

Status: complete — see Retrospective below.
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


Retrospective
-------------

Landed across thirteen commits in April 2026.  Design drifted substantially
from the sketch during implementation — the dispatcher lives somewhere
different than planned, and the public API took a different shape.

### What we built vs what was sketched

**File layout.**  Sketch: new `src/tile_cover.{h,c}` owning a
`tile_cover_programs` type and `tile_cover_dispatch` recursion.  Landed:
everything lives in `src/draw.c`, which already composes shader + coverage
+ blend.  The compiled bundle is `struct umbra_draw` (fully opaque — the
struct definition is in draw.c, no internal header), the recursion is a
static `queue_recurse` in draw.c, and there's no separate tile_cover
module.  Reasoning (from mid-flight discussion): draw.c is where pipeline
composition already happens, and the recursion is ~25 lines that don't
need their own TU.  Matches CLAUDE.md's "don't create helpers / abstractions
for one-time operations."

**API shape.**  Sketch: a new `_tile` entry point next to `umbra_draw_build`,
opt-in, so slides choose.  Landed: one API for everyone.
`umbra_draw(be, shader, coverage, blend, fmt, layout)` returns a
`struct umbra_draw*` holding one compiled program and an optional
`interval_program`.  If the coverage IR intervalizes, the bundle gets
the interval_program; otherwise it stays NULL.
`umbra_draw_queue(d, l, t, r, b, buf)` branches internally: quadtree
when `d->coverage` is non-NULL, single flat dispatch otherwise.  Callers
don't need to know whether their coverage is interval-expressible.  This
emerged from a question in review and is strictly simpler for slides.

The old `umbra_draw_build` (returns `umbra_builder*`) was renamed to
`umbra_draw_builder` to free the name; the new constructor is just
`umbra_draw()` per CLAUDE.md's convention that a type's constructor takes
its bare name and the return type forward-declares the opaque type.

**Uniforms layout.**  Not in the sketch.  `umbra_draw_builder` builds
coverage before shader, so coverage uniforms land at offset 0 in the
buffer.  `build_coverage_interval` independently builds a coverage-only
IR that also starts at offset 0.  Both agree without any offset
arithmetic — `interval_program_run` reads directly from `buf[0].ptr`.

**Interval pointer convention swap.**  Not in the sketch; forced by
aligning with umbra_draw.  Plan 01 had interval.c reading uniforms from
`.ix=1` and storing output to `.ix=0`.  umbra_draw does the opposite.
Swapping interval.c's convention (UNIFORM=0, SINK=1) lets a coverage
authored against umbra_draw lift into an interval_program without pointer
rewriting.  One-line code change plus comment updates.  Worth the
disruption; the alternative was a translation pass on every coverage IR.

**`f32_from_i32` design detour.**  The sketch's "Expected op additions"
list called out `f32_from_i32` as likely-needed.  First landed it as an
identity in interval.c ("same domain in interval land").  Review pushback:
"storage type and op type aren't the same thing; calling it identity
elides that it's a real runtime conversion."  Walked it back.  Better
approach: build the coverage-only IR without wrapping x/y in the
conversion at all.  Coverages thread x/y through type-erased float ops,
so dropping the conversion is transparent at the IR level, and interval.c
stays narrow — every op it accepts has principled interval semantics.
Recorded in commits `58b3fbe4` + `fffdf0f1` so the reasoning is legible.

**Interior / boundary branches.**  Sketch pseudocode had separate
interior (skip coverage) and boundary (full pipeline) programs.
Initially landed with two compiled programs: `full_coverage` (shader +
blend, no coverage multiply) and `partial_coverage` (full pipeline).
Later removed the `full_coverage` program after benchmarking showed the
coverage computation (rect check, circle SDF) is cheap enough that
skipping it in fully-inside tiles saves nothing measurable.  The
quadtree's value is entirely in pruning empty tiles (coverage.hi <= 0),
not in the interior/boundary split.  Now a single program handles both
cases.  The dispatch has two stops: skip (coverage.hi <= 0) and render
(coverage.lo >= 1 or tile is at the minimum-size floor).

**Tight interval bounds at tile edges.**  Sketch pseudocode passed
`(interval){l, r}`.  Landed `(interval){l, r-1}` since op_x yields
integer coords in the closed range `[l, r-1]`.  Matters for thin-boundary
tiles near the 0/1 thresholds where an extra-wide bound misses a prune.

**MIN_TILE chosen by sweep.**  Sketch flagged it as "pick K or 16;
measure."  We swept MIN_TILE ∈ {4..99999} across interp/jit/metal/vulkan/
wgpu on the circle slide and landed 512 — the smallest value where every
backend beats its flat baseline.  Per-backend optima are ~32× apart
(CPU: 16–32, GPU: 512–1024); 512 leaves ~10% CPU throughput on the table
in exchange for not regressing GPU.  TODO in the code for principled
per-backend `dispatch_granularity` querying.

### Side yields

- `tools/bench_interval.c` — gating micro-bench for interval_program_run.
  Built to answer the plan's "bench before committing" instruction.
  Kept in-tree for regression checking as we grow interval.c.  Used the
  `volatile` pattern for the sink (cleaner than a static+final-print).
- `interval.c` learned `mul_f32_imm`, `min_f32_imm`, `max_f32_imm` —
  the clamp idiom every real coverage uses.  Separate commit with
  regression tests.
- Renaming pass: `umbra_draw_build` → `umbra_draw_builder`, reclaiming
  `umbra_draw` for the constructor.  Widespread (every slide + several
  tests) but mechanical.

### Performance

Circle slide (4096×480, circle covering ~1% of canvas):

    backend   flat ns/px   adaptive@512   speedup
      metal       0.05          0.03        1.7×
     vulkan       0.06          0.04        1.5×
       wgpu       0.07          0.06        1.2×
        jit       0.86          0.20        4.3×
     interp       2.48          0.47        5.3×

Interpreter overhead on the interval path measured separately (plan 02
step 0): ~19 ns/call on arm64, ~46 ns/call on x86_64h via Rosetta, for
a 20-op circle SDF.  Far under the plan's ~200 ns budget — the
interpreter is not the bottleneck.  At MIN_TILE=512 on a 4096×480
canvas the dispatcher makes ~40 `interval_program_run` calls per
frame, negligible against the pixel work.

Per-backend MIN_TILE optima (for future tuning):

    backend   optimum MIN_TILE   ns/px there
     interp         32              0.37
        jit         16–32           0.16
      metal        256–1024         0.03
     vulkan        256–1024         0.04
       wgpu         1024            0.05

### What didn't land

- Per-backend `min_tile`: captured as a TODO in draw.c.  The 32× spread
  between CPU-optimal and GPU-optimal is real and there's probably a
  principled way to derive it from backend parameters (subgroup size,
  per-dispatch latency).  Next plan.
- dispatch_overlap interaction — not re-verified.  The cap (64 writes)
  could overflow on a deep enough quadtree, degrading to conservative
  barriers.  Works by construction (non-overlapping tiles) for typical
  slide sizes; revisit if it bites.

### What landed later

- **`struct umbra_draw` replaced by `struct umbra_quadtree`.**
  Benchmarking showed only SDF-like coverages benefit from interval
  analysis — bitmap, winding, and texture coverages produce trivial
  [0,1] bounds with no pruning value.  `struct umbra_draw` (general
  coverage → optional interval_program) was replaced by
  `struct umbra_quadtree` (takes `struct umbra_sdf*`, always builds
  interval_program, always does quadtree dispatch).  Non-SDF slides
  (text, persp, slug) reverted to direct `prog->queue`.
- **`struct umbra_sdf`** — new first-class signed-distance trait.
  `build` returns `f(x,y)` (negative inside, positive outside).
  `umbra_coverage_from_sdf` adapter converts to coverage via
  `clamp(-f, 0, 1)`.  `umbra_sdf_rect` is the first concrete
  subtype — replaces `umbra_coverage_rect` for quadtree-dispatched
  slides.
- **SDF-based dispatch thresholds.**  Quadtree skips when `sdf.lo >= 0`
  (fully outside), stops subdividing when `sdf.hi <= -1` (fully inside
  past the 1-pixel ramp).  This is tighter than the old coverage-based
  thresholds.
- **Interval ops `le_f32`, `and_32`, `sel_32`** added for rect
  coverage intervalization.  The SDF rect (`max(l-x, x-r, t-y, y-b)`)
  uses only `max`/`sub` which were already supported.
- **`full_coverage` program removed.**  Benchmarking showed no
  measurable benefit from the interior/boundary program split.
- **Coverage uniforms at offset 0.**  `umbra_draw_builder` builds
  coverage before shader.  Eliminated `uniform_offset` bookkeeping.

### LOC

Plan: ~300 lines in tile_cover.c, smallish draw.c addition.  After
follow-up simplifications (removing full_coverage, draw.h, uniform_offset),
draw.c's dispatcher contribution is ~60 lines.  No internal header.
~145 lines bench_interval tool, ~100 lines tests, ~40 lines interval.c
for new ops.  Much smaller than planned.

### Lessons

1. **One API beats two.**  "Everyone gets the bundle, dispatch branches
   internally" is strictly simpler for callers than "opt into the
   dispatcher by calling a different function."  Worth reaching for this
   shape earlier in plan drafts.

2. **Keep the interval interpreter narrow.**  When a coverage's IR
   contains an op interval.c doesn't support, the right first move is
   usually to ask "can we avoid emitting that op?" rather than "can
   interval.c accept it?"  The `f32_from_i32` detour cost a round-trip
   we'd have saved by asking this up front.

3. **Bench before optimizing.**  The plan explicitly said "bench before
   committing to the dispatcher" and it paid off — we found we were 10×
   under the interpreter budget and saved ourselves from preemptive
   work on the hot path.

4. **Per-backend performance tuning is a real thing.**  The 32× spread
   between CPU-optimal and GPU-optimal MIN_TILE is a design signal,
   not noise.  A single compromise value can be the right move (it was
   here) but future performance work should plan for per-backend knobs.

5. **"Constructor name is the type" reads better.**  CLAUDE.md's
   convention (`umbra_draw()` returns `struct umbra_draw*`, no separate
   forward decl) landed noticeably cleaner than `umbra_draw_programs`
   / `umbra_draw_compile` / `umbra_draw_programs_free`.  Apply the
   convention deliberately in future public APIs.

6. **Slide migration is a natural forcing function for op support.**
   The plan's "add interval ops as slides hit them" guidance proved
   exactly right: circle's coverage forced `mul/min/max _f32_imm`;
   rect coverage forced `le_f32`, `and_32`, `sel_32`.  Plan future
   work by slide migration rather than by op list.

7. **Measure before assuming a fast path helps.**  The full/partial
   program split looked like an obvious win (skip coverage math on
   interior tiles), but benchmarking showed zero measurable benefit —
   coverage functions are cheap relative to shader+blend work.
   Removing the split halved compile cost and simplified the code.
   Bench first, complexity second.
