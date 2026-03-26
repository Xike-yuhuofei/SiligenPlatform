# Phase 15: B3 validator and test registration hardening

更新时间：`2026-03-25`

## 1. 目标

将 validator 与仓库级/模块级 test registration 规则同步到 `workflow` 与 `runtime-execution` 新增的 `integration/regression` 承载面，并把高风险禁桥约束收口为显式 gate。

## 2. 已完成变更

### 2.1 仓库级 tests 根注册加固

- `tests/CMakeLists.txt`
  - 对 canonical tests roots 改为先校验 `CMakeLists.txt` 必须存在，再统一注册。
  - `workflow/tests` 与 `runtime-execution/tests` 不再以“存在则加载、缺失则静默跳过”的方式处理。

### 2.2 workflow 测试根注册加固

- `modules/workflow/tests/CMakeLists.txt`
  - 为 `process-runtime-core`、`integration`、`regression` 三个正式承载面增加 required gate。
  - 缺失任一子根时直接配置失败，防止新承载面未被 CMake 看见仍误判通过。

### 2.3 runtime-execution 测试根注册加固

- `modules/runtime-execution/tests/CMakeLists.txt`
  - 为 `runtime-host`、`integration`、`regression` 三个模块级测试入口增加 required gate。
  - binary dir 命名改为 `tests/runtime-execution-*` 口径，便于稳定复验。
- `modules/runtime-execution/runtime/host/tests/CMakeLists.txt`
  - `siligen_runtime_host_unit_tests` 之外，正式把 canonical `integration` / `regression` 子根纳入 host tests root。
  - `runtime_execution_integration_host_bootstrap_smoke` 与 `runtime_execution_regression_tests` 现在既可从模块级 tests root 看到，也可从 canonical `runtime/host/tests` root 看到。

### 2.4 validator 新增断言

- `scripts/migration/validate_workspace_layout.py`
  - 新增 `Phase 15` required/forbidden snippet 校验，确保：
    - 仓库级 `tests/CMakeLists.txt` 正式登记 `modules/workflow/tests` 与 `modules/runtime-execution/tests`
    - `workflow/tests` 正式登记 `integration` / `regression`
    - `runtime-execution/tests` 与 `runtime/host/tests` 正式登记 `integration` / `regression`
    - 仓库级 tests root 不回退到直接注册 `modules/runtime-execution/runtime/host/tests`
  - 新增 `runtime-execution` canonical public headers 扫描断言：
    - public headers 一旦继续命中 `src/legacy`，validator 直接失败

## 3. ctest -N 登记结果

本轮配置后，`ctest --test-dir C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build -C Debug -N` 可见：

- `siligen_shared_kernel_tests`
- `siligen_unit_tests`
- `siligen_pr1_tests`
- `process_runtime_core_deterministic_path_execution_test`
- `process_runtime_core_motion_runtime_assembly_test`
- `process_runtime_core_pb_path_source_adapter_test`
- `workflow_integration_motion_runtime_assembly_smoke`
- `workflow_regression_deterministic_path_execution_smoke`
- `siligen_runtime_host_unit_tests`
- `runtime_execution_integration_host_bootstrap_smoke`

当前 `runtime_execution_regression_tests` 仍为聚合 target，不直接作为 `ctest` 用例出现，这符合现阶段设计。

## 4. validator 当前暴露的真实残留

新增 gate 后，`python scripts/migration/validate_workspace_layout.py` 当前会因以下 10 个 public headers 仍引用 `src/legacy` 而失败：

- `modules/runtime-execution/adapters/device/include/siligen/device/adapters/dispensing/TriggerControllerAdapter.h`
- `modules/runtime-execution/adapters/device/include/siligen/device/adapters/dispensing/ValveAdapter.h`
- `modules/runtime-execution/adapters/device/include/siligen/device/adapters/hardware/HardwareConnectionAdapter.h`
- `modules/runtime-execution/adapters/device/include/siligen/device/adapters/hardware/HardwareTestAdapter.h`
- `modules/runtime-execution/adapters/device/include/siligen/device/adapters/motion/HomingPortAdapter.h`
- `modules/runtime-execution/adapters/device/include/siligen/device/adapters/motion/HomingSupport.h`
- `modules/runtime-execution/adapters/device/include/siligen/device/adapters/motion/InterpolationAdapter.h`
- `modules/runtime-execution/adapters/device/include/siligen/device/adapters/motion/MotionRuntimeConnectionAdapter.h`
- `modules/runtime-execution/adapters/device/include/siligen/device/adapters/motion/MotionRuntimeFacade.h`
- `modules/runtime-execution/adapters/device/include/siligen/device/adapters/motion/MultiCardMotionAdapter.h`

这不是 B3 新引入的问题，而是 B3 把原本隐性的 owner 化残留转成了显式 fail-fast gate。该缺口需要 `A4` 继续处理。

## 5. 本轮验证结论

- `cmake -S ... -B ...`
  - 通过
- `ctest -N`
  - 通过，且新增承载面已稳定登记
- `ctest -R "workflow_(integration_motion_runtime_assembly_smoke|regression_deterministic_path_execution_smoke)|runtime_execution_integration_host_bootstrap_smoke"`
  - 通过
- 定向构建修改过的测试根
  - 当前被工作区现有编译错误阻断：`modules/process-path/domain/trajectory/domain-services/MotionPlanner.h` 缺少 `domain/motion/value-objects/MotionTrajectory.h`
  - 该错误发生在 `siligen_process_path_domain_trajectory`，不属于本任务允许修改范围

## 6. 残留缺口

- `runtime-execution` public headers 仍回指 `src/legacy`，导致新增 validator gate 当前为红
- 工作区存在与本任务无关的 `process-path` 编译错误，影响对修改后测试根做完整定向构建复验
- `runtime_execution_regression_tests` 目前仍只有聚合 target，尚未落地真实 regression 用例
