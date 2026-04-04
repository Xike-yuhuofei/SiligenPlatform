---
name: test-hil
description: Validate L5 hardware-in-the-loop closed-loop tests only.
last_updated: 2026-04-04
---

# Purpose
This skill only validates L5 hardware-in-the-loop closed-loop conformance.
This skill MUST NOT bypass offline prerequisites or claim L6 performance success.

# Scope Boundaries
- Owns only `L5`.
- Covers HIL closed-loop validation.
- MUST treat offline prerequisites as mandatory admission requirements.
- MUST NOT act as the lane orchestrator; lane composition belongs to `test-limited`.

# Inputs
Required:
- changed_scope
- risk_profile: `low | medium | high | hardware-sensitive`
- goal: `gate | regression | evidence`

Optional:
- desired_depth: `auto | quick | full-offline | nightly | hil`

# Canonical Entry
MUST use canonical root entries capable of `limited-hil`, including:
- `.\test.ps1 -Lane limited-hil`
- root-entry arguments that enable `-IncludeHilClosedLoop` and `-IncludeHilCaseMatrix` when required

Final verdict MUST be tied to canonical HIL reports.

# Execution Rules
1. IF offline prerequisites have not covered `contracts + integration + e2e + protocol-compatibility`, THEN return `BLOCK`.
2. MUST run from repository root.
3. MUST preserve the distinction between HIL closure and performance validation.
4. MUST reference the formal HIL report root.
5. IF HIL evidence is missing, THEN return `BLOCK`.

# Evidence Rules
MUST reference:
- `tests/reports/hil-controlled-test/**`

MAY reference supporting canonical reports from the prerequisite offline gate.

# Blocking Policy
- `PASS`: HIL validations passed with formal HIL evidence.
- `FAIL`: HIL validations ran and reported failures.
- `BLOCK`: offline prerequisites are not satisfied, or formal HIL evidence is missing.
- `FAIL` from this skill states only that machine-connected closed-loop validation did not pass; it MUST NOT automatically invalidate all offline results.

# Output Contract
## Scope
<state the exact HIL scope>

## Entry
<canonical limited-hil entry and parameters>

## Evidence
- `tests/reports/hil-controlled-test/**`
- <supporting prerequisite report paths if needed>

## Verdict
`PASS | FAIL | BLOCK`

## Blocking Impact
`none | block-lower | block-lane | block-release`

## Next Action
Only:
- `rerun: <canonical limited-hil entry>`
- `escalate: test-limited`
- `handoff: diagnose-* or implement-*`

# Examples
Positive:
- Use `test-hil` only after required offline prerequisites have already passed and the target change touches HIL or machine-connected behavior.

Negative:
- Do NOT use `test-hil` as a shortcut to avoid contract, integration, or simulated E2E validation.

# Prohibitions
- MUST NOT modify source code.
- MUST NOT relax gates, thresholds, or baselines.
- MUST NOT bypass canonical root-level entry as the source of the final verdict.
- MUST NOT infer higher-layer success from lower-layer success.
- MUST NOT emit ambiguous verdicts such as "mostly pass", "looks OK", or "probably fine".
