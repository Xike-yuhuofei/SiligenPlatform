---
name: test-integration
description: Validate L3 integration tests only.
last_updated: 2026-04-04
---

# Purpose
This skill only validates L3 integration conformance.
This skill MUST NOT claim E2E, HIL, or performance success.

# Scope Boundaries
- Owns only `L3`.
- Focuses on cross-module and cross-boundary composition.
- MUST NOT replace contract validation.
- MUST NOT infer physical closed-loop correctness.

# Inputs
Required:
- changed_scope
- risk_profile: `low | medium | high | hardware-sensitive`
- goal: `gate | regression | evidence`

Optional:
- desired_depth: `auto | quick | full-offline | nightly | hil`

# Canonical Entry
MUST use canonical root entries:
- `.\test.ps1 -Lane full-offline-gate`
- `python -m test_kit.workspace_validation` only after environment bootstrap sets `PYTHONPATH` to `shared\testing\test-kit\src`; prefer `.\test.ps1`, which already delegates through `scripts\validation\invoke-workspace-tests.ps1`

Final verdict MUST be based on canonical reports and formal evidence bundles.

# Execution Rules
1. IF contract validation has not passed, THEN return `BLOCK`.
2. MUST run from repository root.
3. MUST preserve the distinction between integration and E2E.
4. MUST report evidence bundle paths when available.
5. IF integration evidence is missing or incomplete, THEN return `BLOCK`.
6. IF integration fails, THEN downstream `test-e2e` MUST be blocked inside the same offline gate.

# Evidence Rules
MUST reference:
- `tests/reports/workspace-validation.json`
- `tests/reports/workspace-validation.md`

SHOULD reference:
- `case-index.json`
- `validation-evidence-bundle.json`
- `evidence-links.md`

# Blocking Policy
- `PASS`: integration validations passed with canonical evidence.
- `FAIL`: integration validations ran and reported failures.
- `BLOCK`: prerequisite contract pass is missing, or formal evidence is missing.
- `FAIL` or `BLOCK` MUST block `test-e2e` within the same offline gate.

# Output Contract
## Scope
<state the exact integration chain>

## Entry
<canonical entry and lane>

## Evidence
- <canonical report paths>
- <evidence bundle paths>

## Verdict
`PASS | FAIL | BLOCK`

## Blocking Impact
`none | block-lower | block-lane | block-release`

## Next Action
Only:
- `rerun: <canonical entry>`
- `escalate: test-e2e | test-offline`
- `handoff: diagnose-* or implement-*`

# Examples
Positive:
- Use `test-integration` after changes that alter runtime-gateway, planner-to-runtime, or HMI-to-backend composition.

Negative:
- Do NOT use `test-integration` alone to claim simulated full-chain closure or machine-level closure.

# Prohibitions
- MUST NOT modify source code.
- MUST NOT relax gates, thresholds, or baselines.
- MUST NOT bypass canonical root-level entry as the source of the final verdict.
- MUST NOT infer higher-layer success from lower-layer success.
- MUST NOT emit ambiguous verdicts such as "mostly pass", "looks OK", or "probably fine".
