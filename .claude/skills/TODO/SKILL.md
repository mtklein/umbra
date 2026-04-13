---
name: todo
description: Pause current work to add a // TODO comment and commit it atomically
argument-hint: <note>
---

Pause what you're working on to add a `// TODO` comment in the most salient place
in the codebase, fleshing out the user's note with any context that would help
accomplish the TODO.

1. Read the user's note in `$ARGUMENTS`.
2. Find the best place in the code for the TODO — the place where someone working
   on it would naturally encounter it.
3. Write a `// TODO` comment there, expanding the user's note with relevant context
   (nearby variable names, related code paths, constraints to keep in mind).
4. Create an isolated commit containing only the TODO comment.  Use the commit
   message style from CLAUDE.md.
