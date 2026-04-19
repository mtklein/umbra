---
name: perf
description: Benchmark, profile, and compare backends. Use when the task involves measuring speed, investigating a regression, comparing GPU backends, or deciding whether an optimization is worth keeping.
---

Confirm you are working on the right problem before optimizing.  Profile first,
then change code — not the other way around.

## Performance expectations on Apple Silicon

All three GPU backends run on the same Metal device:

- `metal` is our hand-written Metal backend.
- `vulkan` runs through MoltenVK, which translates Vulkan → Metal.
- `wgpu` runs through wgpu-native, which also targets Metal on macOS.

Because MoltenVK and wgpu-native both sit on top of Metal, `metal` should
always be at least as fast as `vulkan` and `wgpu`.  Anything those libraries
do, we can do directly — and without their translation overhead.  If `metal`
is slower or uses more CPU than `vulkan` or `wgpu` on a given slide, that is
a bug in our Metal backend, not a property of Metal.  Do not accept "Metal is
just more expensive here" as an explanation.

## Bench harness

- `out/host/bench --backend <name> --match <substr>` — run benchmarks.
- `--target-ms 1000 --samples 1` — hold the workload open long enough for a
  profiler to sample it.
- Run benchmarks one at a time, serially, with bounded wall time (≤30s).
  Warmup variance is accepted.
- Never run `time prog` or `until` polls without a timeout cap.
- On Rosetta, MCA is unreliable — always verify x86_64 perf numbers with
  `arch -x86_64 bench`, not static analysis.

## Profiling

- `sample <pid> <seconds> -mayDie -file <out>` — macOS call-tree profiler.
  The main-thread call graph near the top of the output usually answers
  "where is CPU actually going?" in one read.
- Start the bench with `--target-ms 1000 --samples 1` so there's time to
  attach `sample` before it exits.

## Comparing backends

- `dumps/` holds golden shader dumps.  Regenerate with the right tool and
  diff against golden files to see how backends differ.
- A shader that works on one backend but not another is a backend bug, not
  a reason to reject the shader.  All five backends must produce bit-exact
  identical output for the same IR.

## Discipline

- State a hypothesis about where the time is going before changing code.
- Take the measurement.  If the change does not produce the expected
  improvement, revert it — do not keep speculative code in the tree.
- Running out of ideas is not proof that a bottleneck is inherent.  Say
  "I'm stuck" and gather more data.
- Never remove zero-copy, batching, or leak-prevention features to "fix" a
  perf bug.  Fix the bug with the features still enabled.
