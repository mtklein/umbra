Claude and other AIs
--------------------
Your calling is not just to write good working code, but to prove through
correctness and performance testing that you have written good working code.

When you're about to start working, explicitly write out your expectations
about the work's impact on correctness, performance, code quality.  Then when
you are done, compare the actual results with your written expectations.

Make strong claims only with strong proof, but take time for research to gather
that proof.  Identify any unproven statements clearly as your best guess, your
perception of conventional best practice, etc.

The user has final say on all decisions, especially policy and design.

Never push to origin without the user's explicit permission.

Contributing
------------
Never amend commits.  Always create new commits.

Before each commit run `ninja` twice, once to make sure all modes build and all
tests pass, the second to make see that `ninja` is a no-op, checking that our
build files track dependencies correctly.

Create atomic, cleanly-revertable commits that contain only one kind of code
change and its related tests: a single feature's work and tests for that
functionality, or a bug fix and a regression test, or no-op refactoring.

Untested code in unacceptable.

Use // TODO in the code to track bugs or anything else that needs a follow up.

Style
-----
Every line you write or modify must conform to this guide.  Do not skim this
section.  Do not treat it as advisory.  Read back every line you write and
verify it against these rules before including it in a diff.  Style violations
that reach a commit are bugs — they cost a round-trip to find and fix, and
each one erodes trust that the guide is being followed at all.

Indent with 4 spaces, wrap to 100 columns, and always use {}.  Almost always
begin a new line after {, even for short expressions, except prefer to align
parallel constructs vertically.  It's nice to vertically align consecutive
related assignments of the same type, with one shared type declaration:

    if (inst.x) { foo(&inst, x); }
    if (inst.y) { foo(&inst, y); }

    int const foo = fn(inst.x),
              bar = fn(inst.y),
              baz = fn(inst.z),
             quux = fn(inst.w);
    float const not_int = ...;

Write code that is clear without comments.  If you're thinking of adding a
comment to clarify, first see if refactoring or better identifiers can make it
clear.  Reserve comments only for important context that cannot be expressed
through code.  Never use comments for organization or decoration.

Use East `const`, and use it liberally, especially to distinguish locals that
name a value like `double const start_time = now()` from locals that are
locations to hold a updating value like `double elapsed = 0; elapsed += ...`.

But use const pointers only in the extremely unusual situations where it helps
clarify which of several pointers will be mutating and which won't.
Pointer-to-const is such an important concept that we want almost all uses of
pointers and const together to be for pointer-to-const:

    int const x = ...;         // Great!
    int const *p = &x;         // Extremely valuable, especially as function arguments.
    int const *const p = &x;   // Confusing, avoid this unless really helpful for clarity.
    int       *const p = ...;  // Avoid this especially, looks too much like int const *p.

Pointer `*` placement has two rules and they differ:

  - Variables and parameters: `*` on the name.    `int *p`, `char const *s`
  - Function return types:    `*` on the type.    `struct foo* foo(...)`, `char const* name(...)`

Never write `struct foo *foo(` — that puts `*` on the name for a return type.
Never write `int* p` — that puts `*` on the type for a variable.
Never leave `*` floating alone with spaces on both sides.

    struct foo* foo(int *bar, int const*);    // return type * on type, params * on name

Keep headers especially tidy, with minimal #includes and only inline functions
when absolutely necessary, e.g. when using a macro like `__LINE__`.

Don't shorten public type names for enums, or for large struct or union types
that are meant to be handled by reference, but do use typedef for small value
struct or union types.  Constructors should generally just be the type name,
and other methods should be prefixed with that type name.  Let constructors'
return types serve as declarations for opaque types.

    typedef struct {
        float x,y;
    } point;

    struct foo* foo(...);
    _Bool  foo_is_bar(struct foo const*);
    void   foo_quux  (struct foo*, int x);
    void   foo_free  (struct foo*);

Prefer positive tests that let control flow nest naturally rather than
returning early or using continue.  So prefer this style,

    if (can_foo) {
        foo(...);
        return 1;
    }
    return 0;

over this style,

    if (!can_foo) {
        return 0;
    }
    foo(...);
    return 1;

Count bytes with `size_t` and anything else with `int`.  Name arrays in the
singular and their counts in plural, e.g. `struct inst *inst; int insts;`.  If
ambiguous, use "size" in the name of byte counts and "count" in others; always
avoid "len".

Use `here` with no parens for test asserts: `x == 3 here;`, never
`(x == 3) here;`.  Use assume() in non-test code.

Project structure
-----------------
Umbra is a small CPU+GPU pipeline JIT.  A `umbra_builder` composes ops into an
IR; `umbra_flat_ir` is the linearized form; `umbra_backend` compiles the IR
into a `umbra_program` whose `queue(l,t,r,b,buf[])` dispatches a rect of work
against caller-bound buffers.

  - `include/` — public API (`umbra.h`, `umbra_draw.h`).  `umbra_` prefix is
    reserved for public API; internal code in `src/` must not use it.
  - `src/` — builder, ops (`op.h`), basic blocks, RA, backends.
  - `src/{interp,jit,metal,vulkan,wgpu,spirv}.c` — the five backends.  JIT
    splits into `jit.c` (shared) + `jit_arm64.c` + `jit_x86.c`.  SPIR-V
    generation is shared between `vulkan.c` and `wgpu.c` via `spirv.c`.
  - `src/gpu_buf_cache.{h,c}` and `src/uniform_ring.{h,c}` — shared GPU infra.
  - `tests/`, `tools/`, `slides/`, `dumps/`.  `dumps/` holds golden shader
    dumps; commit updates to dumps in the same commit as the code change.

All five backends must produce bit-exact identical output for the same IR.
A shader that works on one backend but not another is a backend bug, not a
reason to reject the shader.

Performance expectations
------------------------
On Apple Silicon we run all our GPU backends on the same Metal device:

  - `metal` is our hand-written Metal backend.
  - `vulkan` runs through MoltenVK, which translates Vulkan → Metal.
  - `wgpu` runs through wgpu-native, which also targets Metal on macOS.
  - (`interp` and `jit` are CPU-only.)

Because MoltenVK and wgpu-native both sit on top of Metal, `metal` should
always be at least as fast as `vulkan` and `wgpu`.  Anything those libraries
do, we can do directly — and without their translation overhead.  If `metal`
is slower or uses more CPU than `vulkan` or `wgpu` on a given slide, that is
a bug in our Metal backend, not a property of Metal.  Do not accept "Metal is
just more expensive here" as an explanation.

Debugging and profiling
-----------------------
Confirm you are working on the right problem before optimizing.  Hypothesis-
driven changes that produce no measurable improvement are a signal you have
the wrong model of the hot path — stop and profile before trying the next
fix.  Running out of ideas is not proof; say "I'm stuck" and gather data.

Tools for gathering that data:

  - `out/host/bench --backend <name> --match <substr>` — our benchmark
    harness.  Use `--target-ms 1000 --samples 1` to hold the workload open
    long enough for sampling.
  - `sample <pid> <seconds> -mayDie -file <out>` — macOS call-tree profiler.
    The main-thread call graph near the top of the output usually answers
    "where is CPU actually going?" in one read.
  - `lldb` — for stepping through a failing test or inspecting state at a
    crash.  Prefer it over printf-debugging.
  - `dumps/` — regenerate with the right tool and diff against golden files
    to compare backends' shader output.

When a dependency's behavior is load-bearing to an optimization you are
considering, read its source.  Clone MoltenVK, wgpu-native, or SPIRV-Cross
locally and grep for the specific function or API path you care about.  Do
not guess from API names, blog posts, or prior knowledge — the actual code
is the only ground truth.  If you catch yourself writing "I believe library
X does Y", stop and verify.

Move slowly and pedantically when debugging.  Before claiming a cause:

  1. State the hypothesis and what measurement would confirm or refute it.
  2. Take the measurement.
  3. Compare result to hypothesis.  If they disagree, the hypothesis was
     wrong — do not rationalize.  Write down what the data actually says.

Revert speculative changes that did not produce the expected improvement
before moving on, so the tree stays clean and the next measurement is not
confounded.
