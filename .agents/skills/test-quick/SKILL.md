---
name: test-quick
description: Run the canonical quick gate by orchestrating L0, core L1, and core L2 only.
last_updated: 2026-04-04
---

# Purpose
This skill only orchestrates the canonical `quick-gate` lane.
This skill MUST NOT redefine atomic-skill rules or modify gate composition.

# Scope Boundaries
- Owns only the `quick-gate` lane.
- Composes `test-static -> test-unit -> test-contract`.
- MUST NOT duplicate the internal rules of atomic skills.
- MUST NOT be used for integration, E2E, HIL, or performance sign-off.

# Inputs
Required:
- changed_scope
- risk_profile: `low | medium | high | hardware-sensitive`
- goal: `gate | regression | evidence`

Optional:
- lane: `auto | quick-gate`
- desired_depth: `auto | quick | full-offline | nightly | hil`
- skip_layer
- skip_justification

# Canonical Entry
MUST use:
- `.\test.ps1 -Lane quick-gate`
- `.\ci.ps1 -Suite all` only when CI-grade gate evidence is explicitly required

Final lane verdict MUST be anchored to canonical quick-gate outputs.

# Execution Rules
1. MUST run from repository root.
2. MUST execute in this order: `test-static -> test-unit -> test-contract`.
3. MUST keep orchestration logic separate from atomic-skill rules.
4. MUST NOT skip `test-static`.
5. IF `skip_layer` is set, THEN `skip_justification` is mandatory.
6. IF `test-static` returns `BLOCK`, THEN stop immediately and return `BLOCK`.
7. IF `test-unit` or `test-contract` returns `FAIL` or `BLOCK`, THEN return lane verdict `FAIL` or `BLOCK` accordingly and stop.
8. This skill MUST remain a fast gate and MUST NOT expand into `L3+`.

# Evidence Rules
MUST reference:
- `tests/reports/static/`
- `tests/reports/workspace-validation.json`
- `tests/reports/workspace-validation.md`

# Blocking Policy
- `PASS`: all required quick-gate components passed.
- `FAIL`: an executed lower layer failed.
- `BLOCK`: L0 blocked progression, required evidence is missing, or an invalid skip was requested.
- `BLOCK` from this lane blocks quick-gate approval.

# Output Contract
## Scope
<state the exact quick-gate scope>

## Entry
<canonical quick-gate entry>

## Evidence
- <static report paths>
- <workspace validation paths>

## Verdict
`PASS | FAIL | BLOCK`

## Blocking Impact
`none | block-lower | block-lane | block-release`

## Next Action
Only:
- `rerun: .\test.ps1 -Lane quick-gate`
- `escalate: test-offline`
- `handoff: diagnose-* or implement-*`

# Examples
Positive:
- Use `test-quick` for small or medium internal changes when a fast repository-root gate is needed before commit or before broader offline validation.

Negative:
- Do NOT use `test-quick` when the change touches integration chains, simulated full-chain closure, HIL, or performance.

# Prohibitions
- MUST NOT modify source code.
- MUST NOT relax gates, thresholds, or baselines.
- MUST NOT bypass canonical root-level entry as the source of the final verdict.
- MUST NOT infer higher-layer success from lower-layer success.
- MUST NOT emit ambiguous verdicts such as "mostly pass", "looks OK", or "probably fine".
