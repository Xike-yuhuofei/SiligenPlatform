<!--
Sync Impact Report
- Version change: 1.0.0 -> 1.1.0
- Modified principles:
  - None
- Added sections:
  - None
- Removed sections:
  - None
- Templates requiring updates:
  - ✅ updated: .specify/scripts/powershell/common.ps1
  - ✅ updated: .specify/scripts/powershell/create-new-feature.ps1
  - ✅ updated: .specify/scripts/bash/common.sh
  - ✅ updated: .specify/scripts/bash/create-new-feature.sh
  - ✅ updated: .specify/templates/plan-template.md
  - ✅ updated: .specify/templates/tasks-template.md
  - ✅ updated: .specify/scripts/powershell/update-agent-context.ps1
  - ✅ updated: .specify/scripts/bash/update-agent-context.sh
  - ⚠ pending: .specify/init-options.json (upstream schema still indicates sequential numbering)
  - ⚠ pending: .specify/templates/commands/*.md (directory not present)
- Follow-up TODOs:
  - Migrate active legacy branch `001-e2e-arch-alignment` and `specs/001-e2e-arch-alignment/` before using gated speckit workflows on new work.
-->
# SiligenSuite Constitution

## Core Principles

### I. Fact-Based Engineering Decisions
All technical conclusions MUST be based on observable evidence, reproducible behavior,
and explicit constraints. Vague language in requirements, plans, tasks, and reviews is
forbidden. Every decision record MUST state what is known, what is assumed, and what
is out of scope.
Rationale: This prevents subjective drift and keeps cross-team collaboration auditable.

### II. Canonical Workspace First
New development, documentation, and operational instructions MUST use canonical
workspace paths under repository root (`apps/`, `packages/`, `integration/`, `tools/`,
`docs/`, `config/`, `data/`, `examples/`). Historical paths and removed modules MUST
only appear in migration context and MUST NOT be treated as default runtime inputs.
Rationale: A single canonical layout reduces ambiguity and prevents regressions to
retired structures.

### III. Branch Naming Compliance (Mandatory)
Every new branch MUST match `<type>/<scope>/<ticket>-<short-desc>`. Allowed `type`
values are `feat`, `fix`, `chore`, `refactor`, `docs`, `test`, `hotfix`, `spike`, and
`release`. `ticket` MUST use a real task ID; only when unavailable may `NOISSUE` be
used temporarily.
Work on non-compliant branch names MUST stop until branch name is corrected.
Rationale: Enforced naming enables traceability, automation, and release governance.

### IV. Verifiable Build and Test Gates
Changes that affect build/runtime behavior MUST be validated through canonical root
entry points (`.\build.ps1`, `.\test.ps1`, `.\ci.ps1`) or an explicitly justified subset.
Any skipped check MUST be documented with risk and follow-up owner.
Rationale: Uniform execution paths make failures reproducible and reduce hidden
environment coupling.

### V. Legacy Isolation and Explicit Compatibility
Compatibility behavior MUST be explicit and contract-based. Implicit fallback to
legacy binaries, directories, or environment variables is prohibited unless a migration
policy explicitly allows it and the scope is time-bounded.
Rationale: Hidden fallback paths create silent behavior divergence and block cleanup.

## Engineering Constraints

1. Communication in team-facing governance and delivery artifacts MUST be precise,
   testable, and actionable.
2. Requirements and tasks MUST be written using mandatory language (`MUST`,
   `MUST NOT`, `SHOULD` with rationale) when defining quality gates.
3. Runtime and deployment guidance MUST align with canonical docs under `docs/`.

## Delivery Workflow and Quality Gates

1. Spec-driven flow is mandatory for non-trivial work: `spec.md` -> `plan.md` ->
   `tasks.md` -> implementation.
2. `Constitution Check` in implementation plans MUST pass before research/design,
   and MUST be re-validated before implementation starts.
3. Task definitions MUST preserve independent testability per user story and include
   explicit file paths.
4. Pull request review MUST include branch naming compliance, canonical path
   compliance, and evidence of build/test execution.
5. Spec-driven tooling MUST derive feature artifact paths directly from the compliant
   branch name, using `specs/<type>/<scope>/<ticket-short-desc>/`. Legacy sequential
   or timestamp-based branch creation MUST NOT be used for new work.

## Governance

This constitution overrides conflicting local conventions for planning and delivery.
Amendments require: (1) documented rationale, (2) impact assessment on templates and
runtime guidance, and (3) synchronized updates to dependent artifacts in the same
change set.

Versioning policy:
- MAJOR: Incompatible governance changes or principle removals/redefinitions.
- MINOR: New principle/section or materially expanded guidance.
- PATCH: Clarifications, wording improvements, typo fixes, and non-semantic edits.

Compliance review expectations:
- Every feature plan and task set MUST include a constitution compliance check.
- Reviewers MUST block merges that violate mandatory principles.
- Deferred compliance items MUST include owner and due date in writing.

**Version**: 1.1.0 | **Ratified**: 2026-03-23 | **Last Amended**: 2026-03-23
