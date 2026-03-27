# Git Branch Naming

## 1. Purpose

This document defines the mandatory Git branch naming convention for the project.

Goals:

- ensure branch names are globally consistent
- make branch purpose readable at a glance
- bind branches to stable task identifiers
- reduce ambiguity in review, merge, rollback, and traceability

---

## 2. Required Format

All branches MUST follow this format:

```text
<type>/<scope>/<ticket>-<short-desc>
```

Example:

```text
feat/motion/MC-142-add-soft-limit
fix/hmi/BUG-311-fix-startup-freeze
refactor/runtime/ARCH-057-extract-session-snapshot
docs/protocol/SPEC-021-update-frame-contract
```

---

## 3. Field Rules

## 3.1 `type`

`type` indicates the primary change category.

Allowed values:

* `feat`      : new feature
* `fix`       : bug fix
* `refactor`  : code restructuring without intended behavior change
* `docs`      : documentation only
* `test`      : tests only
* `chore`     : maintenance, tooling, scripts, dependency housekeeping
* `perf`      : performance optimization
* `build`     : build system, compiler, packaging
* `ci`        : CI/CD pipeline changes
* `revert`    : revert existing changes
* `spike`     : exploratory or validation work

Rules:

* MUST be lowercase
* MUST be one of the allowed values
* MUST NOT use synonyms such as `feature`, `bugfix`, `optimization`

---

## 3.2 `scope`

`scope` indicates the main module, subsystem, or responsibility boundary.

Recommended project scopes:

* `workflow`
* `ingest`
* `geometry`
* `topology`
* `process`
* `align`
* `path`
* `motion`
* `runtime`
* `trace`
* `hmi`
* `protocol`
* `sdk`
* `sim`
* `recipe`
* `config`
* `infra`
* `build`

Rules:

* MUST be lowercase
* SHOULD be a stable module/domain name
* SHOULD use a single word where possible
* MAY use kebab-case if needed
* MUST NOT use vague scopes such as:

  * `misc`
  * `temp`
  * `other`
  * `update`
  * `project`

Multi-module changes:

* branch name MUST choose one primary scope only
* the chosen scope SHOULD represent the main responsibility boundary of the change
* MUST NOT concatenate multiple scopes into one branch name

Invalid:

```text
feat/hmi-runtime-motion/MC-142-add-feature
```

Valid:

```text
feat/runtime/MC-142-add-online-ready-gate
```

---

## 3.3 `ticket`

`ticket` is the stable task identifier.

Priority order:

### A. Preferred: external task system ID

Use the unique ID from Jira / GitHub Issues / TAPD / Zentao / internal issue tracker.

Examples:

* `MC-142`
* `BUG-311`
* `ARCH-057`
* `SPEC-021`

### B. Fallback: internal project ticket ID

If no external task system exists, use one of these prefixes:

* `REQ-xxx`   : requirement
* `BUG-xxx`   : defect
* `TASK-xxx`  : implementation task
* `SPIKE-xxx` : research / validation
* `ARCH-xxx`  : architecture task
* `SPEC-xxx`  : specification / contract doc

Examples:

* `REQ-001`
* `BUG-014`
* `TASK-087`
* `SPIKE-006`
* `ARCH-012`
* `SPEC-021`

Rules:

* MUST be unique
* MUST be stable once assigned
* MUST represent exactly one primary task
* MUST NOT contain multiple ticket IDs in one branch name

Invalid:

```text
feat/motion/MC-142-BUG-311-fix-and-add
```

### Internal numbering rule

If internal tickets are used:

* numbering MUST be centrally managed
* numbering SHOULD use at least 3 digits
* recommended format:

  * `001` to `999`
  * expand to 4 digits when needed

Examples:

* `TASK-001`
* `TASK-142`
* `BUG-077`

---

## 3.4 `short-desc`

`short-desc` is the concise English summary of the branch purpose.

Rules:

* MUST be lowercase
* MUST use kebab-case
* MUST be English only
* MUST describe the main change only
* SHOULD be 2 to 6 words
* SHOULD stay under 40 characters where practical
* MUST contain only:

  * `a-z`
  * `0-9`
  * `-`

Valid examples:

* `add-soft-limit`
* `fix-startup-freeze`
* `split-frame-parser`
* `update-frame-contract`
* `support-arc-fitting`

Invalid examples:

* `do-some-fixes`
* `update-code`
* `new-feature`
* `try-this`
* `临时修改`
* `fix startup freeze`
* `fix_startup_freeze`

---

## 4. Full Examples

### Feature

```text
feat/motion/MC-142-add-soft-limit
feat/path/REQ-021-support-arc-fitting
feat/runtime/TASK-201-add-online-ready-gate
```

### Fix

```text
fix/hmi/BUG-311-fix-startup-freeze
fix/protocol/BUG-198-fix-frame-length-check
fix/runtime/BUG-402-fix-stop-resume-deadlock
```

### Refactor

```text
refactor/planner/ARCH-057-split-path-service
refactor/sdk/TASK-087-remove-shared-state
refactor/runtime/ARCH-088-extract-session-snapshot
```

### Docs

```text
docs/protocol/SPEC-021-update-frame-contract
docs/runtime/SPEC-034-clarify-state-machine
docs/architecture/ARCH-102-add-boundary-rules
```

### Spike

```text
spike/protocol/SPIKE-006-evaluate-binary-framing
spike/motion/SPIKE-009-test-scurve-profile
```

### Build / CI / Chore

```text
build/infra/TASK-122-upgrade-clang-cl
ci/build/TASK-133-add-static-analysis
chore/config/TASK-144-cleanup-dev-settings
```

---

## 5. Invalid Patterns

The following patterns are forbidden.

### Missing ticket

```text
feat/motion/add-soft-limit
fix/hmi/startup-freeze
```

### Weak or ambiguous ticket

```text
feat/motion/001-add-soft-limit
fix/hmi/002-fix-ui
```

### Ambiguous description

```text
fix/runtime/BUG-311-fix-issue
feat/path/REQ-021-update
refactor/sdk/TASK-087-change-code
```

### Non-English or spaces

```text
feat/motion/MC-142-增加软限位
fix/hmi/BUG-311-fix startup freeze
```

### Temporary or personal naming

```text
fix/xiaochuan/BUG-311-my-fix
feat/temp/TASK-021-try-new-way
```

---

## 6. Relation to Commits and PRs

Branch naming works best when commit titles and PR titles also reference the same ticket.

### Commit title recommendation

```text
MC-142: add software limit config model
MC-142: wire limit check into planner
BUG-311: fix startup freeze on offline mode
```

### PR title recommendation

```text
[MC-142] Add software limit support in motion planner
[BUG-311] Fix HMI startup freeze in offline mode
```

Rules:

* one branch SHOULD map to one primary ticket
* one PR SHOULD map to one primary ticket
* multiple commits MAY exist under the same branch and ticket

---

## 7. Validation Rule

Recommended regex:

```regex
^(feat|fix|refactor|docs|test|chore|perf|build|ci|revert|spike)\/[a-z0-9-]+\/[A-Z]+-\d{3,4}-[a-z0-9-]+$
```

Notes:

* this regex assumes ticket format like `MC-142` or `TASK-0087`
* if your project uses 1 to 4 digits, adjust `\d{3,4}` accordingly
* if scope whitelist is enforced, replace `[a-z0-9-]+` with the explicit scope list

Example with explicit scope whitelist:

```regex
^(feat|fix|refactor|docs|test|chore|perf|build|ci|revert|spike)\/(workflow|ingest|geometry|topology|process|align|path|motion|runtime|trace|hmi|protocol|sdk|sim|recipe|config|infra|build)\/[A-Z]+-\d{3,4}-[a-z0-9-]+$
```

---

## 8. Recommended Enforcement

The project SHOULD enforce this rule through one or more of the following:

* local Git hook
* server-side hook
* CI branch name validation
* PR template and reviewer checklist

Minimum recommendation:

* reject invalid branch names in CI
* reject PRs whose source branch violates this document

---

## 9. Minimal Operating Policy

The team MUST follow these operational rules:

1. create or bind a ticket before creating a branch
2. branch name MUST be finalized at creation time
3. do not reuse one branch for multiple unrelated tickets
4. do not rename a branch casually after review has started
5. if scope changes significantly, create a new branch and new ticket if necessary

---

## 10. Final Rule Summary

Mandatory format:

```text
<type>/<scope>/<ticket>-<short-desc>
```

Mandatory constraints:

* `type` MUST be from the approved whitelist
* `scope` MUST be a stable lowercase module/domain name
* `ticket` MUST be the unique task identifier
* `short-desc` MUST be lowercase English kebab-case
* branch names MUST NOT contain spaces, underscores, Chinese characters, or multiple ticket IDs

Approved example set:

```text
feat/motion/MC-142-add-soft-limit
fix/hmi/BUG-311-fix-startup-freeze
refactor/runtime/ARCH-057-extract-session-snapshot
docs/protocol/SPEC-021-update-frame-contract
```
