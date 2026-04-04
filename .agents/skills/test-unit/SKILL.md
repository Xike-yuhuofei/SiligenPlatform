---
name: test-unit
description: Validate L1 unit tests only.
last_updated: 2026-04-04
---

# Purpose
This skill only validates L1 unit-test conformance.
This skill MUST NOT infer contract, integration, E2E, HIL, or performance readiness.

# Scope Boundaries
- Owns only `L1`.
- Covers focused unit tests near the owning module or app.
- MUST NOT claim lane-level readiness by itself.
- MUST NOT replace `test-contract` or `test-integration`.

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
- `.\test.ps1 -Profile Local -Suite all`

Targeted commands MAY be used only as supplements, for example:
- `python -m pytest .\apps\hmi-app\tests\unit -q`
- `ctest --test-dir .\build -C Debug --output-on-failure`

Final verdict MUST be anchored to canonical root-entry evidence whenever gate status is claimed.

# Execution Rules
1. IF the request is about module boundaries or interface drift, THEN use `test-contract` instead.
2. MUST run from repository root.
3. MUST prefer canonical root entries before targeted commands.
4. IF only targeted commands were executed and no canonical evidence exists, THEN the result MAY support diagnosis but MUST NOT be used as the final gate verdict.
5. IF unit evidence is incomplete, THEN return `BLOCK`.

# Evidence Rules
Primary evidence:
- `tests/reports/workspace-validation.json`
- `tests/reports/workspace-validation.md`

Supporting evidence MAY include framework-native outputs from targeted unit runs.

# Blocking Policy
- `PASS`: required L1 unit validations passed and formal evidence exists.
- `FAIL`: unit validations ran and reported failures.
- `BLOCK`: required evidence is missing or the run did not reach a valid unit-test conclusion.
- `FAIL` or `BLOCK` from this skill SHOULD block promotion to higher layers within the same lane.

# Output Contract
## Scope
<state the exact unit-test scope>

## Entry
<canonical entry first; targeted command only as supplement>

## Evidence
- <formal evidence path 1>
- <formal evidence path 2>

## Verdict
`PASS | FAIL | BLOCK`

## Blocking Impact
`none | block-lower | block-lane | block-release`

## Next Action
Only:
- `rerun: <canonical entry>`
- `escalate: test-contract | test-offline`
- `handoff: diagnose-* or implement-*`

# Examples
Positive:
- Use `test-unit` after internal implementation changes that stay inside a module and do not alter shared contracts.

Negative:
- Do NOT use `test-unit` alone to sign off a cross-module change or DXF-to-runtime chain change.

# Prohibitions
- MUST NOT modify source code.
- MUST NOT relax gates, thresholds, or baselines.
- MUST NOT bypass canonical root-level entry as the source of the final verdict.
- MUST NOT infer higher-layer success from lower-layer success.
- MUST NOT emit ambiguous verdicts such as "mostly pass", "looks OK", or "probably fine".
