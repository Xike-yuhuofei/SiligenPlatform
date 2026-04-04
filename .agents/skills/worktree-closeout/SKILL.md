---
name: worktree-closeout
description: Close out an entire SiligenSuite worktree by scanning all uncommitted changes and unpushed commits, grouping them into task batches, validating each batch, updating affected docs, committing and pushing batch by batch, opening or updating PRs, auto-handling trivial conflicts, switching PRs to ready for review, and reporting remaining residue. Use when the worktree may contain multiple tasks or leftover work and the goal is to drain the worktree systematically rather than close only the current branch.
---

# Worktree Closeout

Execute a fixed worktree-level closeout workflow for all pending work in the current worktree. Inventory first, classify before acting, and do not leave silent residue.

## Project Constraints

1. Use simplified Chinese in user-facing communication.
2. Treat this skill as worktree supervision and cleanup, not as active implementation.
3. Take responsibility for the entire current worktree, including uncommitted changes, untracked files that are plausibly task-related, stash entries, and unpushed commits on relevant branches.
4. Treat `git stash list` as first-class pending work. A stash is not backup noise; it is undisposed code change inventory that must be classified and handled explicitly.
5. Process stash entries before any closeout action that would otherwise claim the worktree is drained.
6. Classify before commit. Do not mix multiple task groups into one batch.
7. Reuse `branch-closeout` rules for each batch after grouping is complete.
8. Run `scripts/validation/resolve-workflow-context.ps1` before closeout work when the repository context needs normalized `ticket/branchSafe/timestamp`.
9. Route high-risk commands through `scripts/validation/invoke-guarded-command.ps1`.
10. Auto-handle conflicts only when the resolution is mechanical, low-risk, and still inside the batch boundary. Otherwise stop that batch and report.

## Workflow

### 1. Inventory the Entire Worktree

1. Detect the repository root, current branch, linked worktree state, and upstream context.
2. Read `git status --short`.
3. Read `git stash list` and inspect each stash entry that belongs to the repository lifecycle.
4. Inspect staged, unstaged, untracked, stashed, and unpushed work that belongs to the current worktree lifecycle.
5. Inspect local branches in the worktree scope and identify unpushed commit ranges.
6. Build one inventory covering every pending item that could block a clean worktree.

Apply these rules:

- Stop if the repository is not a Git repository.
- Stop if the current worktree state is unsafe or cannot be enumerated clearly.
- Do not proceed to batch closeout while any stash entry is still uninspected.

### 2. Classify and Batch

1. Handle stash inventory before all other batch actions:
   - inspect each stash entry summary and scope
   - decide whether to `apply/pop` into an existing batch, preserve it as an explicitly parked batch, or report it as blocked
   - if needed, inspect stash contents with `git stash show -p <stash>` before classification
2. Group inventory items by ticket, branch, module ownership, diff intent, and commit history.
3. Create explicit batches such as `ready to close`, `needs clarification`, `blocked by conflict`, or `park intentionally`.
4. Confirm whether each batch maps to one branch or requires branch creation/splitting.
5. Leave no item unclassified.

Apply these rules:

- Stop before commit or push if any pending item cannot be classified with reasonable confidence.
- Stop if a proposed batch still mixes unrelated task boundaries.
- Stop if any stash entry remains neither consumed into a batch nor explicitly reported as parked or blocked.

### 3. Close Each Ready Batch

For each `ready to close` batch:

1. Freeze the batch boundary.
2. Run the smallest valid verification path for that batch.
3. Update only the documentation truly affected by that batch.
4. Stage and commit only that batch.
5. Sync, auto-handle trivial conflicts, and re-run the minimum necessary validation after conflict resolution.
6. Push the batch branch safely.
7. Open or update the corresponding PR.
8. Move the PR to `ready for review` when the batch is complete.

Apply these rules:

- If a batch fails validation, stop that batch and report it as blocked.
- If a batch hits non-trivial conflicts, stop that batch and report it as blocked.
- Continue with other independent batches only when doing so does not endanger repository clarity.

### 4. Reconcile Residue

1. Re-scan the worktree after each batch closeout.
2. Re-scan `git stash list` after each batch closeout and at the end of the workflow.
3. Confirm which items were cleared, which remain blocked, and which were intentionally parked.
4. Require explicit reporting for every residual item.

### 5. Report Results

Return a concise structured report containing:

1. Worktree summary
2. Inventory and batch classification
3. Stash handling summary, including consumed, parked, and blocked stash entries
4. Closed batches and their commit hashes
5. Validation executed for each batch
6. Conflict handling summary for each batch
7. Remote branches pushed
8. PR links and PR states
9. Remaining residue, blocked batches, and follow-up items

## Hard Stops

Stop the affected batch or the whole workflow when any of the following is true:

- validation failed for a batch
- a pending item cannot be classified
- a stash entry exists but has not been explicitly handled
- a batch mixes unrelated tasks
- conflicts are non-trivial or unresolved
- repository or worktree state is unsafe or unclear
- documentation is clearly affected but not yet updated
- push target or PR target is unknown
- permissions or authentication prevent push or PR operations

Do not claim worktree closeout success while unreported residue still exists.

## Output Rules

1. Use a neutral engineering tone.
2. Keep the report concise and result-oriented.
3. Avoid dumping long command logs unless failure analysis requires them.
4. If the user asked for results only, omit intermediate reasoning and return only the final worktree closeout report.
