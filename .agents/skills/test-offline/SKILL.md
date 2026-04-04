---
name: test-offline
description: Run the canonical full-offline gate by orchestrating L0 through L4.
last_updated: 2026-04-04
---

# Purpose
This skill only orchestrates the canonical `full-offline-gate` lane.
This skill MUST NOT redefine atomic-skill rules or claim HIL closure.

# Scope Boundaries
- Owns only the `full-offline-gate` lane.
- Composes `test-static -> test-unit -> test-contract -> test-integration -> test-e2e`.
- MUST NOT duplicate atomic-skill rules.
- MUST NOT claim physical machine closure.

# Inputs
Required:
- changed_scope
- risk_profile: `low | medium | high | hardware-sensitive`
- goal: `gate | regression | evidence`

Optional:
- lane: `auto | full-offline-gate`
- desired_depth: `auto | quick | full-offline | nightly | hil`
- skip_layer
- skip_justification

# Canonical Entry
MUST use:
- `.\test.ps1 -Lane full-offline-gate`
- `python -m test_kit.workspace_validation` only after environment bootstrap sets `PYTHONPATH` to `shared\testing\test-kit\src`; prefer `.\test.ps1`, which already delegates through `scripts\validation\invoke-workspace-tests.ps1`

Final lane verdict MUST be anchored to canonical full-offline-gate outputs.

# Execution Rules
1. MUST run from repository root.
2. MUST execute in this order: `test-static -> test-unit -> test-contract -> test-integration -> test-e2e`.
3. MUST NOT skip `test-static`.
4. MUST NOT skip `test-contract`.
5. IF `skip_layer` is set, THEN `skip_justification` is mandatory.
6. IF `test-static` returns `BLOCK`, THEN stop immediately and return `BLOCK`.
7. IF `test-contract` fails or blocks, THEN `test-integration` and `test-e2e` MUST NOT run.
8. IF `test-integration` fails or blocks, THEN `test-e2e` MUST NOT run.
9. MUST describe its result as offline closure only.

# Evidence Rules
MUST reference:
- `tests/reports/static/`
- `tests/reports/workspace-validation.json`
- `tests/reports/workspace-validation.md`

SHOULD reference:
- `case-index.json`
- `validation-evidence-bundle.json`
- `evidence-links.md`

# Blocking Policy
- `PASS`: all required `L0-L4` validations passed with canonical evidence.
- `FAIL`: a required executed layer failed.
- `BLOCK`: required evidence is missing, invalid skips were requested, or prerequisite layers blocked progression.
- `FAIL` or `BLOCK` from this lane blocks offline-gate approval.

# Output Contract
## Scope
<state the exact offline-gate scope>

## Entry
<canonical full-offline-gate entry>

## Evidence
- <static reports>
- <workspace validation reports>
- <evidence bundle paths>

## Verdict
`PASS | FAIL | BLOCK`

## Blocking Impact
`none | block-lower | block-lane | block-release`

## Next Action
Only:
- `rerun: .\test.ps1 -Lane full-offline-gate`
- `escalate: test-limited | test-nightly`
- `handoff: diagnose-* or implement-*`

# Examples
Positive:
- Use `test-offline` after changes that affect boundaries, runtime chains, DXF flow, backend composition, or any offline end-to-end behavior.

Negative:
- Do NOT use `test-offline` to sign off board-connected HIL behavior or long-run performance thresholds.

# Prohibitions
- MUST NOT modify source code.
- MUST NOT relax gates, thresholds, or baselines.
- MUST NOT bypass canonical root-level entry as the source of the final verdict.
- MUST NOT infer higher-layer success from lower-layer success.
- MUST NOT emit ambiguous verdicts such as "mostly pass", "looks OK", or "probably fine".
