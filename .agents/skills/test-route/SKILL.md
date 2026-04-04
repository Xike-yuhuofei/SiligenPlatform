---
name: test-route
description: Select the correct canonical test skill or lane based on change scope, risk, and target evidence depth.
last_updated: 2026-04-04
---

# Purpose
This skill only routes test requests to the correct canonical `test-*` skill or lane.
This skill MUST NOT run ad hoc validation logic as a replacement for the owning skill.

# Scope Boundaries
- Owns only test-skill routing.
- MUST NOT redefine atomic or orchestrator rules.
- MUST NOT invent new risk levels.
- MUST NOT invent new lanes.

# Inputs
Required:
- changed_scope
- risk_profile: `low | medium | high | hardware-sensitive`
- goal: `gate | regression | evidence`

Optional:
- lane: `auto | quick-gate | full-offline-gate | nightly-performance | limited-hil`
- desired_depth: `auto | quick | full-offline | nightly | hil`
- skip_layer
- skip_justification

# Canonical Entry
This routing skill does not own a separate test authority.
It MUST choose among canonical skills whose final execution uses root-level entries such as:
- `.\build.ps1`
- `.\test.ps1`
- `.\ci.ps1`

# Execution Rules
1. MUST classify `risk_profile` only as `low | medium | high | hardware-sensitive`.
2. IF the user explicitly specifies a canonical lane or owning skill, THEN honor that request unless it violates a prerequisite rule.
3. IF the change is documentation-only, comment-only, or non-runtime-only, THEN default to `test-quick`.
4. IF the change is internal to one module and does not alter contracts, protocols, or runtime chains, THEN default to `test-quick`.
5. IF the change touches boundaries, shared contracts, runtime chains, DXF processing flow, gateway, backend, or HMI interaction, THEN default to `test-offline`.
6. IF the change touches HIL, board-connected timing, hardware interaction, or machine-connected sequencing, THEN default to `test-limited`.
7. IF the change touches throughput, latency, long-run stability, resource trend, or threshold-based performance gates, THEN default to `test-nightly`.
8. IF `risk_profile` is `hardware-sensitive`, THEN the route MUST explicitly justify why `test-limited` or `test-nightly` is not required.
9. IF `skip_layer` is set, THEN `skip_justification` is mandatory and the route MUST preserve prerequisite rules.
10. MUST default to the safer higher gate when ambiguity remains.

# Evidence Rules
Routing output MUST state:
- selected skill or lane
- reason for selection
- prerequisite constraints that still apply

# Blocking Policy
- `PASS`: a valid route was selected with prerequisite constraints preserved.
- `FAIL`: the route request is internally inconsistent but still evaluable.
- `BLOCK`: the requested route violates prerequisite rules or lacks required input.
- `BLOCK` means no execution should proceed until routing is corrected.

# Output Contract
## Scope
<state the classified change scope>

## Entry
<state the selected skill or canonical lane>

## Evidence
- <state the formal evidence roots that the selected skill will rely on>

## Verdict
`PASS | FAIL | BLOCK`

## Blocking Impact
`none | block-lower | block-lane | block-release`

## Next Action
Only:
- `rerun: <selected canonical skill or lane>`
- `escalate: <safer higher gate>`
- `handoff: <selected owning skill>`

# Examples
Positive:
- Use `test-route` first when the change scope is known but the correct lane or owning `test-*` skill is not yet obvious.

Negative:
- Do NOT use `test-route` as a substitute for the actual validating skill after the target lane has already been determined.

# Prohibitions
- MUST NOT modify source code.
- MUST NOT relax gates, thresholds, or baselines.
- MUST NOT bypass canonical root-level entry as the source of the final verdict.
- MUST NOT infer higher-layer success from lower-layer success.
- MUST NOT emit ambiguous verdicts such as "mostly pass", "looks OK", or "probably fine".
