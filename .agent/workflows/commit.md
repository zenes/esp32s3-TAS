---
description: Summarize changes in progress.md, commit, and push to GitHub
---

This workflow automates the process of documenting changes before committing and pushing.

1.  **Analyze changes**: Use `git diff` or `git status` to identify all current work.
2.  **Update Progress**: Summarize these changes into `examples/eth2ap/docs/progress.md`. Use a bulleted list of meaningful changes.
    - If the file is empty, create a new structure.
    - If it has content, append the new summary with today's date.
3.  **Stage changes**: Run `git add .` to stage all modifications.
4.  **Commit**: Run `git commit -m "[Brief summary of changes]"` using a concise message derived from the summary.
// turbo
5.  **Push**: Run `git push` to upload the changes to the remote repository (`origin`).

> [!NOTE]
> This workflow ensures that the `progress.md` file always reflects the latest state of the repository.
