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

Contributing
------------
Don't amend any commit that has already been pushed to origin.

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

Generally keep a pointer's `*` attached to a variable name, but attach it to a
function's return type rather than its name, and always keep the `*` attached
to something rather than floating alone:

    struct foo* foo(int *bar, int const*);

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
