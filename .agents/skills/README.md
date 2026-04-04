# Test Skills Pack

- Pack Version: `1.0`
- Last Updated: `2026-04-04`

This package freezes the reusable test-skill system for the repository around the canonical root-level authorities, the formal `L0-L6` layered test vocabulary, and the formal lanes `quick-gate`, `full-offline-gate`, `nightly-performance`, and `limited-hil`.

## Layers
### Atomic Skills
- `test-static` -> `L0`
- `test-unit` -> `L1`
- `test-contract` -> `L2`
- `test-integration` -> `L3`
- `test-e2e` -> `L4`
- `test-hil` -> `L5`
- `test-performance` -> `L6`

### Orchestrator Skills
- `test-quick` -> `quick-gate`
- `test-offline` -> `full-offline-gate`
- `test-nightly` -> `nightly-performance`
- `test-limited` -> `limited-hil`

### Routing Skill
- `test-route`

## Selection Guide
- Use `test-route` when the correct lane is not yet obvious.
- Use `test-quick` for low-risk or internal changes that do not alter contracts or runtime chains.
- Use `test-offline` for boundary changes, contract drift, DXF flow changes, runtime composition changes, gateway changes, and HMI/backend interaction changes.
- Use `test-limited` for HIL or machine-connected changes after offline prerequisites are satisfied.
- Use `test-nightly` for throughput, latency, long-run stability, resource-trend, or threshold-gate changes.

## Canonical Authorities
Final gate conclusions MUST remain tied to repository-root canonical entries and their formal report roots:
- `.\build.ps1`
- `.\test.ps1`
- `.\ci.ps1`
- `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\validation\run-local-validation-gate.ps1`

## Evidence Roots
- `tests/reports/static/`
- `tests/reports/workspace-validation.json`
- `tests/reports/workspace-validation.md`
- `tests/reports/ci/`
- `case-index.json`
- `validation-evidence-bundle.json`
- `evidence-links.md`
- `tests/reports/performance/dxf-preview-profiles/`
- `tests/reports/hil-controlled-test/**`

## Maintenance Rule
Update these skills only when one of the following changes:
- canonical root-level entries
- formal `L0-L6` definitions
- formal lane mappings
- formal evidence roots
- repository test-authority rules in `AGENTS.md`
