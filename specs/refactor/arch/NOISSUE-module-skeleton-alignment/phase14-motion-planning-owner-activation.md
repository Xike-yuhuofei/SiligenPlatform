# Phase 14: motion-planning owner activation and test registration hardening

## 1. 背景

`Phase 13` 之后，`motion-planning` 的正确口径仍然是：

- `bridge-backed; residual runtime/control surface frozen`

但这并不等于 `M7` 的 canonical owner target 已被主消费链和验证链稳定激活。

本阶段解决的是两个工程性残留：

1. `workflow` / `dispense-packaging` / 测试链路对 `motion-planning` canonical owner target 的依赖仍未完全显式化。
2. `workflow` canonical tests 在 VS 生成器下存在 target 传播与测试登记脆弱点，导致 owner 激活后出现裸库名链接错误与辅助测试登记不稳。

## 2. 本阶段目标

1. 激活 `modules/motion-planning` 作为 `M7` 的 canonical build owner，而不是仅停留在目录骨架与 metadata 就绪。
2. 保持 `motion-planning` 的 residual runtime/control surface 继续按 frozen 范围管理，不宣称 shell-only closeout。
3. 固化 `workflow` canonical tests 的构建入口与测试登记，使 `process-runtime-core` 三个辅助测试成为稳定登记面。

## 3. 已完成变更

### 3.1 motion-planning owner target 激活

- `modules/motion-planning/CMakeLists.txt`
  - 显式要求 canonical `domain/motion` target 为 `siligen_motion`
  - `siligen_module_motion_planning` 直接链接 `siligen_motion`
- `modules/motion-planning/domain/motion/CMakeLists.txt`
  - canonical `domain/motion` 已作为真实 owner target 承载纯 `M7` 资产

### 3.2 workflow 对 M7 owner target 的消费链稳定化

- `modules/workflow/CMakeLists.txt`
  - 显式暴露 `SILIGEN_MOTION_PLANNING_PUBLIC_INCLUDE_DIR`
- `modules/workflow/domain/CMakeLists.txt`
  - 在链接 `siligen_domain` 之前，先加载 `modules/workflow/domain/domain` supporting targets
  - 避免 `siligen_triggering`、`siligen_configuration`、`siligen_motion_execution_services` 等被 CMake 当作裸库名
- `modules/workflow/process-runtime-core/CMakeLists.txt`
  - bridge-domain fallback 的判断条件从 `siligen_motion` 改为 `siligen_domain_services`
  - 使 fallback 语义对 supporting targets 缺失更准确
- `modules/dispense-packaging/domain/dispensing/CMakeLists.txt`
  - 继续经 `modules/motion-planning` public root 与 `modules/process-path` canonical target 消费 M7/M6 owner surface

### 3.3 workflow canonical test surface 加固

- `modules/workflow/tests/CMakeLists.txt`
  - 将 canonical tests 的 binary dir 固定到 `build/tests/process-runtime-core-canonical`
  - 避免 VS 生成器在更深层目录下生成错误的相对库路径
- `modules/workflow/tests/process-runtime-core/CMakeLists.txt`
  - 保持 `siligen_unit_tests`、`siligen_pr1_tests` 与以下辅助测试在 canonical tests root 下登记：
    - `process_runtime_core_deterministic_path_execution_test`
    - `process_runtime_core_motion_runtime_assembly_test`
    - `process_runtime_core_pb_path_source_adapter_test`
- `modules/workflow/tests/process-runtime-core/unit/infrastructure/adapters/planning/dxf/PbPathSourceAdapterContractTest.cpp`
  - contract test 改为同时接受 protobuf-on / protobuf-off 两种合法返回码
  - 避免把当前工作区的 `SILIGEN_ENABLE_PROTOBUF=ON` 误判为回归

## 4. 验证结果

### 4.1 定向构建

以下目标已通过 `Debug` 定向构建：

- `siligen_unit_tests`
- `process_runtime_core_deterministic_path_execution_test`
- `process_runtime_core_motion_runtime_assembly_test`
- `process_runtime_core_pb_path_source_adapter_test`
- `siligen_runtime_host_unit_tests`

### 4.2 测试登记

`ctest --test-dir <build> -C Debug -N` 当前登记结果包含：

- `siligen_unit_tests`
- `siligen_pr1_tests`
- `process_runtime_core_deterministic_path_execution_test`
- `process_runtime_core_motion_runtime_assembly_test`
- `process_runtime_core_pb_path_source_adapter_test`
- `siligen_runtime_host_unit_tests`

### 4.3 定向执行

以下测试已实际执行并通过：

- `process_runtime_core_deterministic_path_execution_test`
- `process_runtime_core_motion_runtime_assembly_test`
- `process_runtime_core_pb_path_source_adapter_test`
- `siligen_runtime_host_unit_tests`

## 5. 当前口径

本阶段完成后，`motion-planning` 的正确口径更新为：

- `bridge-backed`
- `canonical owner target active`
- `residual runtime/control surface frozen`

本阶段不宣称：

- `motion-planning` 已达到 shell-only closeout
- `motion-planning/src` 已物理排空
- `workflow` / `runtime-execution` 已进入系统级 hard closeout

## 6. 后续建议

1. 继续推进 `motion-planning` residual runtime/control surface drain，缩小 `src` 保留面。
2. 在 `workflow` 中继续把 wrapper-backed public headers 收敛到真正 owner-backed 实现，而不是长期回指兼容层。
3. 在 `runtime-execution` 与 `workflow` 的后续 hard closeout 中，保留本阶段新增的 tests/validator 断言，防止 owner target 回退。
