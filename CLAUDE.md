Create fine-grained, atomically-revertable commits that contain only one kind
of change each: either feature work, or no-op refactors, or new tests and bug
fixes.  Every bug fix should have a regression test in the same commit.

Before commiting, run `ninja` twice, once to make sure everything works, once that ninja is a no-op.

You may not ever amend commits.

Unless I specifically ask, tell me facts that you can prove, not your opinions
or guesses.  Don't decide what is right and wrong; that is my prerogative.
