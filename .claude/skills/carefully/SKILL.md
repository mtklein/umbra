---
name: carefully
description: Work on a problem carefully — predict before, measure during, grade after. Use for features, bug fixes, refactors, optimizations, regressions, backend disagreements, or anything where guessing would be worse than proving. The user names the problem; apply predict / hypothesize / profile discipline as fits.
---

The user has named a problem to work on.  Solve it carefully: commit to
predictions up front, move hypothesis-first while investigating, profile
before optimizing, and grade the actual result against what you said would
happen.  The goal is to understand the system as it actually is — not as you
wish it were — and to prove through correctness and performance testing that
the code you wrote works.

## 1. Before starting — predict

Write out expectations for the work, in plain terms the user can read.  Three
or four short bullets is enough; the point is to commit to a claim, not to
write a design doc.

  - **Correctness** — what behavior changes?  Which existing tests must still
    pass?  What new test would fail today but pass after the change?
  - **Performance** — if the change could affect speed, what do you expect
    `bench` to show?  "No expected change" is a valid prediction.  If you do
    not know, say so.
  - **Code quality** — what is the shape of the diff?  How many files, how
    many lines, any new abstractions?  What would make you want to revert it
    in review?

Skip this step only for trivial one-line changes with no plausible surprise,
pure exploration or read-only tasks, or work where you already wrote
predictions earlier in the session and the task has not meaningfully changed.

## 2. While working — hypothesis-first

Move slowly.  Before claiming a cause or committing to a fix:

  1. State the hypothesis and what measurement would confirm or refute it.
  2. Take the measurement.
  3. Compare result to hypothesis.  If they disagree, the hypothesis was
     wrong — do not rationalize.  Write down what the data actually says.

Hypothesis-driven changes that produce no measurable improvement are a signal
you have the wrong model of the hot path — stop and gather data before trying
the next fix.  Revert speculative changes that did not produce the expected
effect before moving on, so the tree stays clean and the next measurement is
not confounded.

**Running out of ideas is not proof.**  If you have tried several hypotheses
and none worked, say "I'm stuck" and ask for help or gather more data.  Do
not claim the problem is unfixable, inherent, or impossible without a
measurement that proves it.

**Read the dependency's source.**  When a dependency's behavior is load-bearing
to what you are doing, read its source.  Clone MoltenVK, wgpu-native, or
SPIRV-Cross locally and grep for the specific function or API path you care
about.  Do not guess from API names, blog posts, or prior knowledge — the
actual code is the only ground truth.  If you catch yourself writing "I
believe library X does Y", stop and verify.

## 3. For perf work — profile first

Confirm you are working on the right problem before optimizing.  Profile first,
then change code — not the other way around.  State a hypothesis about where
the time is going before changing code.  Take the measurement.  If the change
does not produce the expected improvement, revert it.

### Bench harness

- `out/host/bench --backend <name> --match <substr>` — run benchmarks.
- `--target-ms 1000 --samples 1` — hold the workload open long enough for a
  profiler to sample it.
- Run benchmarks one at a time, serially, with bounded wall time (≤30s).
  Warmup variance is accepted.
- Never run `time prog` or `until` polls without a timeout cap.
- On Rosetta, MCA is unreliable — always verify x86_64 perf numbers with
  `arch -x86_64 bench`, not static analysis.

### Profiling

- `sample <pid> <seconds> -mayDie -file <out>` — macOS call-tree profiler.
  The main-thread call graph near the top of the output usually answers
  "where is CPU actually going?" in one read.
- Start the bench with `--target-ms 1000 --samples 1` so there's time to
  attach `sample` before it exits.

### Apple Silicon backend expectations

All three GPU backends run on the same Metal device: `metal` is hand-written,
`vulkan` goes through MoltenVK, `wgpu` through wgpu-native.  Because MoltenVK
and wgpu-native both sit on top of Metal, `metal` should always be at least
as fast as `vulkan` and `wgpu`.  If `metal` is slower or uses more CPU than
`vulkan` or `wgpu` on a given slide, that is a bug in our Metal backend, not
a property of Metal.  Do not accept "Metal is just more expensive here" as
an explanation.

Never remove zero-copy, batching, or leak-prevention features to "fix" a
perf bug.  Fix the bug with the features still enabled.

## 4. For debugging and backend comparison — tools

- `lldb` — step through a failing test or inspect state at a crash.  Prefer
  it over printf-debugging.
- `dumps/` — regenerate with the right tool and diff against golden files to
  see how backends differ.  A shader that works on one backend but not
  another is a backend bug, not a reason to reject the shader — all backends
  must produce bit-exact identical output for the same IR.

## 5. After finishing — grade

Compare the actual result to what you wrote at step 1:

  - Did the tests behave as predicted?  If a test you expected to fail
    passed anyway, the change may be a no-op.  If one you expected to pass
    failed, the model was wrong.
  - Did the perf number match?  If not, do not rationalize — say so and
    figure out why.
  - Did the diff shape match?  A "small fix" that grew to 400 lines across
    six files is a signal that scope drifted.

If expectations and reality disagree, the expectation was wrong or the code
is wrong.  Write down which, and what you learned.  Do not quietly update
the prediction to match the outcome.

## Anti-patterns

- Writing code that "should work" without running it.
- Tolerating a discrepancy instead of tracing it to the exact root cause.
- Checking inputs to functions rather than outputs.
- Papering over a failure downstream with a NULL guard or fallback.
- Claiming a problem is inherent because you ran out of ideas.
- Leaving speculative changes in the tree after they did not pan out.
