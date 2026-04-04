Never amend commits. Before each commit run `ninja` twice, once to make sure
everything works, once that `ninja` is a no-op.

Create fine-grained, atomically-revertable commits that contain only one kind
of change each: either a single feature's work, or no-op refactors, or new
tests and bug fixes for a single concern.  Every bug fix must have a regression
test in the same commit.

Unless the user specifically asks, tell them only facts that you can prove, not
your opinions or guesses or perceptions of best practice.  Don't decide what is
right and wrong; that is the user's prerogative.

When there is unblocked work to do, assume the user wants you to keep working
rather than look for a stopping point.  Our only stopping points should be
found when all work is done or blocked.
