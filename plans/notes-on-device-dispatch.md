On-device quadtree dispatch: a sibling architectural question
==============================================================

Status: open; recording the discussion, not resolving it.
Interacts with: notes-coverage-vs-sdf.md.

What's the idea
---------------

Today's dispatcher runs on the CPU: `queue_recurse` in src/draw.c
recursively subdivides a rect, evaluates `interval_program_run` at
each level, and issues one `umbra_program->queue(...)` call per leaf
tile.  N tiles per frame → N command-buffer-encode round-trips.

Proposed alternative: push the recursion *onto the device*.  CPU
issues **one** dispatch per coverage per frame.  Inside that dispatch,
threadgroups (or persistent threads, or a work-stealing scheduler)
perform the quadtree themselves — interval-prune, split, sub-dispatch
subtiles, bottom out in per-pixel coverage work when the tile is
small enough.  Per-sub-tile cost drops from "a command-buffer
round-trip" to "a few kernel ops."

What that unlocks
-----------------

**MIN_TILE can drop to whatever is actually cost-optimal for the math.**
The whole 32× spread between CPU-optimal (~16) and GPU-optimal (~1024)
MIN_TILE from plan 02's sweep is *entirely* a reflection of per-dispatch
fixed cost.  Remove it, and GPU MIN_TILE can probably drop to ~16 too.
That recovers the missing ~2× on metal/vulkan and the ~7× on wgpu that
we left on the table when we compromised at 512.

**Sub-pixel recursion becomes practical.**  iv2d's full-depth
quadtree-to-sub-pixel approach, averaging interval-fraction α at the
leaves, is straight compute-kernel math.  No round-trip constraints.
This is probably the cleanest way to get iv2d-quality AA without
paying supersample rates at the shader level.

**One dispatch model serves every backend.**  No more per-backend
MIN_TILE tuning, because the scheduler is on-device and its "tile
size" decisions are driven by kernel-local cost, not pipeline cost.

What it costs
-------------

**An on-device interval evaluator.**  The interval math has to run
inside the shader.  Two flavors:

  - For a **coverage-interval** IR (what we have today), this means
    a per-op interval implementation in each backend's shader language
    — effectively plan 03's pair-lane JIT, reskinned as "emit Metal
    Shading Language / SPIR-V / WGSL instead of arm64/x86."  3×
    backend effort, non-trivial per backend.

  - For an **SDF-interval** scheme (see notes-coverage-vs-sdf.md), this
    is drastically simpler: a tile's SDF bound is `f(center) ± D/2`
    under the Lipschitz assumption — one sample and one widen.  Closed
    form, trivially expressible in any shader language.  Combinators
    are shader-level min/max/abs.

  This is where the two open architectural questions collide: *on-device
  dispatch amplifies the SDF-first argument.*  Coverage-IR-on-device is
  a substantially bigger project than SDF-on-device.

**A work scheduler inside the shader.**  Recursion isn't native in
most shader languages, but GPU-compute idiom supports several
approaches:

  - **Threadgroup-local stack + loop**: each threadgroup takes a root
    tile, iterates a local stack of "tiles to process" with explicit
    push/pop.  Depth-bounded; no dynamic dispatch.
  - **Persistent threads + work queue**: a pool of threads polls a
    global queue of tiles, subdividing and pushing back on split.  More
    general; more synchronization.
  - **Indirect dispatch + multi-pass**: emit a worklist buffer, dispatch
    again on it, repeat until convergence.  Simplest to reason about;
    more CPU round-trips per frame.

  One of these per backend, with backend-specific tuning.

**CPU backends (`interp`, `jit`) gain nothing.**  They already have
near-zero per-dispatch overhead; at MIN_TILE=16–32 they beat flat
baseline by 5–7× (plan 02 measurements).  Moving recursion onto the
"CPU device" is just running the same recursion we already have.  So
this is a GPU-specific win only.

**Loses the clean umbra_program abstraction.**  Each on-device
dispatcher is a different kind of pipeline — more "compute kernel
orchestrator" than "per-pixel shader."  Doesn't compose with the
existing `(compile → queue → flush)` model without some reshaping.

**Debugging is harder.**  Host-side recursion is traceable with a
debugger.  On-device recursion with threadgroup stacks and work
queues is nearly opaque; you're usually left dumping buffers to
figure out what happened.

Where the benefit actually lives
--------------------------------

Quantifying roughly, from plan 02's circle slide at 4096×480 (~1%
coverage), comparing flat vs MIN_TILE=512 vs plausible on-device
MIN_TILE=16:

  backend   flat        our MIN_TILE=512    hypothetical MIN_TILE=16
    metal   0.05 ns/px       0.03              ~0.01    (3× beyond current)
   vulkan   0.06             0.04              ~0.02
     wgpu   0.07             0.06              ~0.03    (largest gain)
      jit   0.86             0.17              0.17     (no change, not device)
   interp   2.48             0.47              0.47     (no change, not device)

The GPU wins are real but not enormous relative to the investment.
The `jit`/`interp` path doesn't benefit at all.  So this is
justifiable only if the intended workload is GPU-dominated AND we
want sub-pixel AA quality that the current partial-coverage shader
can't match.

Orthogonality to coverage-vs-SDF
--------------------------------

  |                        | Coverage-interval      | SDF-interval               |
  | ---------------------- | ---------------------- | -------------------------- |
  | CPU-driven quadtree    | ← what we have today   | plan 04's charitable path  |
  | On-device quadtree     | requires per-backend   | clean/cheap — iv2d math is |
  |                        | interval evaluators    | already shader-friendly    |

They're mostly independent axes but they multiply: the on-device
path is *dramatically* more attractive on the SDF side.  If we
commit to SDF-first (the charitable future in the sibling note),
on-device dispatch becomes a natural follow-on plan — maybe a half-
sized effort, because the interval math is already designed for it.
If we stay coverage-first, on-device becomes a much bigger lift
(effectively merging with plan 03).

Shape of a plan, if we pursued this
-----------------------------------

Rough sketch, to be revisited:

  - **Gate**: measured evidence that GPU MIN_TILE=512 is leaving real
    performance on the table on realistic workloads.  Circle-on-idle
    isn't enough; need a busy scene where the current dispatch model
    is the bottleneck.
  - **Scope**: one backend first (probably Metal, since it has the
    cheapest encode overhead to beat and the most familiar compute
    model).  Prove the architecture before porting.
  - **Shape**: threadgroup-local stack approach is probably the
    simplest starting point — no multi-pass dispatch, no cross-
    threadgroup coordination, bounded depth.  Escalate to
    persistent-thread work queue only if we measure starvation on
    uneven tile distributions.
  - **Interval math**: piggyback on the SDF path.  If plan 04 lands
    umbra_sdf as a first-class type with a Lipschitz `bound()`, its
    interval eval transplants to the shader basically unchanged.  The
    coverage-IR path would be a much bigger project requiring plan
    03's pair-lane JIT retargeted to shader languages.

  Probably a 3–5 plan sequence if pursued seriously:

    P - single-backend (Metal) on-device dispatcher, SDF-only
    P - extend to vulkan + wgpu
    P - sub-pixel recursion for AA quality
    P - tuning and workload bring-up

Open items to revisit
---------------------

- **Real GPU workload.**  Does the 2–3× we'd gain on metal/vulkan
  beyond MIN_TILE=512 matter for what the system is actually drawing?
  We haven't measured on a "real" scene yet.  The circle-on-idle
  numbers may overstate the pure-dispatch-overhead case.

- **Sub-pixel AA.**  Is current partial-coverage shader AA good
  enough for target use cases, or do we need iv2d-style sub-pixel
  recursion?  If the former, one of the two main wins evaporates.

- **Work-distribution quality.**  A naïve threadgroup-local approach
  can starve: one threadgroup gets all the boundary tiles, others
  finish early and sit idle.  Real work-stealing or a persistent-
  thread queue would fix it at the cost of more complexity.  Worth
  prototyping before scoping.

- **Does this replace or coexist with the current CPU dispatcher?**
  CPU path doesn't benefit from on-device dispatch (see above), so
  the dispatcher would have two implementations — `queue_recurse`
  on the host for CPU backends, an on-device kernel for GPU
  backends.  The `umbra_draw_queue` public API stays the same; the
  internal branch on backend type picks the dispatcher.

- **Retrofit path.**  If we land plans 04/05 (SDF-first with
  interval-fraction α baked in) and the charitable future holds,
  this plan becomes natural.  If we stay coverage-first, this plan
  grows to subsume plan 03.  Either way, on-device dispatch is the
  endpoint of whichever architectural thread we pull on.

What this note is and isn't
---------------------------

Not a plan; no action to take directly from it.  A capture of the
architectural option so that when we revisit GPU performance
(either because it becomes a bottleneck, or because sub-pixel AA
quality matters) we don't re-derive the analysis.

Worth re-reading when:
  - planning plan 04's SDF-first shape (informs how intervalizable-on-
    device it needs to be),
  - looking at realistic GPU workloads and finding flat+MIN_TILE=512
    leaves real performance on the table,
  - deciding whether plan 03 (pair-lane JIT) is the right move or
    whether on-device dispatch subsumes it.
