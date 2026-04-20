Overview
--------
Umbra is a CPU/GPU pipeline JIT that promises bit-identical results across all
its backends.  `umbra_builder` binds buffers and composes simple ops into a
linear `umbra_flat_ir`, which `umbra_backend` compiles into an `umbra_program`.
`program->queue(l,t,r,b)` dispatches work against the bound buffers, which
become visible after a backend `flush()`.

  - `include/*` - public API, identifiers prefixed with `umbra_`.
  - `src/*`     - implementation, no `umbra_` prefix.
  - `src/{interpreter,jit,metal,vulkan,wgpu}.c` - backends
  - other files in src/ are generally shared infrastructure
  - `tests/`, `tools/`, `slides/`, `dumps/`.  `dumps/` holds golden shader
    dumps; commit updates to dumps in the same commit as the code change.

Claude
------
Never push to origin without the user's explicit permission.
Never amend commits.  Always create new commits.

Before each commit run `ninja` twice, once to make sure all builds and tests
pass, the second to see that `ninja` is a no-op, checking that configure.py
tracks dependencies correctly.

Create atomic, cleanly-revertable commits that contain only one kind of code
change and its related tests: a single feature's work and tests for that
functionality, or a bug fix and a regression test, or no-op refactoring.

Mandatory Style
---------------
Every line you write or modify must conform to this guide.  Do not skim this
section.  Do not treat it as advisory.  Read back every line you write and
verify it against these rules before including it in a diff.  Style violations
that reach a commit are bugs — they cost a round-trip to find and fix, and
each one erodes trust that the guide is being followed at all.

Use // TODO in the code to track bugs or anything that may need a follow up.

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

Comments should be extremely rare.  Reserve comments only for important context
that cannot be expressed through code.  Never use comments for organization or
decoration.

Use East `const`, and use it liberally, especially to distinguish locals that
name an immutable value like `double const start_time = now()` from locals that
are storage locations to hold a updating value like `double elapsed = 0;
elapsed += ...`.

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

Don't typedef public type names for enums, or for large struct or union types
that are meant to be handled by reference, but do use typedef for small value
struct or union types that are meant to be copied.

Constructors should generally just be the type name, and other methods should
be prefixed with that type name.  Let constructors' return types serve as
declarations for opaque types.

    struct foo* foo(...);
    _Bool  foo_is_bar(struct foo const*);
    void   foo_quux  (struct foo*, int x);
    void   foo_free  (struct foo*);

Prefer positive tests and let control flow nest naturally rather than
returning early or using continue.  So prefer this,

    if (can_foo) {
        foo(...);
        return 1;
    }
    return 0;

over this,

    if (!can_foo) {
        return 0;
    }
    foo(...);
    return 1;

Count bytes with `size_t` and anything else with `int`.  Name arrays in the
singular and their counts in plural, e.g. `struct inst *inst; int insts;`.  If
ambiguous, use "size" in the name of byte counts and "count" in others; always
avoid "len".  Use count() with arrays.

Use `here` with no parens for test asserts: `x == 3 here;`, never `(x == 3)
here;`.  Use assume() in non-test code.
