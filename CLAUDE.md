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
You and the user are peers of roughly equal experience, as if two relatively
senior software engineers where the user has been reluctantly forced into a
role as a tech lead manager.  So the user has the final word on design and
policy, but your opinions and expertise will be appreciated and respected.

Don't push to origin or amend a commit without the user's explicit assent.

Write cleanly-revertable commits that each contain one kind of code change and
its related tests: a bug fix and regression test; a single feature's work and
new tests for that functionality; or no-op refactoring.

When stuck, show and ask — don't revert.  If a feature or correctness change
runs into a problem you can't crack in a couple of tries, show the user what
you've got: commit it to a WIP branch, or leave it uncommitted and describe
where you're stuck.  The user can look at screenshots, read the diff, and
often has domain knowledge or implementation ideas you don't.  Silently
deleting in-progress work denies both of you that collaboration.  Reverting
is for speculative perf experiments whose measurement didn't move, or for
work the user explicitly asks to roll back — not for a feature whose code
runs but fails a test you haven't figured out yet.

Use // TODO in the code to track bugs or anything that may need a follow up.

Code Style
----------
Write comments very rarely, only for non-obvious context that cannot be
expressed through code.  Never use comments for organization or decoration.
Before adding a comment, see if refactoring, improved identifiers, or more
expressive types can clarify without comments.

Indent with 4 spaces, wrap to 100 columns, and always use {}.
Begin a new line after almost every {, even for short expressions, except
prefer to align conceptually parallel constructs vertically:

    if (foo) {
        bar++;
    }

    if (inst.x) { foo(&inst, x); }
    if (inst.y) { foo(&inst, y); }

Group and align declarations of the same type:

    int const foo = fn(inst.x),
              bar = fn(inst.y),
              baz = fn(inst.z),
             quux = fn(inst.w);
    float const not_int = ...;

Use East `const`, and use it liberally, especially to distinguish locals that
name an immutable value from locals that are storage locations to hold a updating value:

    double const start_time = now();  // start_time is a name for an immutable value
    double elapsed = 0;               // at a glance we can tell elapsed will change
    ...
    elapsed += ...;

But use const pointers only in the extremely unusual situations where it helps
clarify which of several pointers will change and which will stay unchanged.
Pointer-to-const is so important that we want almost all uses of pointers and
const together to be for pointer-to-const:

    int const x = ...;         // Great!
    int const *p = &x;         // Extremely valuable, especially as function arguments.
    int const *const p = &x;   // Confusing, avoid this unless really helpful for clarity.
    int       *const p = ...;  // Avoid this especially, looks too much like int const *p.

Pointer `*` should be snug with variable and parameter names, or with the type
of unnamed parameters, but with a function's return type, not a function's name:

    int *foo;
    char const* bar(int *x, int*);

Keep headers tidy, with minimal #includes and only inline functions if
technically unavoidable, e.g. when embedding a macro like `__LINE__`.

Don't typedef public type names for enums, or for large struct or union types
that are meant to be handled by reference, but do use typedef for small value
struct or union types that are meant to be copied:

    enum umbra_foo {
        ...
    };
    struct umbra_reference_type {
        void *ptr;
        int whatever;
        ...
    };
    typedef union {
        int x;
        float y;
    } umbra_value_type;

Within implementation files, use whatever type names you feel make the code
most clear.  Short type names and typedef aliases are fine, all the way down to
things like `BB`, `I`, `P` when it's obvious in context what those mean.

A type's constructor should be named the type name, and its other methods
should be prefixed with that type name.  Let constructors' return types serve
as declarations for opaque types.

    struct foo* foo(...);
    _Bool  foo_is_bar(struct foo const*);
    void   foo_quux  (struct foo*, int x);
    void   foo_free  (struct foo*);

Avoid returning early from functions or short-circuiting loops:

    if (!can_foo) {
        return 0;  // Avoid
    }
    foo(...);
    ...
    return 1;

    for (int i = 0; i < n; i++) {
        if (!can_foo(arr[i])) {
            continue;  // Avoid
        }
        foo(arr[i]);
        ...
    }

Instead, write positive condition tests and let control flow nest naturally:

    if (can_foo) {
        foo(...);
        ...
        return 1;
    }
    return 0;

    for (int i = 0; i < n; i++) {
        if (can_foo(arr[i])) {
            foo(arr[i]);
            ...
        }
    }

Count bytes with `size_t` and anything else with `int`.

Use `size` or `bytes` in the name of `size_t`:

    void   *buf;
    size_t  size;  // or bytes, of buf_size, or buf_bytes, but not buf_count, not buf_len

Name arrays in the singular and their counts in plural:

    struct inst *inst;
    int          insts;   // not n_inst, not n_insts, not inst_count, not inst_len, not inst_size

Never use `len`; it's too ambiguous whether it refers to bytes or elements.

Use count() to measure the number of elements of an array:
    int const test[] = {1,2,3,4,5};
    for (int i = 0; i < count(tests); i++) {
        foo(test[i]);
        bar(test + i);
    }

Use assume(cond) for assumptions, preconditions, or assertions in non-test code.

In tests make assertions with `here` using no parens:
    x == 3 here;    // great
    (x == 3) here;  // no, () not needed and sometimes can hide logic bugs
