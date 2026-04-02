Create fine-grained, atomically-revertable commits that contain only one kind
of change each: either feature work, or no-op refactors, or new tests and bug
fixes.  Every bug fix should have a regression test in the same commit.

Do not amend commits that have already been pushed to origin.
`ninja` must pass for every commit, and a second run must be a no-op.
Don't forget to add updated generated files like dumps/ to your commits.
