# Historical Test Assets

This archive stores non-core diagnostics and test-support implementations removed from `control-core` during the non-core extraction pass on 2026-03-16.

Archived in this batch:
- diagnostics health aggregation adapter (`DiagnosticsPortAdapter`)
- CMP test preset service
- test record mock repository
- test configuration adapter (`IniTestConfigurationAdapter`)

Not archived yet:
- `HardwareTestAdapter` remains in the runtime tree temporarily because the current trigger control path still depends on `IHardwareTestPort`.
