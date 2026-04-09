# TODO

Notes from the uniform_ring + backpressure + pipelined-cohorts arc.
Roughly in order of "value per effort" within each section.

## Small things to fix in a tomorrow-pass

- `prev_fence` field in `struct metal_backend` lives next to
  `frame_committed[]` but is per-cmdbuf compute-serialization, not
  per-frame. Worth a one-line comment clarifying that distinction. The
  current comment is correct but doesn't acknowledge the rotation reset.

- `umbra_metal_flush` and `vk_flush` both call `drain_frame(0); drain_frame(1);`
  after `submit_cmdbuf`. `submit_cmdbuf` already drained the new cur_frame,
  so one of those two calls is always a no-op. Could be
  `drain_frame(cur_frame ^ 1)` (the just-submitted one). Loop is harder to
  mess up but reads as confusing.

- The dump tool's `slugify` produces `dumps/animated_t_in_uniforms/`. The
  parens get squashed to underscores. Cosmetic.

## Things that aren't broken but worth reconsidering

- `run_long_batch_no_oom` smoke test doesn't actually verify backpressure
  triggered. It runs N=12 000 dispatches and checks the final pixel value;
  if backpressure were silently disabled the test would still pass
  (corruption threshold is much higher). The animated bench at `--ms 8000`
  is the canonical regression and the test comment says so, but the test
  doesn't *prove* the mechanism is wired. A cleaner shape: expose a
  rotation counter on each backend and assert it incremented. ~20 lines on
  each side.

- `uniform_ring` lifetime ordering. `uniform_ring_free` calls each chunk's
  `free_chunk` callback, which on the backends uses the device pointer. So
  the backend MUST call `uniform_ring_free` before destroying the device.
  Both backends do this correctly because of code ordering, but it isn't
  documented. A header comment would help.

- `metal_drain_frame`/`vk_drain_frame` are mirror-image functions in
  different files. Same for `metal_submit_cmdbuf`/`vk_submit_cmdbuf`. Can't
  share code (different APIs), but the parallel structure could be
  enforced more explicitly — e.g. by sketching the per-backend algorithm
  in `uniform_ring.h` as the canonical "this is what you should do, don't
  drift". The current parity is enforced by hand-comparison.

- No hard cap on chunk count. The ring can grow without bound if a single
  batch needs more than `threshold` of contiguous space (rare — only
  happens for >64 KiB single uniforms). Adding a cap would force a runtime
  error rather than unbounded memory growth. Not a real issue in practice.

- Both backends keep `cache_buf` alive in parallel with the ring. The
  split is "writable + row-structured + deref'd → cache_buf" and
  "read-only flat → ring". Two separate caching mechanisms with different
  lifetime rules per backend. A long-term cleanup would route the deref'd
  read-only path through the ring too (with content hashing to preserve
  the writable→readonly handoff). Originally noted as a follow-up in the
  metal route-to-ring commit message and stayed deferred.

## Cut corners I'm aware of

- The pipelined design uses `[cmdbuf waitUntilCompleted]` and
  `vkWaitForFences` for "wait for cohort done". Hard CPU stall. The
  "right" answer is `MTLSharedEvent` / timeline `VkSemaphore` with async
  completion handlers. We picked simple-first. Wait points are now rare
  enough (one per ring rotation = one per ~4 k dispatches at 64 KiB) that
  the stall is invisible. Would only matter if a future workload encodes
  much faster than this.

- No backpressure-event counter exposed for testing. Same as the test
  point above.

- Bench measurement methodology. The "double iters until elapsed > min_secs"
  pattern is sensitive to noise — one slow round during the iter-doubling
  can converge at half the iters and inflate ns/px. Saw this manifest as
  bimodal noise during the threshold sweep. A cleaner bench would do
  warmup + N median-of-K runs + drop-outlier filtering.

- Error handling on Vulkan chunk allocation. `vkCreateBuffer` and
  `vkAllocateMemory` can fail; we don't check return codes in
  `vk_ring_new_chunk`. On failure we'd crash later when binding. Codebase
  uses `assume()` for unrecoverable Vulkan failures elsewhere; ring's
  new_chunk doesn't follow that pattern. ~3 lines of `assume()`.

- Animated slide test depends on `prepare()` resetting `t = 0` for
  golden_test correctness. Brittle: any slide that mutates state in
  `draw()` has the same coupling. Working around it with a "frame index"
  passed as a parameter to `draw()` would be cleaner, but bench's `draw()`
  signature doesn't accept that.

- `build/xsan.ninja` and `build/x86_64h_xsan.ninja` have hand-maintained
  lists of expected dump outputs. Adding `slides/anim.c` required
  hand-editing both files. Easy to forget. A pattern-based rule would be
  cleaner.

## Future / deferred work (not problems, things we didn't do)

- Pipelined N=3 if a workload ever shows benefit. We have no evidence it
  would help. Easy change if we want it.

- MoltenVK-specific tuning. MoltenVK adds translation overhead per Vulkan
  call. We don't pay attention to this anywhere — just use Vulkan as if
  native. If we ever ran on real Vulkan hardware (Linux, Windows) the
  picture might be different.

- Larger uniform payloads. The ring handles them via `min_bytes` in
  `new_chunk`, but every test/bench so far uses small (≤80 B) uniforms.
  Untested for, say, 4 KiB uniform blocks. Code path is straightforward
  but unexercised.

- Multi-threaded encoding. Current backends assume single-threaded
  encode. If a client wanted to encode dispatches from multiple threads,
  we'd need locks around `cur_frame` and the rings.

- Content cache as the alternative to ring for the slug-curves-style
  "stable host pointer, large blob" case. The original discussion landed
  on ring; content cache is still viable for that specific path if we
  ever care.

- Push constants for tiny user uniforms as an optimization. We
  deliberately route ALL user uniforms through the ring for parity. If a
  measurable win showed up for some workload, we could re-add push
  constants as an optimization layer in front of the ring. For slug at
  80 B it'd save ~7 setBuffer calls per round in favor of a single
  push-constant write. Probably not measurable.

## Priority ranking if picking from the list

If I had a tomorrow:

1. Add the rotation counter and use it in the smoke test (real
   correctness improvement).

Everything else is fine to leave.
