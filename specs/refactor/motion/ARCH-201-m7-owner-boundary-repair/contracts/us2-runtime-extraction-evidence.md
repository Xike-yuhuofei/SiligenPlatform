# US2 Runtime Extraction Evidence

## Scope

- 阶段：`ARCH-201 Stage B - 边界收敛与 US2 最小闭环`
- 目标：让 `M7 motion-planning` 回归规划职责，让 `M9 runtime-execution` 承接 runtime/control owner
- canonical 落点：
  - `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/motion/`
  - `modules/runtime-execution/application/include/runtime_execution/application/usecases/motion/`
  - `modules/runtime-execution/runtime/host/runtime/motion/`

## File-Level Changes

1. 收口 M7 canonical planning types：
   - `modules/motion-planning/contracts/include/motion_planning/contracts/MotionTrajectory.h`
   - `modules/motion-planning/contracts/include/motion_planning/contracts/MotionPlan.h`
   - `modules/motion-planning/contracts/include/motion_planning/contracts/MotionPlanningReport.h`
   - `modules/motion-planning/contracts/include/motion_planning/contracts/TimePlanningConfig.h`
2. 收口 M9 motion contracts：
   - `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/motion/IMotionRuntimePort.h`
   - `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/motion/IIOControlPort.h`
   - `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/motion/IInterpolationPort.h`
3. 新增 M9 motion services：
   - `modules/runtime-execution/application/include/runtime_execution/application/services/motion/MotionControlServiceImpl.h`
   - `modules/runtime-execution/application/services/motion/MotionControlServiceImpl.cpp`
   - `modules/runtime-execution/application/include/runtime_execution/application/services/motion/MotionStatusServiceImpl.h`
   - `modules/runtime-execution/application/services/motion/MotionStatusServiceImpl.cpp`
4. 新增 M9 motion use case：
   - `modules/runtime-execution/application/include/runtime_execution/application/usecases/motion/MotionControlUseCase.h`
   - `modules/runtime-execution/application/usecases/motion/MotionControlUseCase.cpp`
5. 将旧公开头收瘦为“删除 owner 实体 + 兼容 alias/shim”：
   - `modules/motion-planning/domain/motion/ports/IMotionRuntimePort.h` 已退出 `M7` owner surface
   - `modules/motion-planning/domain/motion/ports/IIOControlPort.h` 已退出 `M7` owner surface
   - `modules/motion-planning/domain/motion/ports/IInterpolationPort.h` 已退出 `M7` owner surface
   - `modules/motion-planning/domain/motion/value-objects/MotionTrajectory.h` 已退出 `M7` owner surface
   - `modules/motion-planning/domain/motion/value-objects/MotionPlanningReport.h` 已退出 `M7` owner surface
   - `modules/motion-planning/domain/motion/value-objects/TimePlanningConfig.h` 已退出 `M7` owner surface
   - `modules/workflow/domain/include/domain/motion/ports/IMotionRuntimePort.h`
   - `modules/workflow/domain/include/domain/motion/ports/IIOControlPort.h`
   - `modules/workflow/domain/include/domain/motion/ports/IInterpolationPort.h`
   - `modules/workflow/domain/include/domain/motion/value-objects/MotionTrajectory.h`
   - `modules/workflow/domain/include/domain/motion/value-objects/MotionPlanningReport.h`
6. 邻接 domain consumer 对齐到 canonical contracts：
   - `modules/dispense-packaging/domain/dispensing/planning/domain-services/DispensingPlannerService.*`
   - `modules/dispense-packaging/domain/dispensing/domain-services/DispensingController.h`
   - `modules/workflow/domain/domain/dispensing/planning/domain-services/DispensingPlannerService.cpp`
   - `modules/workflow/domain/domain/dispensing/domain-services/DispensingController.h`
7. host/provider/container 最小重接：
   - `modules/runtime-execution/runtime/host/runtime/motion/WorkflowMotionRuntimeServicesProvider.*`
   - `modules/runtime-execution/runtime/host/container/ApplicationContainer.Motion.cpp`
   - `modules/runtime-execution/runtime/host/container/ApplicationContainer.System.cpp`
   - `modules/runtime-execution/runtime/host/container/ApplicationContainer.h`
   - `modules/runtime-execution/runtime/host/container/ApplicationContainerFwd.h`
8. gateway 最小重接：
   - `apps/runtime-gateway/transport-gateway/include/siligen/gateway/tcp/tcp_facade_builder.h`
   - `apps/runtime-gateway/transport-gateway/src/facades/tcp/TcpMotionFacade.h`
   - `apps/runtime-gateway/transport-gateway/src/facades/tcp/TcpMotionFacade.cpp`
9. 测试与构建入口：
   - `modules/motion-planning/tests/CMakeLists.txt`
   - `modules/runtime-execution/runtime/host/tests/CMakeLists.txt`
   - `modules/runtime-execution/application/CMakeLists.txt`
   - `modules/runtime-execution/contracts/runtime/CMakeLists.txt`
   - `modules/motion-planning/tests/unit/domain/motion/NoRuntimeControlLeakTest.cpp`
   - `modules/runtime-execution/runtime/host/tests/unit/runtime/motion/MotionControlMigrationTest.cpp`

## Guardrail Changes

`scripts/validation/assert-module-boundary-bridges.ps1` 已补充 Stage B 的 M7 -> M9 迁移规则，覆盖：

- required references：
  - `MotionControlUseCase.cpp` 已进入 `runtime-execution/application/CMakeLists.txt`
  - `IMotionRuntimePort.h` / `IIOControlPort.h` 已进入 `runtime-execution/contracts/runtime/CMakeLists.txt`
  - host motion provider 已进入 `runtime-execution/runtime/host/CMakeLists.txt`
- forbidden ownership / compat：
  - `motion-planning/domain/motion/CMakeLists.txt` 不得重新编译 `MotionControlServiceImpl.cpp` / `MotionStatusServiceImpl.cpp`
  - `apps/runtime-gateway/.../tcp_facade_builder.h` 不得重新 resolve 四个 legacy workflow motion use case
  - `apps/runtime-gateway/.../TcpMotionFacade.h` 不得重新持有四个 legacy workflow motion use case 成员
  - `modules/motion-planning` / `modules/workflow/domain/include` 的旧 runtime/control 公开头不得重新声明 owner class

## Verification Commands

1. 独立验证构建目录 configure：
   - `cmake -S D:/Projects/wt-spike153 -B D:/Projects/wt-spike153/build-phase2 -DSILIGEN_BUILD_TESTS=ON -DSILIGEN_USE_PCH=OFF -DSILIGEN_PARALLEL_COMPILE=ON`
2. 边界脚本：
   - `pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/validation/assert-module-boundary-bridges.ps1 -WorkspaceRoot D:/Projects/wt-spike153 -ReportDir tests/reports/module-boundary-bridges-phase2-current`
3. 定向 build：
   - `cmake --build D:/Projects/wt-spike153/build-phase2 --config Debug --target siligen_motion_planning_unit_tests siligen_unit_tests siligen_runtime_host_unit_tests --parallel`
   - `cmake --build D:/Projects/wt-spike153/build-phase2 --config Debug --target process_runtime_core_motion_runtime_assembly_test workflow_integration_motion_runtime_assembly_smoke --parallel`
4. 规划 owner 边界回归：
   - `D:/Projects/wt-spike153/build-phase2/bin/Debug/siligen_motion_planning_unit_tests.exe --gtest_filter=MotionPlannerTest.*:MotionPlannerConstraintTest.*:InterpolationProgramPlannerTest.*:MotionPlannerOwnerPathTest.*:MotionPlanningOwnerBoundaryTest.*:NoRuntimeControlLeakTest.*`
5. 邻接 consumer 回归：
   - `D:/Projects/wt-spike153/build-phase2/bin/Debug/siligen_unit_tests.exe --gtest_filter=DispensingProcessServiceWaitForMotionCompleteTest.*:DispensingWorkflowUseCaseTest.*`
6. host/runtime seam 回归：
   - `D:/Projects/wt-spike153/build-phase2/bin/Debug/siligen_runtime_host_unit_tests.exe --gtest_filter=MotionControlMigrationTest.*:WorkflowMotionRuntimeServicesProviderTest.*`
7. runtime assembly smoke：
   - `D:/Projects/wt-spike153/build-phase2/bin/Debug/process_runtime_core_motion_runtime_assembly_test.exe`
   - `D:/Projects/wt-spike153/build-phase2/bin/Debug/workflow_integration_motion_runtime_assembly_smoke.exe`
8. root-entry 补充证据：
   - `pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/validation/run-local-validation-gate.ps1 -ReportRoot tests/reports/local-validation-gate-phase2-current`

## Current Verification Status

截至 2026-04-04：

- 源码级与文件级检查：已完成
- 独立验证构建目录：`build-phase2/` configure 成功
- 边界脚本：已执行，报告位于 `tests/reports/module-boundary-bridges-phase2-current/`，结果为 `passed`
  - `finding_count = 0`
  - 报告：`tests/reports/module-boundary-bridges-phase2-current/module-boundary-bridges.md`
- 定向 build：已完成
  - `siligen_motion_planning_unit_tests`：passed
  - `siligen_unit_tests`：passed
  - `siligen_runtime_host_unit_tests`：passed
  - `process_runtime_core_motion_runtime_assembly_test`：passed
  - `workflow_integration_motion_runtime_assembly_smoke`：passed
- 规划 owner 边界回归：16/16 通过
- 邻接 consumer 回归：33/33 通过
- host/runtime seam 回归：5/5 通过
- runtime assembly smoke：两个可执行均 `exit 0`
- 本地验证门禁：已复跑，报告目录 `tests/reports/local-validation-gate-phase2-current/20260404-073057/`
  - 结果：`overall_status: failed`
  - 通过步骤：`review-baseline`、`workspace-layout`、`dsp-e2e-spec-docset`、`legacy-exit`、`module-boundary-bridges`、`build-validation-local-contracts`、`build-validation-ci-contracts`
  - 唯一失败步骤：`test-contracts-ci`
  - 失败根因：`apps/hmi-app/tests/unit/test_offline_preview_builder.py` 触发 `no-loose-mock` 静态门禁
  - 结论：root-entry gate 当前被 `HMI` 静态测试治理阻断，而非 `ARCH-201 Stage B / US2` 本身阻断

结论：US2 的代码、contracts、consumer 对齐、host seam 与 Stage B 专项验证均已闭环；当前唯一未闭环证据是 root-entry local gate，而其阻断点位于 `apps/hmi-app` 的独立静态门禁，不属于本阶段 owner seam 范围。
