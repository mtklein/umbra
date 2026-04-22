---
name: tidy
description: Apply /review to the whole codebase, not just unpushed commits
---

Apply the same scrutiny as `/review` but to the entire codebase rather than just
unpublished commits.

Be especially critical of code style.  For any bit of code you're considering,
ask: would I write it this way today?  If not, rewrite it.

Look for things that are weird even though they don't technically violate code
style, like:
- Expressions or statements spanning multiple lines that would be clearer on one line
- Strange spacing or alignemnt
- Unclear or inconsistent identifier names
- Outdated conventions that have drifted from current CLAUDE.md guidelines
- Unnecessary comments, especially organizational or decorative ones
- Inconsistent formatting between similar constructs

Make fixes in fine-grained, atomically-revertable commits.  Run `ninja` twice
after each commit: once to verify everything works, once to confirm no-op.
