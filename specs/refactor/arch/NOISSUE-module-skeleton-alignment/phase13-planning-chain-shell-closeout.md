# Phase 13: planning-chain shell-only closeout

更新时间：`2026-03-25`

## 目标

在不把 `motion-planning` 强行推进到高风险 hard closeout 的前提下，完成规划链中低风险模块的连续收口，并把 `workflow` 的 planning / dispensing 兼容 interface target 切到新的模块 canonical target。

本阶段目标分为两类：

- `process-planning`
- `process-path`
- `dispense-packaging`

推进到：

- `canonical-only implementation; shell-only bridges`

同时把 `motion-planning` 固定为：

- `bridge-backed; residual runtime/control surface frozen`

## 实施内容

### 1. process-planning shell-only closeout

- 保持 `modules/process-planning/CMakeLists.txt` 继续直接链接 `siligen_process_planning_domain_configuration`
- 物理删除 `modules/process-planning/src/configuration/**`
- 物理删除 `modules/process-planning/src/planning/**`
- `modules/process-planning/src` 当前仅剩 `README.md`

### 2. process-path canonical target 固化与 shell-only closeout

本阶段为 `modules/process-path/domain/trajectory` 建立独立 canonical target：

- `siligen_process_path_domain_trajectory`

并同步完成：

- `modules/process-path/CMakeLists.txt`
  - 模块根 target 改为直接链接 `siligen_process_path_domain_trajectory`
  - 删除对 `siligen_process_runtime_core_planning` 的默认依赖
- `modules/process-path/domain/trajectory/CMakeLists.txt`
  - 删除 `add_library(domain_trajectory ...)`
  - 用 `SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR` 与 `SILIGEN_SHARED_COMPAT_INCLUDE_ROOT` 替代 `${PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS}`
- 物理删除 `modules/process-path/src/trajectory/**`
- 物理删除 `modules/process-path/src/process/**`
- `modules/process-path/src` 当前仅剩 `README.md`

### 3. dispense-packaging canonical target 固化与 shell-only closeout

本阶段为 `modules/dispense-packaging/domain/dispensing` 建立独立 canonical target：

- `siligen_dispense_packaging_domain_dispensing`

并同步完成：

- `modules/dispense-packaging/CMakeLists.txt`
  - 模块根 target 改为直接链接 `siligen_dispense_packaging_domain_dispensing`
  - 删除对 `siligen_process_runtime_core_dispensing` 的默认依赖
- `modules/dispense-packaging/domain/dispensing/CMakeLists.txt`
  - 删除 `add_library(domain_dispensing ...)`
  - 直接链接 `siligen_process_path_domain_trajectory`
  - 保留对 `siligen_motion` 的运行期兼容依赖，不在本阶段切入 `M7` hard closeout
- 物理删除 `modules/dispense-packaging/src/domain-dispensing/**`
- 物理删除 `modules/dispense-packaging/src/dispensing/**`
- `modules/dispense-packaging/src` 当前仅剩 `README.md`

### 4. workflow interface forwarder 改造

保留以下兼容 interface target 名称不变：

- `siligen_process_runtime_core_planning`
- `siligen_process_runtime_core_dispensing`

但其真实转发面改为：

- `siligen_process_runtime_core_planning`
  - `siligen_process_path_domain_trajectory`
  - `siligen_motion`
- `siligen_process_runtime_core_dispensing`
  - `siligen_dispense_packaging_domain_dispensing`
  - `siligen_triggering`
  - `siligen_dispensing_execution_services`
  - `siligen_valve_core`

同时：

- `modules/workflow/domain/CMakeLists.txt`
- `modules/workflow/application/CMakeLists.txt`
- `modules/workflow/application/usecases-bridge/CMakeLists.txt`
- `modules/workflow/tests/process-runtime-core/CMakeLists.txt`

已不再把 `domain_trajectory`、`domain_dispensing` 作为 planning / dispensing 默认 owner target 使用。

### 5. motion-planning residual freeze

本阶段不宣称 `motion-planning` 已完成 shell-only closeout。

当前 residual runtime/control surface 冻结为：

- `HomingProcess*`
- `JogController*`
- `MotionBufferController*`
- `MotionControlService*`
- `MotionStatusService*`
- `IAxisControlPort.h`
- `IPositionControlPort.h`
- `IJogControlPort.h`
- `IHomingPort.h`
- `IMotionConnectionPort.h`
- `IMotionRuntimePort.h`
- `IMotionStatePort.h`
- `IIOControlPort.h`
- `IAdvancedMotionPort.h`

当前保留为后续 canonical owner 候选面的运动规划资产包括：

- `TriggerCalculator*`
- `TimeTrajectoryPlanner*`
- `TrajectoryPlanner*`
- `SpeedPlanner*`
- `VelocityProfileService*`
- `MotionValidationService*`
- `interpolation/*`
- `MotionTrajectory.h`
- `MotionTypes.h`
- `SemanticPath.h`
- `TimePlanningConfig.h`
- `TrajectoryAnalysisTypes.h`
- `TrajectoryTypes.h`

因此，本阶段后 `motion-planning` 的正确口径为：

- `bridge-backed; residual runtime/control surface frozen`

## validator 与状态文档更新

本阶段同步增强：

- `scripts/migration/validate_workspace_layout.py`
  - 新增 `Phase 13` strict shell bridge contents 断言
  - 新增 `process-path` / `dispense-packaging` canonical target required / forbidden snippets 断言
  - 新增 `workflow` planning / dispensing forwarder required / forbidden snippets 断言
  - 新增 `motion-planning` residual freeze 文档断言
- `specs/refactor/arch/NOISSUE-module-skeleton-alignment/README.md`
- `specs/refactor/arch/NOISSUE-module-skeleton-alignment/module-checklist.md`
- `specs/refactor/arch/NOISSUE-module-skeleton-alignment/wave-mapping.md`

## 验证记录

已完成以下验证：

- `python scripts/migration/validate_workspace_layout.py --wave "Wave 13"`
  - 通过
- `.\build.ps1 -Profile Local -Suite contracts`
  - 通过
- `cmake --build C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build --config Debug --target siligen_process_planning_domain_configuration siligen_process_path_domain_trajectory siligen_dispense_packaging_domain_dispensing siligen_planner_cli siligen_runtime_host_unit_tests siligen_unit_tests`
  - 通过
- `ctest --test-dir C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build -C Debug -R "siligen_runtime_host_unit_tests|siligen_unit_tests|process_runtime_core_deterministic_path_execution_test" --output-on-failure`
  - 当前已注册并匹配的 `siligen_unit_tests` 与 `siligen_runtime_host_unit_tests` 通过
- `C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build\bin\Debug\process_runtime_core_deterministic_path_execution_test.exe`
  - 直接执行通过

当前构建目录中的 `process_runtime_core_deterministic_path_execution_test` 已成功生成并可执行，但尚未出现在现有 `ctest -N` 测试清单中；本阶段将其记录为验证登记差异，而不是 Phase 13 的功能阻塞项。

## 关闭口径

本阶段完成后，模块级正确口径为：

- `runtime-execution` 已 `canonical-only implementation; shell-only bridges`
- `workflow` 已 `canonical-only implementation; shell-only bridges`
- `trace-diagnostics` 已 `canonical-only implementation; shell-only bridges`
- `topology-feature` 已 `canonical-only implementation; shell-only bridges`
- `coordinate-alignment` 已 `canonical-only implementation; shell-only bridges`
- `process-planning` 已 `canonical-only implementation; shell-only bridges`
- `process-path` 已 `canonical-only implementation; shell-only bridges`
- `dispense-packaging` 已 `canonical-only implementation; shell-only bridges`
- `motion-planning` 仍为 `bridge-backed; residual runtime/control surface frozen`

系统整体仍不得表述为“所有模块都已完成 hard closeout”。
