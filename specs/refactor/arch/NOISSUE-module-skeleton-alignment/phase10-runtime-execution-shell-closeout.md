# Phase 10: runtime-execution shell-only bridge closeout

更新时间：`2026-03-25`

## 目标

在保持 `workflow/process-runtime-core` 仍为内部 bridge-backed implementation 的前提下，将 `runtime-execution` 从：

- canonical-first build + residual bridge payload

推进到：

- canonical-only implementation + shell-only compatibility bridges

本阶段不追求删除 bridge 目录本身，而是要求 `device-contracts`、`device-adapters`、`runtime-host` 三个历史 bridge 根不再承载任何真实实现或公开头树。

## 实施内容

### 1. shell-only bridge 目标形态

本阶段完成后，以下 bridge 根只允许保留：

- `modules/runtime-execution/device-contracts`
  - `CMakeLists.txt`
  - `README.md`
- `modules/runtime-execution/device-adapters`
  - `CMakeLists.txt`
  - `README.md`
  - `vendor/multicard/README.md`
- `modules/runtime-execution/runtime-host`
  - `CMakeLists.txt`
  - `README.md`
  - `tests/CMakeLists.txt`

### 2. canonical CMake 去除 bridge vendor fallback

以下 canonical CMake 已移除默认 bridge vendor fallback：

- `modules/runtime-execution/adapters/device/CMakeLists.txt`
- `modules/runtime-execution/runtime/host/CMakeLists.txt`

保留策略：

- 仍允许显式外部覆盖 `SILIGEN_MULTICARD_VENDOR_DIR`
- 默认仅认 canonical vendor root：`modules/runtime-execution/adapters/device/vendor/multicard`

### 3. bridge payload 物理排空

本阶段已物理删除：

- `modules/runtime-execution/device-contracts/include/**`
- `modules/runtime-execution/device-adapters/include/**`
- `modules/runtime-execution/device-adapters/src/**`
- `modules/runtime-execution/runtime-host/src/**`

因此，`runtime-execution` 的真实实现与公开头承载面已只剩 canonical roots：

- `modules/runtime-execution/contracts/device`
- `modules/runtime-execution/adapters/device`
- `modules/runtime-execution/runtime/host`

### 4. 最小 workflow 配套清障

为避免 `runtime-execution` shell-only closeout 被上游测试与注释残留阻塞，本阶段同步完成：

- `modules/workflow/process-runtime-core/tests/CMakeLists.txt`
  - 删除对 `modules/runtime-execution/device-adapters/src`
  - 删除对 `modules/runtime-execution/device-adapters/include`
  - 增加 canonical public include root：`modules/runtime-execution/adapters/device/include`
- `modules/workflow/domain/include/domain/diagnostics/ports/ITestConfigurationPort.h`
  - 将实现说明从 `runtime-host/src/runtime/configuration` 更新到 canonical `runtime/host/runtime/configuration`

本阶段未迁移 `workflow/process-runtime-core` 内部实现，也不改变其当前 bridge-backed 结论。

## 验证结果

`2026-03-25` 已完成以下验证，结果均通过：

- `python scripts/migration/validate_workspace_layout.py --wave "Wave 10"`
  - `runtime_execution_bridge_shell_failure_count=0`
  - `runtime_execution_active_reference_failure_count=0`
- `.\build.ps1 -Profile Local -Suite contracts`
  - `contracts` 构建通过，`process-runtime-core` 测试侧不再因 `siligen/device/adapters/motion/HomingSupport.h` 缺少 include root 而失败
- `cmake --build C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build --config Debug --target siligen_device_adapters siligen_runtime_host siligen_planner_cli siligen_transport_gateway`
  - 关键 canonical targets 构建通过
- `ctest --test-dir C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build -C Debug -R "siligen_runtime_host_unit_tests|siligen_unit_tests|siligen_pr1_tests" --output-on-failure`
  - `3/3` tests passed

## 关闭口径

本阶段完成后，`runtime-execution` 的正确口径为：

- canonical build roots 是唯一真实实现面
- 历史 bridge roots 已降为 shell-only compatibility roots
- 不再保留 bridge include/source payload
- 不再保留默认 bridge vendor fallback

这意味着：

- `runtime-execution` 已达到模块级 hard-drain closeout
- `workflow` 仍未达到 hard closeout，仍保留 `process-runtime-core` 内部 bridge-backed implementation

因此，系统整体口径仍然不能表述为“所有高复杂模块均已物理清空 bridge”。
