03 — Pair-lane interval JIT
===========================

Status: not started, speculative.
Depends on: 01 (semantic reference), 02 (consumer of fast interval eval).
Gate: measured evidence that scalar interval eval from (01) is a real
bottleneck.

Plan 02 landed with measured interval overhead at ~19 ns/call on native
arm64 and ~46 ns/call on x86_64h (a 20-op circle SDF; see
`tools/bench_interval.c`).  At QUEUE_MIN_TILE=512 on a 4096×480 canvas the
dispatcher makes ~40 `interval_program_run` calls per frame, on the order
of 1 µs total.  Nowhere near a bottleneck.  A pair-lane JIT would speed
this up 5–20× per the hypothesis below, saving us <1 µs per frame at the
currently measured workloads.  Do not start this plan until a real
scenario (deep quadtrees, per-pixel interval math, or densely-nested
regions) produces a profile where interval eval is the clear hot spot.

Goal
----

If CPU scalar interval evaluation becomes the bottleneck — e.g. deep
quadtrees on huge viewports, or per-pixel interval math on a shape layer —
teach the umbra JIT backends to emit interval arithmetic directly, with each
value represented as a pair of K-lane SIMD registers (`lo`, `hi`).

Why
---

iv2d evaluates intervals only at quadtree corners, so sparse scalar eval is
fine. This plan exists so we know the escape hatch if (01)(02) plateau on
interval-eval overhead, or if we decide to push interval analysis into the
per-pixel path (e.g. for dense adaptive supersampling).

Not urgent. Do not start until profiling shows interval eval is hot.

Current state
-------------

- JIT represents a value as a register (K-lane). No multi-component values
  beyond the RA's existing `chan : 2` bookkeeping in `val`.
- `i16_from_i32`, `i32_from_s16` etc. already use channels for split values.
- `src/ra.c`/`ra.h` exposes `ra_ensure_chan`, `ra_free_chan`, `ra_chan_reg` —
  the scaffolding is in place.

Two candidate representations
-----------------------------

A. **Parallel builder, existing ops.**

   An `umbra_iv_*` builder layer that, per interval op, emits two or more
   existing IR nodes for `lo` and `hi`. For example:

       umbra_iv_add_f32(b, a, c):
           lo = umbra_add_f32(b, a.lo, c.lo);
           hi = umbra_add_f32(b, a.hi, c.hi);
           return (iv_val){lo, hi};

       umbra_iv_mul_f32(b, a, c):
           four = {min,min,max,max of four products};
           return {min_f32(four), max_f32(four)};

   Pros: zero changes to op set, IR, RA, backends. CSE still works.
   Cons: `mul` blows up to ~10 existing ops; codegen may be suboptimal.

B. **First-class interval ops.**

   New ops `add_iv_f32, sub_iv_f32, mul_iv_f32, ...` that consume
   `(a.lo, a.hi, b.lo, b.hi)` and produce `(r.lo, r.hi)`. Each backend
   implements these once with tight hand-written code.

   Pros: tightest codegen, especially for `mul_iv_f32` which has a natural
   min/max reduction.
   Cons: touches op.h, op.c, the hash, the interpreter, RA channel handling,
   every backend. High cost.

Strong lean toward A. Only adopt B if A proves measurably slow after
landing and someone has a concrete profile pointing at a specific op.

Files touched (variant A)
-------------------------

- new: `include/umbra_interval.h` — parallel builder API (follow plan 01's
  `interval_*` naming, not the `iv_*` shorthand used in earlier drafts)
- new: `src/interval_builder.c` — thin wrapper that emits the op sequences
- new: `tests/interval_jit_test.c`

Files touched (variant B, if we ever do it)
-------------------------------------------

- `src/op.h`, `src/op.c`: new ops
- `src/interpreter.c`, `src/jit_arm64.c`, `src/jit_x86.c`: code each new op
- `src/spirv.c`, `src/metal.m`, `src/vulkan.c`, `src/wgpu.c`: ditto
- `src/ra.c`: handle paired-channel alloc/free for `_iv_` ops
- everything in variant A's file list, too

Testing
-------

- For each builder helper, eval the same expression via (01)'s scalar
  interval interpreter. Expect bit-exact lo/hi on affine ops, conservative
  bounds matching within ULP on mul/div/sqrt.
- Backend bit-exactness across interp/jit/metal/vulkan/wgpu (per
  `feedback_all_backends_equivalent`).
- Stress: compare 10k random input intervals against scalar reference.

Expectations
------------

Performance: if (02)'s interval eval is the bottleneck, expect 5–20× speedup
from K-lane SIMD. If it is not the bottleneck, expect no visible change and
we should not have done this plan yet.

Code quality (variant A): small, contained, additive. No risk to existing
paths.

When work starts, record the profile motivating it and the variant chosen.

Open questions
--------------

- How do we express "sel_32 union" (i.e. if-then-else on intervals) when
  the mask is itself interval-valued? Conservative answer: treat mixed mask
  as union of both sides; code in A would be a pair of sels with a widening.
- Can we reuse channels (`val.chan`) to hold `hi` implicitly, or do we need
  explicit paired IDs? Easier to start explicit and compact later.
- Loop/if interval analysis remains out of scope here; that lives in (01).
- Per plan 02's "keep interval.c narrow" lesson: a pair-lane JIT
  replicates interval.c's op set in codegen form.  Whatever op-support
  gaps interval.c has *today* (sel_32, compares, gather, integer ops)
  propagate here.  Plan 03 should not grow the op set — it should make
  what interval.c already supports run faster.  Expanding op coverage
  lives in plan 04's slide-porting work.

Non-goals
---------

- No new dispatch model (that's (02)).
- No public `iv32` type in `umbra.h`. Keep intervals an IR-level concern.
