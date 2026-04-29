The user has the last word in design and policy.

Never write comments except to use // TODO to track bugs and follow-ups.

Indent with 4 spaces, wrap to 100 columns, and always use {}.

Begin a new line after {, even for short expressions, but prefer to align
conceptually parallel constructs vertically:

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

Use East `const`, and liberally, especially to distinguish locals that name an
immutable value from locals that act as storage locations for updating values:

    double const start_time = now();
    double elapsed = 0;  // we can tell `elapsed` will change at a glance
    ...
    elapsed += start_time - ...;

But don't use const pointers.  Pointer-to-const is so important that we want
almost all uses of pointers and const together to be for pointer-to-const:

    int const x = ...;         // Great!
    int const *p = &x;         // Extremely valuable, especially as function arguments.
    int       *const p = ...;  // Avoid, looks too much like `int const *p`.
    int const *const p = &x;   // Should be rare, mostly for globals.

Pointer `*` should be snug with variable and parameter names, or with the type
of unnamed parameters, or with a function's return type:

    int *foo;
    char const* bar(int *x, int*);

Keep headers tidy, with minimal #includes and inline functions only when
technically unavoidable, e.g. embedding a macro like `__LINE__`.

Public APIs belong in include/ and get an `umbra_` prefix; nothing else should
have an `umbra_` prefix.  Don't typedef public type names for enums, or for
large struct or union types that are meant to be handled by reference, but do
use typedef for small struct or union value types:

    enum umbra_foo {
        ...
    };
    struct umbra_reference_type {
        void *ptr;
        int whatever;
        ...
    };
    typedef union {
        int   i;
        float f;
    } umbra_value_type;

A type's constructor should have the same name as the type, and its other
methods should be prefixed with that type name.  Let the constructor's return
type serve as the declaration for opaque types.  Opaque types should always
have a free method that completely cleans up the object and permits NULL.

    struct foo* foo(...);
    void   foo_free  (struct foo*);  // NULL must be safe
    _Bool  foo_is_bar(struct foo const*);
    void   foo_quux  (struct foo*, int x);

Favor positive condition tests and let control flow nest naturally:

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

Name arrays in the singular and their counts in plural,
and use count() to measure the number of elements in an array:

    struct inst *inst;
    int          insts;   // not n_inst, not n_insts, not inst_count, not inst_len, not inst_size

    int const test[] = {1,2,3,4,5};
    for (int i = 0; i < count(tests); i++) {
        foo(test[i]);
        bar(test+i);
    }

Never use `len`; it's too ambiguous whether it refers to bytes or elements or Euclidean distance.

Use assume(cond) for assumptions, preconditions, or assertions in non-test code.
In tests make assertions with `here` using no parens:
    x == 3 here;    // great
    (x == 3) here;  // no, () not needed and sometimes can hide logic bugs
