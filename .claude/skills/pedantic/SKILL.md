---
name: pedantic
description: Move slowly and hypothesis-first when debugging. Use when a test is failing, a backend is misbehaving, or an optimization is not producing the change you expected.
---

Move slowly and pedantically.  The goal is to understand the system as it
actually is, not as you wish it were.

## Before claiming a cause

1. State the hypothesis and what measurement would confirm or refute it.
2. Take the measurement.
3. Compare result to hypothesis.  If they disagree, the hypothesis was wrong —
   do not rationalize.  Write down what the data actually says.

Hypothesis-driven changes that produce no measurable improvement are a signal
you have the wrong model of the hot path — stop and gather data before trying
the next fix.

Revert speculative changes that did not produce the expected improvement
before moving on, so the tree stays clean and the next measurement is not
confounded.

## Running out of ideas is not proof

If you have tried several hypotheses and none worked, say "I'm stuck" and ask
for help or gather more data.  Do not claim the problem is unfixable, inherent,
or impossible without a measurement that proves it.

## Read the dependency's source

When a dependency's behavior is load-bearing to what you are doing, read its
source.  Clone MoltenVK, wgpu-native, or SPIRV-Cross locally and grep for the
specific function or API path you care about.  Do not guess from API names,
blog posts, or prior knowledge — the actual code is the only ground truth.
If you catch yourself writing "I believe library X does Y", stop and verify.

## Tools

- `lldb` — step through a failing test or inspect state at a crash.  Prefer
  it over printf-debugging.
- `dumps/` — regenerate with the right tool and diff against golden files to
  compare backend output.

## Anti-patterns

- Writing code that "should work" without running it.
- Tolerating a discrepancy instead of tracing it to the exact root cause.
- Checking inputs to functions rather than outputs.
- Papering over a failure downstream with a NULL guard or fallback.
