Overview
--------
Umbra is a CPU/GPU pipeline JIT that promises bit-identical results across all
its backends.  `umbra_builder` binds buffers and composes simple ops into a
linear `umbra_flat_ir`, which an `umbra_backend` compiles into an
`umbra_program`.  `program->queue(l,t,r,b)` dispatches work against the bound
buffers, which become visible after a backend `flush()`.

  - `include/*` - public API, identifiers prefixed with `umbra_`.
  - `src/*`     - implementation, no `umbra_` prefix.
  - `src/{interpreter,jit,metal,vulkan,wgpu}.c` - backends
  - other files in src/ are generally shared infrastructure
  - `tests/`, `tools/`, `slides/`, `dumps/`.  `dumps/` holds golden shader
    dumps; commit updates to dumps in the same commit as the code change.

Contributing
------------
Never push to origin without the user's explicit permission.
Always create new commits; never amend commits.

Before each commit run `ninja` twice, once to make sure all builds and tests
pass, then again to see that `ninja` is a no-op, checking that configure.py
tracks dependencies correctly.

Create atomic, cleanly-revertable commits that contain only one kind of code
change and its related tests: a single feature's work and tests for that
functionality, or a bug fix and a regression test, or no-op refactoring.

Use // TODO in the code to track bugs or anything that may need a follow up.

Code Style
----------
Write comments only for important context that is not obvious and cannot be
expressed through code.  Never use comments for organization or decoration.  If
you think you need a comment, see if you can express that information better by
refactoring, improving identifier names, or by using the type system.

Indent with 4 spaces, wrap to 100 columns, and always use {}.  Begin a new line
after {, even for short expressions, except prefer to align conceptually
parallel constructs vertically.  Group declarations of the same type and align
on the =.

    if (foo) {
        bar++;
    }

    if (inst.x) { foo(&inst, x); }
    if (inst.y) { foo(&inst, y); }

    int const foo = fn(inst.x),
              bar = fn(inst.y),
              baz = fn(inst.z),
             quux = fn(inst.w);
    float const not_int = ...;

Use East `const`, and use it liberally, especially to distinguish locals that
name an immutable value like `double const start_time = now()` from locals that
are storage locations to hold a updating value like `double elapsed = 0;
elapsed += ...`.

But use const pointers only in the extremely unusual situations where it helps
to clarify which of several pointers will change and which will stay unchanged.
Pointer-to-const is so important that we want almost all uses of pointers and
const together to be for pointer-to-const:

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

Constructors should be named the type name, and other methods should be
prefixed with that type name.  Let constructors' return types serve as
declarations for opaque types.

    struct foo* foo(...);
    _Bool  foo_is_bar(struct foo const*);
    void   foo_quux  (struct foo*, int x);
    void   foo_free  (struct foo*);

Write positive tests and let control flow nest naturally.
Don't short-circuit loops or early-return from functions:

    // good
    if (can_foo) {
        foo(...);
        return 1;
    }
    return 0;

    // bad
    if (!can_foo) {
        return 0;
    }
    foo(...);
    return 1;

    // good
    for (int i = 0; i < n; i++) {
        if (can_foo(arr[i])) {
            foo(arr[i]);
        }
    }

    // bad
    for (int i = 0; i < n; i++) {
        if (!can_foo(arr[i])) { continue; }
        foo(arr[i]);
    }

Count bytes with `size_t` and anything else with `int`.
Name arrays in the singular and their counts in plural:
    struct inst *inst;
    int          insts;   // not inst_count, not inst_len, not inst_size

Use count() to measure the number of elements of an array:
    int const test[] = {1,2,3,4,5};
    for (int i = 0; i < count(tests); i++) {
        foo(test[i]);
        bar(test + i);
    }

If necessary include `size` in the name only when counting bytes,
and very, very rarely `count` in the name when counting other things.
    void   *buf;
    size_t  buf_size;  // not buf_count, not buf_len

Use "size" in the name of byte counts and "count" in others; alway
avoid "len".  Use count() with arrays.

Use assume(cond) for assertions in non-test code.
In tests use `here` with no parens:
    x == 3 here;    // good
    (x == 3) here;  // bad
