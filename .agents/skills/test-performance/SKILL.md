---
name: test-performance
description: Validate L6 performance and stability tests only.
last_updated: 2026-04-04
---

# Purpose
This skill only validates L6 performance and stability conformance.
This skill MUST NOT rewrite functional-correctness conclusions or modify thresholds.

# Scope Boundaries
- Owns only `L6`.
- Covers performance and long-run stability evidence.
- MUST treat `threshold_gate` as the blocking authority for the nightly lane.
- MUST NOT treat baseline drift alone as the formal blocking gate.

# Inputs
Required:
- changed_scope
- risk_profile: `low | medium | high | hardware-sensitive`
- goal: `gate | regression | evidence`

Optional:
- desired_depth: `auto | quick | full-offline | nightly | hil`

# Canonical Entry
MUST use canonical root entries for the nightly lane:
- `.\test.ps1 -Lane nightly-performance`
- `python -m test_kit.workspace_validation --lane nightly-performance --suite performance` only after environment bootstrap sets `PYTHONPATH` to `shared\testing\test-kit\src`; prefer `.\test.ps1`, which already delegates through `scripts\validation\invoke-workspace-tests.ps1`

The canonical nightly lane SHOULD preserve its documented arguments:
- `--include-start-job`
- `--include-control-cycles`
- `--pause-resume-cycles 3`
- `--stop-reset-rounds 3`
- `--long-run-minutes 5`
- `--gate-mode nightly-performance`
- `--threshold-config tests/baselines/performance/dxf-preview-profile-thresholds.json`

# Execution Rules
1. MUST run from repository root.
2. MUST preserve the distinction between performance failure and functional failure.
3. MUST treat the nightly `threshold_gate` as the formal blocking authority.
4. MUST verify that required report tables exist, including `Control Cycle` and `Long Run`.
5. IF formal performance evidence is missing, THEN return `BLOCK`.

# Evidence Rules
MUST reference:
- `tests/reports/performance/dxf-preview-profiles/`

Required report content includes:
- Preview / Execution / Single Flight
- Control Cycle
- Long Run

# Blocking Policy
- `PASS`: performance thresholds passed and required reports exist.
- `FAIL`: the nightly run completed and threshold-based performance gates failed.
- `BLOCK`: formal evidence is missing or incomplete.
- `FAIL` from this skill MUST block performance release decisions, but MUST NOT rewrite L0-L5 correctness conclusions.

# Output Contract
## Scope
<state the exact performance scope>

## Entry
<canonical nightly-performance entry>

## Evidence
- `tests/reports/performance/dxf-preview-profiles/`
- <specific threshold-gate artifact if present>

## Verdict
`PASS | FAIL | BLOCK`

## Blocking Impact
`none | block-lower | block-lane | block-release`

## Next Action
Only:
- `rerun: <canonical nightly entry>`
- `escalate: test-nightly`
- `handoff: diagnose-* or implement-*`

# Examples
Positive:
- Use `test-performance` when the change may affect throughput, latency, long-run stability, or resource trends.

Negative:
- Do NOT use `test-performance` to conclude that runtime correctness or HIL closure passed.

# Prohibitions
- MUST NOT modify source code.
- MUST NOT relax gates, thresholds, or baselines.
- MUST NOT bypass canonical root-level entry as the source of the final verdict.
- MUST NOT infer higher-layer success from lower-layer success.
- MUST NOT emit ambiguous verdicts such as "mostly pass", "looks OK", or "probably fine".
