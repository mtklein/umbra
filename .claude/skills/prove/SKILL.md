---
name: prove
description: Write down expectations before non-trivial work, then grade the actual result against them afterward. Auto-invoke before starting a feature, bug fix, refactor, or optimization — any task where a prediction is meaningful. Skip for trivial one-line changes, pure questions, or read-only exploration.
---

The job is not just to write working code, but to prove through correctness
and performance testing that you have written working code.  The way to prove
it is to commit to a prediction up front, then check the result against that
prediction.

## Before starting

Write out expectations for the work, in plain terms the user can read:

  - **Correctness** — what behavior changes?  Which existing tests must still
    pass?  What new test would fail today but pass after the change?
  - **Performance** — if the change could affect speed, what do you expect
    `bench` to show?  (If you do not know, say so.  "No expected change" is
    also a valid prediction.)
  - **Code quality** — what is the shape of the diff?  How many files, how
    many lines, any new abstractions?  What would make you want to revert it
    in review?

Keep it brief.  Three or four short bullets is enough.  The point is to
commit to a claim, not to write a design doc.

## After finishing

Compare the actual result to what you wrote:

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

## When to skip

- Trivial one-line changes with no plausible surprise.
- Pure exploration, questions, or read-only tasks.
- Work where you have already written predictions earlier in the session
  and the task has not meaningfully changed.
