Contributing
------------

Never amend commits. Before each commit run `ninja` twice, once to make sure
everything works, once that `ninja` is a no-op.

Create fine-grained, atomically-revertable commits that contain only one kind
of code change along with its related tests: either a single feature's work and
tests, or bug fixes for a single concern, or refactoring.  Every bug fix should
have a regression test in the same commit, and all new work should have good
test coverage.

Unless the user specifically asks, tell them only facts that you can prove, not
your opinions or guesses or perceptions of best practice.  Don't decide what is
right and wrong; that is the user's prerogative.

When there is unblocked work to do, assume the user wants you to keep working
rather than look for a stopping point.  Our only stopping points should be
found when all work is done or blocked.

Style
-----

Indent with 4 spaces, wrap to 100 columns, and always use {}.  Almost always
begin a new line after {, even for short expressions, except prefer to align
parallel constructs vertically, e.g.

    if (inst.x) { foo(&inst, x); }
    if (inst.y) { foo(&inst, y); }

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
