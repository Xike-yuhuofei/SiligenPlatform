# ARCH-201 US4 CMP/Trigger Owner Closeout Tasks

- [x] Create a dedicated US4 spec / plan / evidence bundle.
- [x] Move live Trigger/CMP owner implementations into `siligen_dispense_packaging_domain_dispensing`.
- [x] Turn workflow Trigger/CMP compatibility headers into forwarding shims.
- [x] Stop workflow domain targets and tests from relying on workflow-side Trigger/CMP owner implementations.
- [x] Add scoped US4 bridge checks and owner-boundary assertions.
- [x] Run targeted build/test commands and capture results.

## Validation Target

- `siligen_motion_planning_unit_tests.exe`
- `siligen_unit_tests.exe`
- `scripts/validation/assert-module-boundary-bridges.ps1`
