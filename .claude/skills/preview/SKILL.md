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
   - **Style alignment** with CLAUDE.md guidelines (East const, braces, wrapping,
     pointer placement, naming conventions, etc.) — fix any violations.
   - **Testing gaps** — check coverage output and identify untested paths.
     Write missing tests and commit them.
   - **Reproducible performance changes** — if the diff touches hot paths, note
     whether a bench comparison was done and what the results were.
4. Run `ninja` twice: once to verify everything works, once to confirm no-op.
