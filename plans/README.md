iv2d-in-umbra
=============

Living plans for absorbing iv2d's key ideas (https://github.com/mtklein/iv2d)
into umbra. The short version of the vision: iv2d's "implicit region,
interval-pruned, adaptively dispatched" model composes cleanly with umbra's
K-lane JITed pipeline substrate. iv2d decides *where* to draw; umbra decides
*what color* to write at each lane. This directory captures the steps to make
that composition real.

These are working docs. Revise them as we learn. Per CLAUDE.md, each plan has
an explicit "Expectations" block so we can compare intent against the actual
result when the work lands.

Order and dependencies
----------------------

    01 interval IR interpreter           (foundation, no deps)
    02 quadtree tile dispatcher          (needs 01)
    03 pair-lane interval JIT            (needs 01; probably not until 01/02
                                          prove CPU interval eval is a bottleneck)
    04 region coverage front-end         (independent, nicer with 02)
    05 interval-fraction alpha           (needs 01; pairs with 04)

Current state
-------------

Plans 01 and 02 landed and then evolved substantially.  The original
scalar interval interpreter (plan 01) and recursive quadtree dispatcher
(plan 02) have both been replaced.

**SDFs are the first-class shape type.**  `struct umbra_sdf` has one
`build` callback that takes `umbra_interval` arguments and returns
`umbra_interval`.  Exact intervals (lo == hi) give point evaluation;
wide intervals give conservative bounds.  The `umbra_interval_*`
helpers in `src/interval.c` emit standard builder ops — no new IR,
no JIT changes, no backend changes.  This is plan 03's variant A
realized: interval math compiled through the existing backends.

**Dispatch is a flat grid, not a quadtree.**  `umbra_sdf_dispatch`
compiles a bounds program from the SDF's `build` at construction time,
dispatches it over a tile grid in one call, then dispatches the draw
program only into tiles where `lo < 0`.  The bounds program runs on
a CPU backend (JIT preferred).  The recursive quadtree and the scalar
interval interpreter (`src/interval.h`) are both gone.

**Non-SDF coverages use direct dispatch.**  Bitmap, winding, and
texture coverages go through `umbra_draw_builder` → `be->compile` →
`prog->queue`.  No tiling, no interval math — they never benefited.

**The coverage adapter bridges SDF to the draw pipeline.**
`umbra_sdf_coverage` wraps an SDF as an `umbra_coverage` by calling
`build` with exact pixel-center intervals and converting `f` to
coverage via hard edge or soft edge.

**Plans 03, 04, 05 are largely subsumed.**  Plan 03's interval math
runs on all backends via `umbra_interval_*` (no dedicated JIT needed).
Plan 04's region authoring is just writing an `umbra_sdf` with the
interval helpers.  Plan 05's interval-fraction AA (`-lo/(hi-lo)`)
slots into the coverage adapter as a quality option.

What's next
-----------

  - **Per-pixel interval-fraction coverage.**  Call `sdf->build`
    with pixel-extent intervals in the coverage adapter, compute
    `-lo/(hi-lo)` as coverage.  This gives principled AA at boundary
    pixels.  Needs the two-program interior/boundary split to avoid
    the extra work on fully-inside tiles.

  - **Horizontal tile coalescing.**  Merge adjacent covered tiles
    into a single `draw->queue()` call to reduce per-dispatch
    overhead.  TODO in draw.c.

  - **On-device dispatch.**  The flat grid approach and the compiled
    interval math are both naturally GPU-friendly.  See
    notes-on-device-dispatch.md.

See also two architectural notes:

  - [notes-coverage-vs-sdf.md](notes-coverage-vs-sdf.md) —
    **resolved**: we went SDF-first.
  - [notes-on-device-dispatch.md](notes-on-device-dispatch.md) —
    open; the flat grid + compiled intervals make this more
    approachable than the old quadtree + scalar interpreter.

Non-goals
---------

- No wholesale port of iv2d's code. iv2d's VM and value array are strictly
  weaker than umbra's IR + CSE + JIT; we lift ideas, not files.
- No new backends. All ideas ride umbra's existing interp/jit/metal/vulkan/wgpu.
