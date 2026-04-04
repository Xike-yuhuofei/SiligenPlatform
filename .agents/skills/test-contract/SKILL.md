---
name: test-contract
description: Validate L2 module contract tests only.
last_updated: 2026-04-04
---

# Purpose
This skill only validates L2 module-contract conformance.
This skill MUST NOT diagnose root cause or modify contract baselines.

# Scope Boundaries
- Owns only `L2`.
- Focuses on boundary behavior, protocol shape, and contract drift.
- MUST NOT replace integration or E2E validation.
- MUST NOT update snapshots, schemas, or contract fixtures automatically.

# Inputs
Required:
- changed_scope
- risk_profile: `low | medium | high | hardware-sensitive`
- goal: `gate | regression | evidence`

Optional:
- desired_depth: `auto | quick | full-offline | nightly | hil`

# Canonical Entry
MUST prefer canonical root entries:
- `.\test.ps1 -Lane quick-gate`
- `.\test.ps1 -Lane full-offline-gate`
- `python -m test_kit.workspace_validation` only after environment bootstrap sets `PYTHONPATH` to `shared\testing\test-kit\src`; prefer `.\test.ps1`, which already delegates through `scripts\validation\invoke-workspace-tests.ps1`

Final gate verdict MUST be rooted in canonical report outputs.

# Execution Rules
1. IF the change touches a shared interface, protocol payload, contract fixture, or cross-module boundary, THEN this skill is mandatory.
2. MUST run from repository root.
3. MUST prefer canonical root entries before any targeted contract-only invocation.
4. IF contract validation fails, THEN downstream `test-integration`, `test-e2e`, and `test-limited` MUST be blocked.
5. IF formal contract evidence is missing, THEN return `BLOCK`.

# Evidence Rules
MUST reference:
- `tests/reports/workspace-validation.json`
- `tests/reports/workspace-validation.md`

Supporting evidence SHOULD include:
- `case-index.json`
- `validation-evidence-bundle.json`
- `evidence-links.md`

# Blocking Policy
- `PASS`: contract validations passed and canonical evidence exists.
- `FAIL`: contract validations ran and reported boundary mismatches or protocol drift.
- `BLOCK`: formal evidence is missing or the run did not reach a valid contract conclusion.
- `FAIL` or `BLOCK` MUST block `test-integration`, `test-e2e`, and `test-limited`.

# Output Contract
## Scope
<state the exact contract scope>

## Entry
<canonical entry and parameters>

## Evidence
- `tests/reports/workspace-validation.json`
- `tests/reports/workspace-validation.md`
- <optional contract evidence bundle paths>

## Verdict
`PASS | FAIL | BLOCK`

## Blocking Impact
`none | block-lower | block-lane | block-release`

## Next Action
Only:
- `rerun: <canonical entry>`
- `escalate: test-integration | test-offline`
- `handoff: diagnose-* or implement-*`

# Examples
Positive:
- Use `test-contract` after changing shared payloads, adapters, protocol compatibility, or inter-module APIs.

Negative:
- Do NOT use `test-contract` as a substitute for full integration validation across runtime chains.

# Prohibitions
- MUST NOT modify source code.
- MUST NOT relax gates, thresholds, or baselines.
- MUST NOT bypass canonical root-level entry as the source of the final verdict.
- MUST NOT infer higher-layer success from lower-layer success.
- MUST NOT emit ambiguous verdicts such as "mostly pass", "looks OK", or "probably fine".
