# Quickstart

## Build

- `.\build.ps1`

## Focused validation

- `ctest --test-dir .\build -C Debug --output-on-failure -R siligen_motion_planning_unit_tests`
- `ctest --test-dir .\build -C Debug --output-on-failure -R siligen_dispense_packaging_unit_tests`
- `ctest --test-dir .\build -C Debug --output-on-failure -R siligen_unit_tests`
- `ctest --test-dir .\build -C Debug --output-on-failure -R siligen_runtime_host_unit_tests`
- `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\validation\assert-module-boundary-bridges.ps1`
