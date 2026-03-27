# US2 Runtime Extraction Evidence

## Scope

- 阶段：`ARCH-201 Stage B - 边界收敛与 US2 最小闭环`
- 目标：让 `M7 motion-planning` 回归规划职责，让 `M9 runtime-execution` 承接 runtime/control owner
- canonical 落点：
  - `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/motion/`
  - `modules/runtime-execution/application/include/runtime_execution/application/usecases/motion/`
  - `modules/runtime-execution/runtime/host/runtime/motion/`

## File-Level Changes

1. 新增 M9 motion contracts：
   - `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/motion/IMotionRuntimePort.h`
   - `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/motion/IIOControlPort.h`
2. 新增 M9 motion services：
   - `modules/runtime-execution/application/include/runtime_execution/application/services/motion/MotionControlServiceImpl.h`
   - `modules/runtime-execution/application/services/motion/MotionControlServiceImpl.cpp`
   - `modules/runtime-execution/application/include/runtime_execution/application/services/motion/MotionStatusServiceImpl.h`
   - `modules/runtime-execution/application/services/motion/MotionStatusServiceImpl.cpp`
3. 新增 M9 motion use case：
   - `modules/runtime-execution/application/include/runtime_execution/application/usecases/motion/MotionControlUseCase.h`
   - `modules/runtime-execution/application/usecases/motion/MotionControlUseCase.cpp`
4. 将旧公开头改为 shim/alias：
   - `modules/motion-planning/domain/motion/ports/IMotionRuntimePort.h`
   - `modules/motion-planning/domain/motion/ports/IIOControlPort.h`
   - `modules/workflow/domain/include/domain/motion/ports/IMotionRuntimePort.h`
   - `modules/workflow/domain/include/domain/motion/ports/IIOControlPort.h`
   - `modules/motion-planning/domain/motion/domain-services/MotionControlServiceImpl.h`
   - `modules/motion-planning/domain/motion/domain-services/MotionStatusServiceImpl.h`
   - `modules/workflow/domain/include/domain/motion/domain-services/MotionControlServiceImpl.h`
   - `modules/workflow/domain/include/domain/motion/domain-services/MotionStatusServiceImpl.h`
5. host/provider/container 最小重接：
   - `modules/runtime-execution/runtime/host/runtime/motion/WorkflowMotionRuntimeServicesProvider.*`
   - `modules/runtime-execution/runtime/host/container/ApplicationContainer.Motion.cpp`
   - `modules/runtime-execution/runtime/host/container/ApplicationContainer.System.cpp`
   - `modules/runtime-execution/runtime/host/container/ApplicationContainer.h`
   - `modules/runtime-execution/runtime/host/container/ApplicationContainerFwd.h`
6. gateway 最小重接：
   - `apps/runtime-gateway/transport-gateway/include/siligen/gateway/tcp/tcp_facade_builder.h`
   - `apps/runtime-gateway/transport-gateway/src/facades/tcp/TcpMotionFacade.h`
   - `apps/runtime-gateway/transport-gateway/src/facades/tcp/TcpMotionFacade.cpp`
7. 测试与构建入口：
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

1. 独立验证构建目录 configure / reconfigure：
   - `cmake -S . -B bsc -DSILIGEN_BUILD_TESTS=ON -DSILIGEN_BUILD_TARGET=tests`
2. 边界脚本：
   - `pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/validation/assert-module-boundary-bridges.ps1`
3. 定向 build：
   - `cmake --build bsc --config Debug --target siligen_motion_planning_unit_tests`
   - `cmake --build bsc --config Debug --target siligen_runtime_host_unit_tests`
   - `cmake --build bsc --config Debug --target siligen_transport_gateway`
   - `cmake --build bsc --config Debug --target siligen_planner_cli`
   - `cmake --build bsc --config Debug --target siligen_process_path_unit_tests`
4. process-path 关键用例：
   - `D:\Projects\SiligenSuite\bsc\bin\Debug\siligen_process_path_unit_tests.exe --gtest_filter=ProcessPathFacadeTest.*`
5. 本地验证门禁：
   - `pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/validation/run-local-validation-gate.ps1`

## Current Verification Status

截至 2026-03-27：

- 源码级与文件级检查：已完成
- 独立验证构建目录：`bsc/` configure / reconfigure 成功
- 边界脚本：已执行，报告位于 `tests/reports/module-boundary-bridges/`，结果为 `passed`
  - `finding_count = 0`
  - 报告：`tests/reports/module-boundary-bridges/module-boundary-bridges.md`
- 定向 build：已完成
  - `siligen_motion_planning_unit_tests`：passed
  - `siligen_runtime_host_unit_tests`：passed
  - `siligen_transport_gateway`：passed
  - `siligen_planner_cli`：passed
  - `siligen_process_path_unit_tests`：passed
- process-path 关键用例：`ProcessPathFacadeTest.*` 已通过，确认 `CoordinateTransformSet.owner_module == "M5"`
- 本地验证门禁：已执行，报告目录 `tests/reports/local-validation-gate/20260327-204804/`，结果为 `overall_status: passed`

结论：US2 的代码、shim、host/gateway seam、边界脚本与验证门禁已完成闭环；Stage B 原先阻断验证的 superbuild / contracts / bootstrap seam 问题已在 Stage C 收口。
