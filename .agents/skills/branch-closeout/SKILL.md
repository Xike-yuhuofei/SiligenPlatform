---
name: branch-closeout
description: Close out one finished task branch in SiligenSuite, including validating the branch boundary, organizing task-related uncommitted changes, checking unpushed commits, updating affected docs, committing precisely, pushing safely, opening or updating the PR, auto-handling trivial conflicts, switching the PR to ready for review, and reporting structured results. Use when the current branch already represents one clear task and has entered final收尾. Do not use for multi-task worktree cleanup, active implementation, deep debugging, or unclear task boundaries.
---

# Branch Closeout

Execute a fixed branch-level closeout workflow for one finished task branch. Keep the boundary singular, stop on unsafe repository states, and leave no ambiguous branch residue.

## Project Constraints

1. Use simplified Chinese in user-facing communication.
2. Treat this skill as branch closeout only. Do not continue implementation inside this workflow.
3. Require one clear task boundary on the current branch. If the branch contains multiple task groups, stop and route to `worktree-closeout`.
4. Run `scripts/validation/resolve-workflow-context.ps1` before closeout work when the repository context needs normalized `ticket/branchSafe/timestamp`.
5. Route high-risk commands through `scripts/validation/invoke-guarded-command.ps1`.
6. Do not rewrite unrelated docs or expand scope with opportunistic refactors.
7. Auto-handle conflicts only when the resolution is mechanical, low-risk, and still inside the frozen task boundary. Otherwise stop and report.

## Workflow

### 1. Confirm Branch Boundary

1. Detect the current branch, upstream, and repository context.
2. Read `git status --short`.
3. Inspect staged, unstaged, untracked, and unpushed branch changes.
4. Review the diff and commit range for the current branch.
5. Confirm all pending and pushed-but-not-merged work on the branch belongs to one task.

Apply these rules:

- Stop if the repository is not a Git repository.
- Stop if unrelated changes are mixed in.
- Stop if the branch boundary remains ambiguous after inspecting status, diff, and unpushed commits.

### 2. Run Minimum Validation

1. Discover the smallest valid verification path from explicit user instruction, `AGENTS.md`, repository scripts, and existing CI conventions.
2. Prefer targeted validation for touched modules over broad full-repo validation when that is sufficient.
3. Record both what was run and what was intentionally not run.

Apply these rules:

- Stop before commit, push, or PR update if validation fails.
- If no strong validation path is discoverable, run the strongest lightweight checks available and report the limitation clearly.

### 3. Analyze Documentation Impact

Check whether the branch changes affect:

- `README`
- architecture or design docs
- interface or contract docs
- configuration docs
- startup, build, or run instructions
- troubleshooting docs
- changelog or change record
- session handoff docs already used by the repository

Update only the documentation that is truly affected. Document what changed, why it changed, and what behavior or workflow is affected.

### 4. Commit Safely

1. Stage only branch-related files.
2. Write a precise commit message that reflects the actual change.
3. Prefer repository conventions first.
4. Fall back to concise conventional styles such as `feat(scope): ...`, `fix(scope): ...`, `refactor(scope): ...`, `docs(scope): ...`, or `chore(scope): ...`.
5. Create the commit only after validation and documentation checks are complete.

### 5. Sync and Auto-Handle Conflicts

1. Detect the intended base branch and current upstream.
2. Sync the branch using the repository-preferred path.
3. Auto-handle trivial conflicts such as lockfiles, generated files, or obvious non-overlapping text merges when the correct resolution is deterministic.
4. Re-run the minimum necessary validation after any conflict resolution.

Apply these rules:

- Stop if conflicts are semantic, ownership-sensitive, or require product judgment.
- Stop if the push target or PR base is missing or ambiguous.

### 6. Push Safely

1. Push only to the confirmed remote branch.
2. Confirm the remote branch now contains all intended commits.

### 7. Open or Update PR

1. Detect whether a PR already exists for the current branch.
2. Create the PR when missing, using the task boundary, validation evidence, and doc impact.
3. Update the existing PR title or body when stale.
4. Move the PR to `ready for review` after branch validation and conflict checks are complete.

### 8. Report Results

Return a concise structured report containing:

1. Change summary
2. Code files changed
3. Documentation files changed
4. Validation executed and results
5. Conflict handling summary
6. Commit message
7. Commit hash
8. Remote and branch pushed
9. PR link and current PR state
10. Blockers, risks, and follow-up items

## Hard Stops

Stop before commit, push, or PR ready-for-review when any of the following is true:

- validation failed
- unrelated changes are mixed in
- the branch boundary is unclear
- conflicts are non-trivial or unresolved
- repository state is unsafe or unclear
- documentation is clearly affected but not yet updated
- push target is unknown
- PR target is unknown
- permissions or authentication prevent push or PR operations

## Output Rules

1. Use a neutral engineering tone.
2. Keep the report concise and result-oriented.
3. Avoid dumping long command logs unless failure analysis requires them.
4. If the user asked for results only, omit intermediate reasoning and return only the final branch closeout report.
