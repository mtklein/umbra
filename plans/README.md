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

Natural path of work: 01 first, then 04 and 05 to get an end-to-end iv2d-style
pipeline running on umbra's substrate without adaptive dispatch, then 02 to
turn on the performance story, then 03 only if measurements justify it.

Actual path so far: 01 then 02.  Plan 01's `interval_program` landed
with just enough op support for a proof-of-concept circle slide that
made sense to wire straight into the tile dispatcher.  After plan 02
landed, all coverage slides (solid, text, persp, slug) were migrated
to the new `umbra_draw()` / `umbra_draw_queue()` API.  Rect coverage
forced `le_f32`, `and_32`, `sel_32` into interval.c, giving solid-fill
slides quadtree dispatch (3–4× speedup on CPU backends).  Text, persp,
and slug use gathers/loops that interval.c can't bound, so they fall
back to flat dispatch through the same API.  Plans 04 and 05 remain
the natural next step — they give the quadtree richer coverage shapes
to exercise.

See also two open architectural notes — captured discussions to
re-read when drafting future plans.  Not plans themselves:

  - [notes-coverage-vs-sdf.md](notes-coverage-vs-sdf.md) —
    whether our coverage-interval dispatcher is the right shape
    vs. iv2d's sdf-interval approach.  Interacts with plans 04, 05.
  - [notes-on-device-dispatch.md](notes-on-device-dispatch.md) —
    whether the quadtree recursion should run on the CPU (as it
    does now) or inside the GPU dispatch itself.  Interacts with
    plans 03, 04 and with the coverage-vs-sdf note above.

The two notes are independent axes but they multiply: on-device
dispatch is dramatically easier under an SDF-first architecture
than under our current coverage-first one.

What "absorbing iv2d's key ideas" means here
--------------------------------------------

In descending order of leverage:

  1. Interval arithmetic as a *second interpretation* of the existing IR —
     every pipeline becomes a bounds estimator for free. Plan 01.
  2. Adaptive tile dispatch driven by those bounds — per-pixel work only on
     boundary tiles. Plan 02.
  3. A small builder-style API for writing implicit shapes, feeding the
     existing `umbra_coverage` trait. Plan 04.
  4. Interval-fraction α as a cheap, principled sub-pixel coverage estimate.
     Plan 05.
  5. If and only if measurements demand it: pair-lane interval math in the
     JIT so interval evaluation scales with the K-lane substrate. Plan 03.

Non-goals
---------

- No wholesale port of iv2d's code. iv2d's VM and value array are strictly
  weaker than umbra's IR + CSE + JIT; we lift ideas, not files.
- No new public types like `iv32` in umbra's public headers. Intervals stay
  an IR-level concern.
- No new backends. All ideas ride umbra's existing interp/jit/metal/vulkan/wgpu.
