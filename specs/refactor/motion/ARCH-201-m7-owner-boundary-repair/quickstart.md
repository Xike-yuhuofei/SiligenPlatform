# Quickstart: ARCH-201 Stage B - 边界收敛与 US2 最小闭环

## 1. 范围确认

1. 确认当前分支为 `refactor/motion/ARCH-201-m7-owner-boundary-repair`
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
   - `pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/validation/assert-module-boundary-bridges.ps1`
2. 预期结果：
   - `tests/reports/module-boundary-bridges/module-boundary-bridges.md` 为 `status: passed`
   - `finding_count = 0`

## 3. 定向构建与测试

使用独立验证构建目录 `bsc/`，按以下顺序执行：

1. `cmake -S . -B bsc -DSILIGEN_BUILD_TESTS=ON -DSILIGEN_BUILD_TARGET=tests`
2. `cmake --build bsc --config Debug --target siligen_motion_planning_unit_tests`
3. `cmake --build bsc --config Debug --target siligen_runtime_host_unit_tests`
4. `cmake --build bsc --config Debug --target siligen_transport_gateway`
5. `cmake --build bsc --config Debug --target siligen_planner_cli`
6. `cmake --build bsc --config Debug --target siligen_process_path_unit_tests`
7. `D:\Projects\SiligenSuite\bsc\bin\Debug\siligen_process_path_unit_tests.exe --gtest_filter=ProcessPathFacadeTest.*`
8. 本地验证门禁：
   - `pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/validation/run-local-validation-gate.ps1`

## 4. 当前验证基线（2026-03-27）

- `bsc/` configure / reconfigure：已验证成功
- 定向 build：`siligen_motion_planning_unit_tests`、`siligen_runtime_host_unit_tests`、`siligen_transport_gateway`、`siligen_planner_cli`、`siligen_process_path_unit_tests` 均已验证成功
- `ProcessPathFacadeTest.*`：已通过，确认 `CoordinateTransformSet.owner_module == "M5"`
- `run-local-validation-gate.ps1`：已通过，报告目录 `tests/reports/local-validation-gate/20260327-204804/`

当前 Stage B 不再受“顶层 reconfigure 失败 / contracts 目标缺失 / boundary findings 未收敛”阻断；后续若继续推进 US3 / US4，应以本验证基线为新的起点。
