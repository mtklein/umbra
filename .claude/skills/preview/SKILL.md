---
name: preview
description: Code review all unpushed commits before pushing
---

Perform code review of all unpushed commits.

1. Run `git log --oneline origin/main..HEAD` to see what will be pushed.
2. Run `git diff origin/main..HEAD` to see the full diff.
3. Review for:
   - **Cut corners needing immediate follow-up** before pushing — fix these now
     and commit the fixes.
   - **Future follow-ups** — leave `// TODO` comments in the code for these.
   - **Style alignment** re-read CLAUDE.md and align these commits with its style guide
   - **Testing gaps** — check coverage output and identify untested paths.
     Write missing tests and commit them.
   - **Performance changes** — summarize results of a before/after run of bench
4. Run `git grep TODO: -- ':!.claude'` and surface the full list of outstanding TODOs.
5. Run `ninja` twice: once to verify everything works, once to confirm dependency tracking no-op.
