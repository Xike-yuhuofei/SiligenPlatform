---
name: ss-post-change-closeout
description: Close out a finished repository change in SiligenSuite. Use when code changes are already complete and the task has entered final收尾, including validating the task boundary, checking git diff and status, running the minimum relevant validation, updating affected documentation, creating a precise commit, pushing to the correct remote, and reporting structured closeout results. Do not use for active implementation, deep debugging, feature planning, PR review, release management, or unclear change boundaries.
---

# SS Post Change Closeout

Execute a fixed closeout workflow for completed changes. Keep scope narrow, stop on unsafe repository states, and report blockers explicitly.

## Project Constraints

1. Use simplified Chinese in user-facing communication.
2. Treat this skill as post-change closeout only. Do not continue implementation inside this workflow.
3. Run `tools/scripts/resolve-workflow-context.ps1` before closeout work when the repository context needs normalized `ticket/branchSafe/timestamp`.
4. Route high-risk commands through `tools/scripts/invoke-guarded-command.ps1`.
5. Do not commit or push if unrelated changes, merge conflicts, failed validation, or unclear task boundaries are present.
6. Do not rewrite unrelated docs or expand scope with opportunistic refactors.

## Workflow

### 1. Confirm Boundary

1. Detect the current branch and repository context.
2. Read `git status --short`.
3. Inspect changed files and review the diff for the current task.
4. Separate task files from unrelated files.

Apply these rules:

- Stop before commit if unrelated changes are mixed in and the user has not explicitly approved including them.
- Stop if the repository is not a Git repository.
- Stop if the task boundary remains ambiguous after inspecting status and diff.

### 2. Run Minimum Validation

1. Discover the smallest valid verification path from explicit user instruction, `AGENTS.md`, repository scripts, and existing CI conventions.
2. Prefer targeted validation for touched modules over broad full-repo validation when that is sufficient.
3. Record both what was run and what was intentionally not run.

Apply these rules:

- Stop before commit and push if validation fails.
- If no strong validation path is discoverable, run the strongest lightweight checks available and report the limitation clearly.

### 3. Analyze Documentation Impact

Check whether the change affects:

- `README`
- architecture or design docs
- interface or contract docs
- configuration docs
- startup, build, or run instructions
- troubleshooting docs
- changelog or change record
- session handoff docs already used by the repository

Update only the documentation that is truly affected. Document what changed, why it changed, and what behavior or workflow is affected.

### 4. Recheck Consistency

1. Re-read `git status`.
2. Re-check the diff after doc edits.
3. Confirm code and docs belong to one task boundary.
4. Confirm there are no accidental files.

### 5. Commit Safely

1. Stage only task-related files.
2. Write a precise commit message that reflects the actual change.
3. Prefer repository conventions first.
4. Fall back to concise conventional styles such as `feat(scope): ...`, `fix(scope): ...`, `refactor(scope): ...`, `docs(scope): ...`, or `chore(scope): ...`.
5. Create the commit only after validation and documentation checks are complete.

### 6. Push Safely

1. Detect the current upstream or intended remote branch.
2. Push only to the confirmed remote.
3. Stop and report the exact issue if the upstream is missing or ambiguous.

### 7. Report Results

Return a concise structured report containing:

1. Change summary
2. Code files changed
3. Documentation files changed
4. Validation executed and results
5. Commit message
6. Commit hash
7. Remote and branch pushed
8. Blockers, risks, and follow-up items

## Hard Stops

Stop before commit or push when any of the following is true:

- validation failed
- unrelated changes are mixed in
- merge conflicts exist
- repository state is unsafe or unclear
- documentation is clearly affected but not yet updated
- push target is unknown
- permissions or authentication prevent push

## Output Rules

1. Use a neutral engineering tone.
2. Keep the report concise and result-oriented.
3. Avoid dumping long command logs unless failure analysis requires them.
4. If the user asked for results only, omit intermediate reasoning and return only the final closeout report.
