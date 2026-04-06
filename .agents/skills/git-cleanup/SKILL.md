---
name: git-cleanup
description: Safely inventory and remove worktrees and branches that are outside an explicit whitelist file stored inside this skill folder, covering local worktrees, local branches, and remote branches. Use when Codex needs to 清理白名单以外的 worktree、删除残留 branch、清退临时分支、或先做 Git 清理 dry-run 再执行本地/远端删除。
---

# Git Cleanup

Safely clean Git assets outside an explicit keep-list. Read the whitelist from this skill folder first, inventory everything second, dry-run before deletion, and delete remote branches last.

Treat repository stash entries as first-class pending work. If any stash exists, process stash classification first and do not continue to deletion until every stash entry is either preserved explicitly or routed to a closeout workflow.

## Scope

Use this skill for repository cleanup where the desired result is:

- keep only the explicitly whitelisted worktrees
- keep only the explicitly whitelisted local branches
- keep only the explicitly whitelisted remote branches
- remove stale temporary worktrees and branches outside that set

Do not use this skill for normal task closeout, active implementation, or ambiguous repository recovery. Route living work to `closeout-branch` or `closeout-worktree`.

## Whitelist Authority

Treat [`whitelist.json`](./whitelist.json) in this skill directory as the primary authority file.

If the user also gives ad hoc keep-items in the prompt, merge them as a temporary overlay but do not rewrite the authority set silently. State both:

- file-backed keep-set
- prompt overlay keep-set

Stop when `whitelist.json` is missing, malformed, or semantically incomplete.

## Safety Contract

1. Require an explicit whitelist before treating any Git asset as disposable.
2. Default to dry-run when the user does not explicitly ask to execute deletions.
3. Expand whitelist closure before classification:
   - keep the current repository root worktree
   - keep every branch checked out by a whitelisted worktree
   - keep the current branch
   - keep the remote default branch
   - keep protected branch patterns such as `main`, `master`, `develop`, `dev`, `release/*`, and `hotfix/*` unless the user explicitly overrides them
4. Treat remote deletion as highest risk and execute it only after local worktree and local branch cleanup are complete.
5. Never use `git branch -D`, `git worktree remove --force`, `git push --force`, or any equivalent force-delete path inside this skill.
6. Check GitHub PR state only as a remote-branch deletion gate. Open or unknown PR state means `blocked`, not guess-and-delete.
7. Stop instead of guessing when ownership, merge state, PR state, or whitelist authority is unclear.
8. Treat any existing stash as unresolved code change inventory. Do not delete worktrees or branches while stash ownership or intent is unclear.

## Authority File Shape

`whitelist.json` should contain a conservative keep-set like:

```json
{
  "remote": "origin",
  "base_branch": "origin/main",
  "protect_current_worktree": true,
  "protect_current_branch": true,
  "protect_default_branch": true,
  "protected_branch_patterns": [
    "main",
    "master",
    "develop",
    "dev",
    "release/*",
    "hotfix/*"
  ],
  "worktrees": [
    "D:/Projects/wt-task152"
  ],
  "local_branches": [
    "main"
  ],
  "remote_branches": [
    "origin/main"
  ]
}
```

Use exact names or exact paths for explicit keep-items. Use patterns only for protected branch classes. Do not infer the keep-set from branch naming alone.

## Safety Deletion Standard

Apply the following standard without weakening it unless the user explicitly changes policy.

### 1. Truth Standard

- The authority file is the only default keep-set.
- The current repository root worktree is always protected.
- The current branch is always protected.
- Any branch checked out by a whitelisted worktree is automatically protected.
- The remote default branch is automatically protected.

### 2. Execution Standard

- Start with `inventory + dry-run`.
- Inventory stash entries before proposing any deletion.
- Execute only after explicit user approval to delete.
- Delete strictly in this order:
  1. classify and handle stash entries
  2. non-whitelisted worktrees
  3. non-whitelisted local branches
  4. non-whitelisted remote branches

### 3. Worktree Deletion Standard

A worktree is deletable only when all conditions hold:

- it is outside the keep-set
- it is not the main worktree
- it is not the current worktree
- it is clean under `git -C <path> status --short --branch`
- it is not locked

Allowed command:

```powershell
git worktree remove <worktree-path>
```

### 4. Local Branch Deletion Standard

A local branch is deletable only when all conditions hold:

- it is outside the keep-set
- it is not the current branch
- it is not checked out by any remaining worktree
- it is fully merged into the authority base branch
- Git accepts `git branch -d` without force

Allowed command:

```powershell
git branch -d <branch>
```

### 5. Remote Branch Deletion Standard

A remote branch is deletable only when all conditions hold:

- it is outside the remote keep-set
- it is not the remote default branch
- it does not match a protected pattern
- its local counterpart is absent or already proven disposable
- PR state is `merged`, `closed`, or explicitly absent
- there is no open PR that still uses the branch

Allowed command:

```powershell
git push <remote> --delete <branch>
```

### 6. Reconciliation Standard

- Re-inventory after every destructive command.
- Do not bulk-delete multiple assets in one shell line.
- The final report must prove which candidates were removed, which remain blocked, and why.

## Workflow

### 1. Freeze the Authority Set

1. Load [`whitelist.json`](./whitelist.json).
2. Discover the current worktree, current branch, and remote default branch.
3. Read `git stash list` and freeze stash inventory before deletion planning.
4. Normalize the whitelist into one `keep-set` for:
   - worktrees
   - local branches
   - remote branches
5. Add the protected defaults from the safety contract.
6. Print the final keep-set before proposing any deletion.

Stop when:

- `whitelist.json` does not exist
- the authority file cannot be parsed
- the remote default branch cannot be determined and the user did not provide one
- the keep-set still depends on guesswork

### 2. Inventory All Git Assets

Collect evidence with read-only commands first:

```powershell
git stash list
git worktree list --porcelain
git branch --show-current
git symbolic-ref refs/remotes/origin/HEAD
git for-each-ref --format="%(refname:short)|%(upstream:short)|%(objectname:short)|%(committerdate:iso8601)" refs/heads
git for-each-ref --format="%(refname:short)|%(objectname:short)|%(committerdate:iso8601)" refs/remotes/origin
```

For each stash entry, inspect at least its summary and, when ownership is unclear, inspect its patch:

```powershell
git stash show --stat <stash>
git stash show -p <stash>
```

For each listed worktree, inspect:

```powershell
git -C <worktree-path> status --short --branch
```

For each deletion candidate branch, inspect:

```powershell
git rev-list --left-right --count <base-branch>...<branch>
git branch --contains <branch-tip>
```

When GitHub context matters for remote deletion, inspect PR state before deleting the remote branch. If PR state cannot be confirmed, classify the branch as `blocked`.

### 3. Classify Stash Before Deletion

Classify each stash entry into exactly one status:

- `preserve-explicitly`: stash is intentionally retained and must be reported as residue
- `route-to-closeout`: stash belongs to active or parked task work and must be handled by `closeout-worktree` or `closeout-branch`, not by cleanup deletion
- `blocked`: stash ownership, branch target, or intent is unclear

Apply these rules:

- If any stash exists, stop deletion execution until every stash entry has one explicit classification.
- If a stash contains task-related code changes, route it to closeout rather than ignoring it.
- If the user wants the repository fully drained, a preserved stash still counts as residue and must be called out explicitly.

### 4. Classify Every Non-Stash Item

Classify each worktree or branch into exactly one status:

- `keep`: item is explicitly whitelisted or protected by closure
- `delete-candidate`: item is outside whitelist and currently safe to delete
- `blocked`: item is outside whitelist but cannot be safely deleted yet
- `needs-confirmation`: evidence is insufficient or the user did not approve execution

Apply these rules:

- A worktree with any uncommitted or untracked work is `blocked`.
- A branch checked out by any existing worktree is `blocked` until that worktree is removed safely.
- A branch ahead of the authoritative base branch is `blocked`.
- A branch with unknown merge state is `blocked`.
- A remote branch with an open or unknown PR state is `blocked`.
- A remote branch must not be a delete-candidate until its local counterpart is either absent or already proven disposable.

### 5. Produce a Dry-Run Report

Before deletion, always produce a dry-run report containing:

1. whitelist authority and derived protected set
2. stash inventory and classification
3. inventory summary
4. delete-candidates grouped into:
   - worktrees
   - local branches
   - remote branches
5. blocked items and exact reasons
6. the exact commands that would be run
7. PR-state evidence for each remote delete-candidate

If the user asked only for analysis, stop here.

### 6. Remove Non-Whitelisted Worktrees

Delete local worktrees first, only when they are clean and outside the keep-set:

```powershell
git worktree remove <worktree-path>
```

After each removal, re-run `git worktree list --porcelain` and confirm the removed path is gone.

Stop when:

- the worktree is the current repository root
- the worktree still has changes
- the worktree is locked
- the worktree path cannot be resolved clearly

### 7. Remove Non-Whitelisted Local Branches

Delete local branches only after confirming they are not checked out anywhere and are fully merged to the authoritative base:

```powershell
git branch -d <branch>
```

Re-inventory branch refs after each deletion.

Stop when:

- the branch is current or protected
- the branch is checked out by any remaining worktree
- the branch is ahead of base
- Git requires a force delete

### 8. Remove Non-Whitelisted Remote Branches

Delete remote branches last and only after local safety checks are complete:

```powershell
git push <remote> --delete <branch>
```

Before deletion, confirm:

- the branch is outside the remote keep-set
- the branch is not the remote default branch or another protected branch
- merge state is known and acceptable
- PR state is closed, merged, or confirmed irrelevant
- the exact remote and branch names match the dry-run report

After deletion, confirm with:

```powershell
git ls-remote --heads <remote> <branch>
```

### 9. Reconcile and Report

Re-run the full inventory and report:

1. stash entries preserved, routed, or still blocked
2. items removed
3. items kept
4. items still blocked
5. proof that no non-whitelisted clean candidate remains silently
6. follow-up actions for blocked items

## Hard Stops

Stop the workflow when any of the following is true:

- the whitelist source is missing, ambiguous, or contradictory
- a stash entry exists but has not been explicitly classified and handled
- the current worktree or current branch would be deleted
- a candidate worktree contains local changes
- a candidate worktree is locked
- a candidate branch has unmerged or unpushed work
- a candidate remote branch has unknown PR or merge state
- the remote default branch cannot be identified
- a command would require force deletion
- a candidate maps to active work that should be closed out instead of deleted

## Command Discipline

Prefer small, inspectable Git commands. Do not batch destructive commands into one shell line. Execute one deletion at a time and re-inventory after every destructive step.

When a deletion command is high risk or the user asks for extra caution, route it through `scripts/validation/invoke-guarded-command.ps1` if that still preserves command clarity. If the repository needs stronger deletion guards than the existing script provides, stop and report the gap instead of improvising forceful cleanup.

## Output Rules

Return a concise structured result:

1. keep-set
2. delete plan
3. blocked items
4. commands executed or proposed
5. post-cleanup verification

If no item is safely deletable, say so explicitly and stop without deletion.
