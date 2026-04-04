# Quickstart: ARCH-201 Stage B - 边界收敛与 US2 最小闭环

## 1. 范围确认

1. 确认当前工作树承载的是 `ARCH-201 Stage B / US2` residue
   - 当前 closeout 分支为 `spike/motion/SPIKE-153-review-dispense-trajectory`
2. 确认活动工作树只包含 Stage B 白名单目录与 compile-only collateral
   - `modules/motion-planning`
   - `modules/process-path`
   - `modules/runtime-execution`
   - `modules/workflow`
   - `scripts/validation`
   - `shared/contracts/device`
   - `apps/runtime-gateway`

## 2. 门禁脚本

1. 运行边界门禁：
   - `pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/validation/assert-module-boundary-bridges.ps1 -WorkspaceRoot D:/Projects/wt-spike153 -ReportDir tests/reports/module-boundary-bridges-phase2-current`
2. 预期结果：
   - `tests/reports/module-boundary-bridges-phase2-current/module-boundary-bridges.md` 为 `status: passed`
   - `finding_count = 0`

## 3. 定向构建与测试

使用独立验证构建目录 `build-phase2/`，按以下顺序执行：

1. `cmake -S D:/Projects/wt-spike153 -B D:/Projects/wt-spike153/build-phase2 -DSILIGEN_BUILD_TESTS=ON -DSILIGEN_USE_PCH=OFF -DSILIGEN_PARALLEL_COMPILE=ON`
2. `cmake --build D:/Projects/wt-spike153/build-phase2 --config Debug --target siligen_motion_planning_unit_tests siligen_unit_tests siligen_runtime_host_unit_tests --parallel`
3. `cmake --build D:/Projects/wt-spike153/build-phase2 --config Debug --target process_runtime_core_motion_runtime_assembly_test workflow_integration_motion_runtime_assembly_smoke --parallel`
4. `D:/Projects/wt-spike153/build-phase2/bin/Debug/siligen_motion_planning_unit_tests.exe --gtest_filter=MotionPlannerTest.*:MotionPlannerConstraintTest.*:InterpolationProgramPlannerTest.*:MotionPlannerOwnerPathTest.*:MotionPlanningOwnerBoundaryTest.*:NoRuntimeControlLeakTest.*`
5. `D:/Projects/wt-spike153/build-phase2/bin/Debug/siligen_unit_tests.exe --gtest_filter=DispensingProcessServiceWaitForMotionCompleteTest.*:DispensingWorkflowUseCaseTest.*`
6. `D:/Projects/wt-spike153/build-phase2/bin/Debug/siligen_runtime_host_unit_tests.exe --gtest_filter=MotionControlMigrationTest.*:WorkflowMotionRuntimeServicesProviderTest.*`
7. `D:/Projects/wt-spike153/build-phase2/bin/Debug/process_runtime_core_motion_runtime_assembly_test.exe`
8. `D:/Projects/wt-spike153/build-phase2/bin/Debug/workflow_integration_motion_runtime_assembly_smoke.exe`
9. 如需补充 root-entry 证据，再执行：
   - `pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/validation/run-local-validation-gate.ps1 -ReportRoot tests/reports/local-validation-gate-phase2-current`

## 4. 当前验证基线（2026-04-04）

- `build-phase2/` configure：已验证成功
- 定向 build：`siligen_motion_planning_unit_tests`、`siligen_unit_tests`、`siligen_runtime_host_unit_tests`、`process_runtime_core_motion_runtime_assembly_test`、`workflow_integration_motion_runtime_assembly_smoke` 均已验证成功
- 规划 owner 边界回归：16/16 通过
- `workflow/dispense` 邻接 consumer 回归：33/33 通过
- host/runtime seam 回归：5/5 通过
- `MotionRuntimeAssemblyFactory` 两个 smoke 可执行均以 `exit 0` 通过
- 边界脚本：`tests/reports/module-boundary-bridges-phase2-current/module-boundary-bridges.md` 为 `passed`
- `run-local-validation-gate.ps1` 当前复跑报告位于 `tests/reports/local-validation-gate-phase2-current/20260404-073057/`
  - 结果：`failed (7/8)`
  - Stage B 相关步骤全部通过
  - 唯一失败项是 `test-contracts-ci`
  - 具体阻断为 `apps/hmi-app/tests/unit/test_offline_preview_builder.py` 的 `no-loose-mock` 静态门禁，不属于 Stage B / US2 改动面

当前 Stage B / US2 已具备独立收口所需的代码与定向验证证据；若后续继续推进 US3 / US4，应以本验证基线为起点，并把 root-entry 失败视为独立于本阶段的跨域静态门禁阻断。
