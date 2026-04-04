# Implementation Plan: ARCH-201 Stage D - Planning Owner Hard Cut

## Summary

本阶段先完成第一批 hard cut：删除 `workflow` 内已无 live consumer 的 planning residual，并把门禁升级为“这些残留文件必须不存在”。后续批次再继续补 owner-side tests 与 consumer migration 证据。

## Canonical Owner

- planning domain owner: `modules/dispense-packaging/domain/dispensing/planning/`
- planning assembly owner: `modules/dispense-packaging/application/services/dispensing/`
- workflow role: orchestrator only

## First Batch Scope

1. 删除 `workflow` planning residual `.h/.cpp`
2. 删除 dead `PlanningPreviewAssemblyService`
3. 更新 boundary script 与 owner boundary test
4. 更新模块 README 与 Stage D 规格资产

## Validation

- `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/validation/assert-module-boundary-bridges.ps1 -WorkspaceRoot D:/Projects/wt-arch201d`
- `cmake --build C:/Users/Xike/AppData/Local/SiligenSuite/control-apps-build-arch201c --config Debug --target siligen_motion_planning_unit_tests siligen_unit_tests siligen_dispense_packaging_unit_tests --parallel`
- `ctest --test-dir C:/Users/Xike/AppData/Local/SiligenSuite/control-apps-build-arch201c -C Debug --output-on-failure -R "siligen_motion_planning_unit_tests|siligen_unit_tests|siligen_dispense_packaging_unit_tests"`
