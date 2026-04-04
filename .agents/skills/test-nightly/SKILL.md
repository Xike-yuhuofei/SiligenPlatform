---
name: test-nightly
description: Run the canonical nightly-performance lane by orchestrating L6 and its formal nightly evidence checks.
last_updated: 2026-04-04
---

# Purpose
This skill only orchestrates the canonical `nightly-performance` lane.
This skill MUST NOT redefine atomic performance rules or rewrite correctness verdicts from lower layers.

# Scope Boundaries
- Owns only the `nightly-performance` lane.
- Orchestrates `test-performance` and nightly report completeness checks.
- MUST NOT be used as a replacement for offline or HIL gates.
- MUST NOT redefine the nightly threshold authority.

# Inputs
Required:
- changed_scope
- risk_profile: `low | medium | high | hardware-sensitive`
- goal: `gate | regression | evidence`

Optional:
- lane: `auto | nightly-performance`
- desired_depth: `auto | quick | full-offline | nightly | hil`
- skip_layer
- skip_justification

# Canonical Entry
MUST use:
- `.\test.ps1 -Lane nightly-performance`
- `python -m test_kit.workspace_validation --lane nightly-performance --suite performance` only after environment bootstrap sets `PYTHONPATH` to `shared\testing\test-kit\src`; prefer `.\test.ps1`, which already delegates through `scripts\validation\invoke-workspace-tests.ps1`

The canonical nightly lane SHOULD preserve documented arguments and threshold configuration.

# Execution Rules
1. MUST run from repository root.
2. MUST orchestrate `test-performance` without redefining its atomic rules.
3. MAY reuse `L3/L4` samples when the canonical nightly lane does so, but MUST NOT rewrite the lane definition.
4. IF `skip_layer` is set, THEN `skip_justification` is mandatory.
5. MAY skip sample-reuse steps only when explicitly justified.
6. MUST NOT skip the threshold gate.
7. MUST verify the existence of required report sections, including `Control Cycle` and `Long Run`.

# Evidence Rules
MUST reference:
- `tests/reports/performance/dxf-preview-profiles/`

SHOULD mention the threshold-gate artifact or equivalent canonical report entry.

# Blocking Policy
- `PASS`: nightly performance validation passed with complete reports.
- `FAIL`: the nightly threshold gate failed.
- `BLOCK`: canonical evidence is missing, required report sections are absent, or an invalid skip was requested.
- `FAIL` or `BLOCK` from this lane blocks performance release decisions.

# Output Contract
## Scope
<state the exact nightly scope>

## Entry
<canonical nightly-performance entry>

## Evidence
- `tests/reports/performance/dxf-preview-profiles/`
- <threshold-gate artifact if present>

## Verdict
`PASS | FAIL | BLOCK`

## Blocking Impact
`none | block-lower | block-lane | block-release`

## Next Action
Only:
- `rerun: .\test.ps1 -Lane nightly-performance`
- `escalate: test-performance`
- `handoff: diagnose-* or implement-*`

# Examples
Positive:
- Use `test-nightly` when the target change may affect throughput, long-run timing, resource growth, or stability trends.

Negative:
- Do NOT use `test-nightly` as a replacement for `test-offline` or `test-limited`.

# Prohibitions
- MUST NOT modify source code.
- MUST NOT relax gates, thresholds, or baselines.
- MUST NOT bypass canonical root-level entry as the source of the final verdict.
- MUST NOT infer higher-layer success from lower-layer success.
- MUST NOT emit ambiguous verdicts such as "mostly pass", "looks OK", or "probably fine".
