---
name: test-static
description: Validate L0 static and build gates only.
last_updated: 2026-04-04
---

# Purpose
This skill only validates L0 static and build gate conformance.
This skill MUST NOT diagnose root cause or modify static-gate policies.

# Scope Boundaries
- Owns only `L0`.
- Treats `pyrightconfig.json`, `scripts/validation/run-pyright-gate.ps1`, and `scripts/testing/check_no_loose_mock.py` as current L0 authority anchors.
- MUST NOT claim `L1-L6` success.
- MUST NOT route to another skill; routing belongs to `test-route`.

# Inputs
Required:
- changed_scope
- risk_profile: `low | medium | high | hardware-sensitive`
- goal: `gate | regression | evidence`

Optional:
- desired_depth: `auto | quick | full-offline | nightly | hil`

# Canonical Entry
MUST use repository-root canonical entries:
- `.\build.ps1`
- `.\test.ps1 -Lane quick-gate`
- `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\validation\run-local-validation-gate.ps1`

Ad hoc commands MAY appear only as supporting context.
Final verdict MUST come from canonical entry outputs and canonical report paths.

# Execution Rules
1. IF the request is not about `L0`, THEN stop and redirect to the owning skill.
2. MUST run from repository root.
3. MUST treat `tests/reports/static/` as the formal report root for static gates.
4. IF static-gate evidence is missing, THEN return `BLOCK`.
5. IF any L0 gate fails, THEN set verdict to `BLOCK`.
6. MUST state that `L0` failure blocks `quick-gate` and blocks progression into `L3/L4/L5`.

# Evidence Rules
MUST reference:
- `tests/reports/static/pyright-report.*`
- `tests/reports/static/no-loose-mock-report.*`

MAY also reference:
- `tests/reports/workspace-validation.json`
- `tests/reports/workspace-validation.md`

# Blocking Policy
- `PASS`: all required L0 gates passed and formal evidence exists.
- `FAIL`: the run completed but reported non-blocking issues within L0 support context; this state SHOULD be rare.
- `BLOCK`: any required L0 gate failed, or formal evidence is missing.
- `BLOCK` from this skill MUST block `quick-gate` and MUST prohibit progression into `L3/L4/L5`.

# Output Contract
## Scope
<state the exact static/build scope that was validated>

## Entry
<list canonical root entries and parameters used>

## Evidence
- <formal report path 1>
- <formal report path 2>

## Verdict
`PASS | FAIL | BLOCK`

## Blocking Impact
`none | block-lower | block-lane | block-release`

## Next Action
Only one of:
- `rerun: <canonical entry>`
- `escalate: <higher skill or lane>`
- `handoff: <diagnose-* or implement-* skill>`

# Examples
Positive:
- Use `test-static` before any quick gate when code changes may affect typing, buildability, or mocking discipline.

Negative:
- Do NOT use `test-static` to claim that integration, E2E, HIL, or performance validation has passed.

# Prohibitions
- MUST NOT modify source code.
- MUST NOT relax gates, thresholds, or baselines.
- MUST NOT bypass canonical root-level entry as the source of the final verdict.
- MUST NOT infer higher-layer success from lower-layer success.
- MUST NOT emit ambiguous verdicts such as "mostly pass", "looks OK", or "probably fine".
