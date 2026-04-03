---
name: new-task
description: Create, validate, and repair the repository task setup for a new unit of work. For this repository, a new task means a compliant Git branch name in the format type/scope/ticket-short-desc plus a paired new worktree created at the same time; the worktree name must follow wt-<ticket-compact>[-n] and be 15 characters or fewer. Use when the user asks to start a new task, create a compliant branch name, validate an existing branch name, rename a branch to match policy, derive type, scope, ticket, and short-desc from task context, repair a branch-and-worktree pair, or apply repository-specific task naming rules.
---

# New Task

Generate, validate, or repair the repository task setup for this repository. In this repository, starting a new task means creating both a compliant branch and a paired worktree. Use the bundled generic reference only as a fallback for examples and validation details.

## Workflow

### 1. Build the Task Branch

Use exactly:

```text
<type>/<scope>/<ticket>-<short-desc>
```

Fill the fields in this order:
- `type`: choose the smallest accurate change category from the active whitelist
- `scope`: choose one primary module or responsibility boundary only
- `ticket`: use an authoritative task ID; if no external tracker exists, use a repository-approved internal ticket such as `REQ-001`, `BUG-014`, `TASK-087`, `SPIKE-006`, `ARCH-012`, or `SPEC-021`; do not invent placeholders unless the repository explicitly defines one
- `short-desc`: write a concise lowercase English kebab-case summary of the main change

Repository-specific companion rule:
- when creating a new branch for work, create a new temporary `worktree` at the same time
- the `worktree` name must use `wt-<ticket-compact>[-n]`
- `ticket-compact` means the branch ticket prefix lowercased and directly joined with its number, for example `BUG-311 -> bug311`, `TASK-001 -> task001`, `ARCH-203 -> arch203`
- the `worktree` name must be `<= 15` characters
- the `worktree` name may contain only lowercase letters, digits, and hyphens
- do not include branch `type`, `scope`, or `short-desc` in the `worktree` name
- add `-2`, `-3`, and so on only when the same ticket genuinely needs multiple temporary `worktree`s
- whenever generating a new task setup, also return a compliant short `worktree` name

Default generic `type` whitelist:
- `feat`
- `fix`
- `refactor`
- `docs`
- `test`
- `chore`
- `perf`
- `build`
- `ci`
- `revert`
- `spike`

Default generic `scope` examples:
- `workflow`
- `ingest`
- `geometry`
- `topology`
- `process`
- `align`
- `path`
- `motion`
- `runtime`
- `trace`
- `hmi`
- `protocol`
- `sdk`
- `sim`
- `recipe`
- `config`
- `infra`
- `build`

### 2. Validate

Reject or repair names with any of these problems:
- missing `type`, `scope`, `ticket`, or `short-desc`
- `type` not in the active whitelist
- uppercase scope or vague scope such as `misc`, `temp`, `other`
- multiple scopes jammed together
- multiple ticket IDs
- ambiguous or non-authoritative ticket values such as plain numbers without an approved prefix
- non-English `short-desc`
- spaces, underscores, or non-kebab-case wording in `short-desc`

If the user also provides or asks for a `worktree` name, reject or repair it when:
- it does not match `wt-<ticket-compact>[-n]`
- the name is longer than `15` characters
- it contains uppercase letters, underscores, spaces, or other non-kebab-case characters
- it repeats branch `type`, `scope`, or `short-desc`
- it does not map back to the same ticket as the branch

If the user asks to create or repair a new task setup without a `worktree`, treat the result as incomplete and add a compliant `worktree` name.

### 3. Return an Actionable Result

When generating a new task setup, return:
- the recommended branch name
- a short field-by-field explanation when the choice is not obvious
- a recommended `worktree` name

When validating an existing task setup, return:
- valid or invalid
- each concrete violation
- one repaired branch name if repair is possible
- one repaired `worktree` name if repair is possible

When the request is to start a new task, the actionable result must include both:
- the branch name
- the `worktree` name

If this repository rule conflicts with the bundled generic reference, follow this repository rule.

## Heuristics

- Prefer one task branch to one primary ticket.
- Prefer one task branch to one primary responsibility boundary.
- Prefer one `worktree` to one primary ticket.
- Keep `short-desc` specific. Reject weak tails such as `update`, `change-code`, `fix-issue`, `try-this`.
- Keep `scope` stable and reusable. Do not invent person-specific or temporary scopes.
- If the task spans multiple modules, choose the main ownership boundary instead of concatenating scopes.
- Keep the `worktree` short and ticket-centered. Do not mirror the full branch name.

## Output Patterns

### Generate

Use this shape:

```text
Recommended: fix/runtime/SS-205-startup-timeout
Reason: type=fix, scope=runtime, ticket=SS-205, short-desc=startup-timeout
Worktree: wt-ss205
```

### Validate

Use this shape:

```text
Result: invalid
Issues:
- missing ticket
- short-desc contains spaces
Repaired: feat/hmi/TASK-001-add-start-button
Worktree: wt-task001
```
