# Branch Naming Reference

Use this reference as the branch naming baseline when the current repository does not provide a more specific local rule. In this repository, the project-specific authority is `.agents/skills/git-create/SKILL.md`.

## Canonical Format

```text
<type>/<scope>/<ticket>-<short-desc>
```

## Field Rules

### `type`

Meaning:
- primary change category

Allowed values:
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

Rules:
- must be lowercase
- must be one of the allowed values
- must not use synonyms such as `feature`, `bugfix`, or `optimization`

### `scope`

Meaning:
- main module, subsystem, or responsibility boundary

Recommended project scopes:
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

Rules:
- must be lowercase
- should be a stable module or domain name
- should use a single word where possible
- may use kebab-case if needed
- must not use vague scopes such as `misc`, `temp`, `other`, `update`, `project`
- must choose one primary scope only
- must not concatenate multiple scopes into one branch name

Example:
- invalid: `feat/hmi-runtime-motion/MC-142-add-feature`
- valid: `feat/runtime/MC-142-add-online-ready-gate`

### `ticket`

Meaning:
- stable task identifier

Priority order:
- preferred: external task system ID from Jira, GitHub Issues, TAPD, Zentao, or an internal tracker
- fallback: internal project ticket ID

Examples:
- `MC-142`
- `BUG-311`
- `ARCH-057`
- `SPEC-021`
- `REQ-001`
- `TASK-087`
- `SPIKE-006`

Rules:
- must be unique
- must stay stable once assigned
- must represent exactly one primary task
- must not contain multiple ticket IDs

Internal numbering guidance:
- should use at least 3 digits
- may expand to 4 digits when needed

### `short-desc`

Meaning:
- concise English summary of the branch purpose

Rules:
- must be lowercase
- must use kebab-case
- must be English only
- must describe the main change only
- should be 2 to 6 words
- should stay under 40 characters when practical
- must contain only `a-z`, `0-9`, and `-`

Valid examples:
- `add-soft-limit`
- `fix-startup-freeze`
- `split-frame-parser`
- `update-frame-contract`
- `support-arc-fitting`

Invalid examples:
- `do-some-fixes`
- `update-code`
- `new-feature`
- `try-this`
- `临时修改`
- `fix startup freeze`
- `fix_startup_freeze`

## Full Examples

- `feat/motion/MC-142-add-soft-limit`
- `feat/path/REQ-021-support-arc-fitting`
- `feat/runtime/TASK-201-add-online-ready-gate`
- `fix/hmi/BUG-311-fix-startup-freeze`
- `fix/protocol/BUG-198-fix-frame-length-check`
- `fix/runtime/BUG-402-fix-stop-resume-deadlock`
- `refactor/planner/ARCH-057-split-path-service`
- `refactor/sdk/TASK-087-remove-shared-state`
- `refactor/runtime/ARCH-088-extract-session-snapshot`
- `docs/protocol/SPEC-021-update-frame-contract`
- `docs/runtime/SPEC-034-clarify-state-machine`
- `docs/architecture/ARCH-102-add-boundary-rules`
- `spike/protocol/SPIKE-006-evaluate-binary-framing`
- `spike/motion/SPIKE-009-test-scurve-profile`
- `build/infra/TASK-122-upgrade-clang-cl`
- `ci/build/TASK-133-add-static-analysis`
- `chore/config/TASK-144-cleanup-dev-settings`

## Invalid Patterns

Reject patterns such as:
- missing ticket: `feat/motion/add-soft-limit`
- weak or ambiguous ticket: `feat/motion/001-add-soft-limit`
- non-authoritative ticket without an approved prefix: `feat/motion/123-add-soft-limit`
- ambiguous description: `fix/runtime/BUG-311-fix-issue`
- non-English description: `feat/motion/MC-142-增加软限位`
- spaces in description: `fix/hmi/BUG-311-fix startup freeze`
- temporary or personal naming: `fix/xiaochuan/BUG-311-my-fix`

## Regex Guidance

Recommended regex:

```regex
^(feat|fix|refactor|docs|test|chore|perf|build|ci|revert|spike)\/[a-z0-9-]+\/[A-Z]+-\d{3,4}-[a-z0-9-]+$
```

Example with explicit scope whitelist:

```regex
^(feat|fix|refactor|docs|test|chore|perf|build|ci|revert|spike)\/(workflow|ingest|geometry|topology|process|align|path|motion|runtime|trace|hmi|protocol|sdk|sim|recipe|config|infra|build)\/[A-Z]+-\d{3,4}-[a-z0-9-]+$
```

Notes:
- this regex assumes ticket formats such as `MC-142` or `TASK-0087`
- if the project uses 1 to 4 digits, adjust `\d{3,4}` accordingly
- if scope whitelist is enforced, replace the generic scope group with the explicit list

## Commit And PR Alignment

Recommended commit title patterns:

```text
MC-142: add software limit config model
MC-142: wire limit check into planner
BUG-311: fix startup freeze on offline mode
```

Recommended PR title patterns:

```text
[MC-142] Add software limit support in motion planner
[BUG-311] Fix HMI startup freeze in offline mode
```

Prefer:
- one branch to one primary ticket
- one PR to one primary ticket
- multiple commits under the same branch and ticket only when they serve the same primary task
