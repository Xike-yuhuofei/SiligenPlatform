# Implementation Plan: ARCH-201 Stage D - Planning Owner Hard Cut

## Summary

本阶段先完成第一批 hard cut：删除 `workflow` 内已无 live consumer 的 planning residual，并把门禁升级为“这些残留文件必须不存在”。后续已继续完成第二批 docs/index tightening 与第三批 seam/test/evidence closeout，为 `PR #95` 转 ready for review 提供直接依据。

## Canonical Owner

- planning domain owner: `modules/dispense-packaging/domain/dispensing/planning/`
- planning assembly owner: `modules/dispense-packaging/application/services/dispensing/`
- workflow role: orchestrator only

## Closeout Scope

1. 删除 `workflow` planning residual `.h/.cpp`
2. 删除 dead `PlanningPreviewAssemblyService`
3. 更新 boundary script 与 owner boundary test
4. 清理现行 docs/index 中对 removed workflow planning artifacts 的残留
5. 为 `AuthorityPreviewAssemblyService` / `ExecutionAssemblyService` 增加 direct owner-side tests
6. 为 `PlanningUseCase` 增加 split seam parity test，并修正 workflow 基线文档真值

## Residual Policy

- `workflow/domain/domain/dispensing/planning/**` 与 `workflow/domain/include/domain/dispensing/planning/**` 的 removed artifacts 必须持续不存在。
- 当前 docs/index 不允许再把上述 removed artifacts 当作 live 文件树或 live owner 证据。
- 历史评审文档与旧 spec 中对 removed artifacts 的引用视为保留证据，不纳入 live residual 清理目标。

## Validation

- `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/validation/assert-module-boundary-bridges.ps1 -WorkspaceRoot D:/Projects/wt-arch201d`
- `cmake --build C:/Users/Xike/AppData/Local/SiligenSuite/control-apps-build-arch201c --config Debug --target siligen_motion_planning_unit_tests siligen_unit_tests siligen_dispense_packaging_unit_tests --parallel`
- `ctest --test-dir C:/Users/Xike/AppData/Local/SiligenSuite/control-apps-build-arch201c -C Debug --output-on-failure -R "siligen_motion_planning_unit_tests|siligen_unit_tests|siligen_dispense_packaging_unit_tests"`

## PR Ready Conditions

- `PR #95` 继续保持 stacked 到 Stage C 分支，不改 base 到 `ARCH-201-mainline-trajectory`
- 当前分支只保留 Stage D planning owner hard cut 相关改动，无 unrelated residue
- 边界脚本、`siligen_motion_planning_unit_tests`、`siligen_unit_tests`、`siligen_dispense_packaging_unit_tests` 全绿
- Stage D 规格资产、workflow 基线文档与模块索引对 live seam 的描述一致
