---
name: test-limited
description: Run the canonical limited-hil lane by orchestrating L5 admission and HIL validation.
last_updated: 2026-04-04
---

# Purpose
This skill only orchestrates the canonical `limited-hil` lane.
This skill MUST NOT bypass offline admission requirements or redefine HIL atomic rules.

# Scope Boundaries
- Owns only the `limited-hil` lane.
- Orchestrates offline admission checks and `test-hil`.
- MUST NOT replace full machine validation planning outside the canonical limited-hil lane.
- MUST NOT duplicate atomic HIL rules.

# Inputs
Required:
- changed_scope
- risk_profile: `low | medium | high | hardware-sensitive`
- goal: `gate | regression | evidence`

Optional:
- lane: `auto | limited-hil`
- desired_depth: `auto | quick | full-offline | nightly | hil`
- skip_layer
- skip_justification

# Canonical Entry
MUST use:
- `.\test.ps1 -Lane limited-hil`
- canonical root-entry parameters that enable `-IncludeHilClosedLoop` and `-IncludeHilCaseMatrix` when the lane requires them

Final lane verdict MUST be anchored to canonical limited-hil outputs.

# Execution Rules
1. MUST run from repository root.
2. MUST confirm offline admission coverage for `contracts + integration + e2e + protocol-compatibility` before `test-hil`.
3. MUST orchestrate `test-hil` without redefining its atomic rules.
4. IF `skip_layer` is set, THEN `skip_justification` is mandatory.
5. MUST NOT skip offline admission requirements.
6. IF offline prerequisites are not satisfied, THEN return `BLOCK`.
7. MAY include a small amount of `L6` sampling only when the canonical lane does so.

# Evidence Rules
MUST reference:
- `tests/reports/hil-controlled-test/**`

SHOULD reference supporting canonical offline evidence when needed to prove admission coverage.

# Blocking Policy
- `PASS`: limited-hil admission and HIL validation passed with canonical evidence.
- `FAIL`: HIL validation ran and failed.
- `BLOCK`: offline admission requirements were not satisfied, canonical HIL evidence is missing, or an invalid skip was requested.
- `FAIL` or `BLOCK` from this lane blocks HIL approval.

# Output Contract
## Scope
<state the exact limited-hil scope>

## Entry
<canonical limited-hil entry>

## Evidence
- `tests/reports/hil-controlled-test/**`
- <supporting offline evidence if needed>

## Verdict
`PASS | FAIL | BLOCK`

## Blocking Impact
`none | block-lower | block-lane | block-release`

## Next Action
Only:
- `rerun: .\test.ps1 -Lane limited-hil`
- `escalate: test-hil`
- `handoff: diagnose-* or implement-*`

# Examples
Positive:
- Use `test-limited` when the change touches machine-connected behavior and offline prerequisites are already expected to be satisfied.

Negative:
- Do NOT use `test-limited` to skip contract, integration, or simulated E2E admission checks.

# Prohibitions
- MUST NOT modify source code.
- MUST NOT relax gates, thresholds, or baselines.
- MUST NOT bypass canonical root-level entry as the source of the final verdict.
- MUST NOT infer higher-layer success from lower-layer success.
- MUST NOT emit ambiguous verdicts such as "mostly pass", "looks OK", or "probably fine".
