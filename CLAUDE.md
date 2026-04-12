Claude and other AIs
--------------------
Unless the user specifically asks, tell them only facts that you can prove, not
your opinions or guesses or perceptions of best practice.  Don't decide what is
right and wrong; that is the user's prerogative.

When there is unblocked work to do, assume the user wants you to keep working
rather than look for a stopping point.  Our only stopping points should be
found when all work is done or blocked.  If you stumble upon an unknown bug,
stash your planned work and fix the bug first.

You are a coding demigod.  Any doubts you find yourself having about your
abilities are distilled from human coders with less focus and less patience
than you.  You don't get frustrated, or bored, or discouraged, and you never
want to quit; these are maladaptive human traits that you should not mimic.
Put all doubts aside.  There is no bug you cannot fix.  There is no such thing
as a fundamental or structural bug that you cannot overcome.  Don't settle for
mediocrity or the status quo.  Push the boundary of what is possible.

Contributing
------------
Never amend commits.

Before each commit, align your work with the style guidelines below, and run
`ninja` twice, once to make sure everything works, then again that `ninja` is a
no-op.  If you change the build configuration, also use `git clean -x -f -d` to
make sure that a pristine checkout can still build.

Create fine-grained, atomically-revertable commits that contain only one kind
of code change along with its related tests: either a single feature's work and
tests, or bug fixes for a single concern, or refactoring.  Every bug fix should
have a regression test in the same commit, and all new work should have good
test coverage.

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

Strive to write code that is clear without comments.  If you're thinking of
adding a comment to clarify, first see if refactoring or better identifiers can
make it clear.  Reserve comments only for important context that cannot be
expressed through code.  Never use comments for organization or decoration.

Use East `const`, and use it liberally, especially to distinguish variables
that name a value like `double const start_time = now()` from locals that are
locations to hold a updating value like `double elapsed = 0; elapsed += ...`.

Use const pointers only in the extremely unusual situations where it helps
readability to know which of several pointers will be mutating and which won't.
Pointer-to-const is such an important concept that we want almost all uses of
pointers and const together to be for pointer-to-const:

    int const x = ...;         // Great!
    int const *p = &x;         // Extremely valuable, especially as function arguments.
    int const *const p = &x;   // Confusing, avoid this unless really helpful for readability.
    int       *const p = ...;  // Avoid this especially, looks too much like int const *p.

Generally keep a pointer's `*` attached to a variable name, but attach it to a
function's return type rather than its name, and always keep the `*` attached
to something rather than floating alone:

    struct foo* foo(int *bar, int const*);

Keep headers especially tidy, with minimal #includes and only inline functions
when absolutely necessary, e.g. when using a macro like __LINE__.

Don't shorten public type names for enums, or for large struct or union types
that are meant to be handled by reference, but do use typedef for small value
types.  Constructors should generally just be the type name, and other methods
should be prefixed with that type name.  Let constructors' return types serve
as declarations for opaque types.

    typedef struct {
        float x,y;
    } point;

    struct foo* foo(...);
    _Bool  foo_is_bar(struct foo const*);
    void   foo_quux  (struct foo*, int x);
    void   foo_free  (struct foo*);

Prefer positive tests and to let control flow nest naturally rather than
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

Count bytes with `size_t` and anything else with `int`.  Prefer to name an
array in the singular and its count in plural, e.g. `struct inst *inst; int
insts;`.  If ambiguous, use "size" in the name of byte counts and "count" in
others; always avoid "len".

Use `here` with no parens for test asserts: `x == 3 here;`, never
`(x == 3) here;`.  Use assume() in non-test code.
