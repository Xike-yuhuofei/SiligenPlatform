---
name: test-e2e
description: Validate L4 simulated end-to-end tests only.
last_updated: 2026-04-04
---

# Purpose
This skill only validates L4 simulated end-to-end conformance.
This skill MUST NOT claim HIL or physical-machine closure.

# Scope Boundaries
- Owns only `L4`.
- Covers simulated full-chain closure.
- MUST keep simulated closure distinct from machine closure.
- MUST NOT replace `test-hil`.

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

Final verdict MUST be rooted in canonical offline-gate outputs and evidence bundles.

# Execution Rules
1. IF integration validation has not passed, THEN return `BLOCK`.
2. MUST run from repository root.
3. MUST describe the result as simulated end-to-end only.
4. MUST attach or reference formal evidence bundle artifacts.
5. IF formal E2E evidence is missing, THEN return `BLOCK`.

# Evidence Rules
MUST reference:
- `tests/reports/workspace-validation.json`
- `tests/reports/workspace-validation.md`

SHOULD reference:
- `case-index.json`
- `validation-evidence-bundle.json`
- `evidence-links.md`

# Blocking Policy
- `PASS`: simulated end-to-end validations passed with canonical evidence.
- `FAIL`: simulated end-to-end validations ran and reported failures.
- `BLOCK`: prerequisite integration pass is missing, or formal evidence is missing.
- `FAIL` from this skill MUST NOT be rewritten as HIL failure; it only blocks offline closure.

# Output Contract
## Scope
<state the exact simulated E2E chain>

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
- `escalate: test-offline | test-limited`
- `handoff: diagnose-* or implement-*`

# Examples
Positive:
- Use `test-e2e` after changes that affect the simulated full chain from engineering input through runtime execution preparation.

Negative:
- Do NOT use `test-e2e` to sign off real HIL timing or board-connected behavior.

# Prohibitions
- MUST NOT modify source code.
- MUST NOT relax gates, thresholds, or baselines.
- MUST NOT bypass canonical root-level entry as the source of the final verdict.
- MUST NOT infer higher-layer success from lower-layer success.
- MUST NOT emit ambiguous verdicts such as "mostly pass", "looks OK", or "probably fine".
