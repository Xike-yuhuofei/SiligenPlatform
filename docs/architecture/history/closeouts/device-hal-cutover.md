# Device HAL Cutover

更新时间：`2026-03-19`

## 1. 结论

本轮已经把 `modules/device-hal` 里“真正属于设备层”的 owner 大面切到 package：

- `packages/device-contracts`：纯契约 owner
- `packages/device-adapters`：真实设备适配器 / fake / mock / wrapper owner

但 `control-core/modules/device-hal` 还不能整体删除，因为仍残留两块不应继续留在设备层的真实实现：

- diagnostics logging
- recipes persistence / serializer

这两块当前仍由 `control-core/src/infrastructure/CMakeLists.txt` 直接编译 legacy 源文件。

## 2. 真实 owner 切换状态

### 2.1 `packages/device-contracts`：已切为纯契约 owner

当前 contract surface 已落在：

- `include/siligen/device/contracts/commands/device_commands.h`
- `include/siligen/device/contracts/state/device_state.h`
- `include/siligen/device/contracts/capabilities/device_capabilities.h`
- `include/siligen/device/contracts/faults/device_faults.h`
- `include/siligen/device/contracts/events/device_events.h`
- `include/siligen/device/contracts/ports/device_ports.h`

当前边界满足：

- `device-contracts` 只链接 `siligen_shared_kernel`
- 只公开自己的 `include/`
- 未发现 `modules/device-hal`、`control-core/third_party`、实现包的 include 或 link

因此，设备公开契约的真实 owner 已经不再是 `modules/device-hal`。

### 2.2 `packages/device-adapters`：已切为真实设备适配器 owner

当前 `siligen_device_adapters` 已承接：

- multicard wrapper / mock / stub
- fake motion / fake dispenser
- 现代公开适配器头 `include/siligen/device/adapters/**`

关键边界：

- `siligen_device_adapters` 只公开 `include/`
- `siligen_device_adapters` 只正向依赖：
  - `siligen_device_contracts`
  - `siligen_shared_kernel`
- 不得反向依赖核心业务实现；当前 package target 未向 `process-runtime-core` 或 `control-core/src/domain` 反向公开 include 面

### 2.3 `siligen_device_adapters_legacy_bridges`：已把 legacy include owner 收到 package 内

为兼容 runtime-host / control-runtime 现有装配代码，本轮新增了 package-owned bridge：

- target：`siligen_device_adapters_legacy_bridges`
- 开关：`SILIGEN_ENABLE_DEVICE_ADAPTERS_LEGACY_BRIDGES`

它当前承接的仅是过渡责任：

- `src/legacy/adapters/diagnostics/health/testing/**`
- `src/legacy/adapters/motion/controller/**`
- `src/legacy/adapters/dispensing/dispenser/**`
- `src/legacy/drivers/multicard/**`

这一步的意义是：

- runtime-host/control-runtime 已不必继续从 `modules/device-hal/src/**` 取 motion、dispensing、health-testing、multicard 的真实源码
- legacy include root 现在由 package 管控，而不是 `control-core/modules/device-hal`

## 3. CMake / include / link 的实际切换

### 3.1 已完成的切换

- `packages/device-adapters/CMakeLists.txt`
  - 已删除对 `control-core/third_party` 的 fallback
  - 已拆成：
    - `siligen_device_adapters`
    - `siligen_device_adapters_legacy_bridges`
- `packages/shared-kernel/CMakeLists.txt`
  - 已删除对 `control-core/third_party` 的 fallback
- `control-core/src/infrastructure/CMakeLists.txt`
  - `siligen_device_hal_hardware` 已通过 link 接入：
    - `siligen_device_adapters`
    - `siligen_device_adapters_legacy_bridges`（存在时）
    - `siligen_device_contracts`

### 3.2 已完成的 include 切换

`packages/runtime-host/src/bootstrap/InfrastructureBindingsBuilder.cpp` 已从 `modules/device-hal/src/**` 切走以下真实设备面：

- `HardwareTestAdapter.h`
- `ValveAdapter.h`
- `TriggerControllerAdapter.h`
- `InterpolationAdapter.h`
- `MultiCardMotionAdapter.h`
- `HomingPortAdapter.h`
- `MotionRuntimeConnectionAdapter.h`
- `MotionRuntimeFacade.h`
- `IMultiCardWrapper.h`
- `MockMultiCardWrapper.h`
- `RealMultiCardWrapper.h`

对应的新 include 已改为：

- `legacy/adapters/**`
- `siligen/device/adapters/drivers/multicard/**`

### 3.3 已建立的回流阻断

`tools/scripts/legacy_exit_checks.py` 已新增：

- `device-hal-mirrored-source-include-blocked`

它会阻止已迁出的 motion/controller、dispensing/dispenser、diagnostics health testing、多卡 wrapper 再次回流为：

- `#include "modules/device-hal/src/..."`

## 4. 已完成切换的依赖面

### 4.1 已完成

以下设备实现 owner 已切到 package：

- diagnostics/health/testing
- motion/controller/*
- dispensing/dispenser/*
- multicard wrapper / mock / stub / fake

### 4.2 仍在 legacy 目录的真实残面

以下内容还是真实 blocker，不能被模糊地说成“设备层未完成”：

1. diagnostics logging
   - `control-core/modules/device-hal/src/adapters/diagnostics/logging/SpdlogLoggingAdapter.cpp`
   - 当前仍由 `control-core/src/infrastructure/CMakeLists.txt` 编译为 `siligen_spdlog_adapter`
   - 这不是设备驱动，后续应迁到 logging/observability canonical owner
2. recipes persistence / serializer
   - `RecipeJsonSerializer.cpp`
   - `RecipeFileRepository.cpp`
   - `TemplateFileRepository.cpp`
   - `AuditFileRepository.cpp`
   - `ParameterSchemaFileProvider.cpp`
   - `RecipeBundleSerializer.cpp`
   - 当前仍由 `control-core/src/infrastructure/CMakeLists.txt` 编译为 `siligen_device_hal_persistence`
   - 这不是设备适配器，后续应迁到 runtime/storage 或独立 recipe persistence canonical owner

### 4.3 仍引用 legacy `modules/device-hal` 的 live consumer

1. `packages/runtime-host/src/bootstrap/InfrastructureBindingsBuilder.cpp`
   - 仍直接 include：
     - `modules/device-hal/src/adapters/diagnostics/logging/SpdlogLoggingAdapter.h`
     - `modules/device-hal/src/adapters/recipes/RecipeFileRepository.h`
     - `modules/device-hal/src/adapters/recipes/TemplateFileRepository.h`
     - `modules/device-hal/src/adapters/recipes/AuditFileRepository.h`
     - `modules/device-hal/src/adapters/recipes/ParameterSchemaFileProvider.h`
     - `modules/device-hal/src/adapters/recipes/RecipeBundleSerializer.h`
2. `packages/transport-gateway/src/tcp/TcpCommandDispatcher.cpp`
   - 仍直接 include：
     - `modules/device-hal/src/adapters/recipes/RecipeJsonSerializer.h`

## 5. 删除 `control-core/modules/device-hal` 前还必须完成的事

### 5.1 diagnostics/logging owner 切换

建议 canonical owner：

- `packages/traceability-observability` 或等价的 logging package

最低退出条件：

- `SpdlogLoggingAdapter.*` 不再位于 `modules/device-hal/src/adapters/diagnostics/logging`
- `control-core/src/infrastructure/CMakeLists.txt` 不再直接编译该 legacy 路径
- runtime-host include 改到 canonical package 头

### 5.2 recipes owner 切换

建议 canonical owner：

- `packages/runtime-host` 下的 runtime/storage/recipes，或单独的 recipe persistence package

最低退出条件：

- `Recipe*` / `Template*` / `Audit*` / `ParameterSchema*` / `RecipeBundleSerializer*` 不再位于 `modules/device-hal/src/adapters/recipes`
- `InfrastructureBindingsBuilder.cpp` 与 `TcpCommandDispatcher.cpp` 改为 include canonical package 头
- `control-core/src/infrastructure/CMakeLists.txt` 不再直接编译上述 legacy 源文件

## 6. 风险

1. `device-adapters` 的现代 target 已不依赖 `control-core/third_party`，但 `legacy_bridges` 仍会通过 `process-runtime-core/src` 和 shared compat root 遇到上游 Boost 依赖。
2. `device-contracts` 当前保持纯契约边界；后续新增能力不能把 recipe、logging、runtime 配置等实现性内容顺手塞回 contract 包。
3. MultiCard 现场 SDK 的代码 owner 已切到 `packages/device-adapters/vendor/multicard`，但现场联机/HIL/产线验收尚未完成；这里只能标记为“代码 owner 已切换，现场验收未完成”。

## 7. 验证

### 7.1 通过

- `cmake -S control-core -B build/shared-device-cutover -G Ninja -DSILIGEN_BUILD_TESTS=ON -DSILIGEN_FETCH_GTEST=ON -DSILIGEN_USE_PCH=OFF -DSILIGEN_PARALLEL_COMPILE=OFF`
- `cmake --build build/shared-device-cutover --target siligen_shared_kernel siligen_shared_kernel_tests siligen_device_adapters --parallel 8`
- `cmake -S packages/device-adapters -B build/device-adapters-standalone -G Ninja -DSILIGEN_ENABLE_DEVICE_ADAPTERS_LEGACY_BRIDGES=OFF`
- `cmake --build build/device-adapters-standalone --target siligen_device_adapters --parallel 8`
- `powershell -ExecutionPolicy Bypass -File .\legacy-exit-check.ps1 -Profile Local -ReportDir integration\reports\legacy-exit\shared-device-cutover`

结果：

- 现代 `siligen_device_adapters` 已可独立构建
- modern adapter surface 已真实脱离 `modules/device-hal`
- legacy exit checks 通过，未发现回流

### 7.2 已知失败与其含义

失败命令：

- `cmake -S packages/device-adapters -B build/device-adapters-legacy-standalone -G Ninja -DSILIGEN_ENABLE_DEVICE_ADAPTERS_LEGACY_BRIDGES=ON -DSILIGEN_PROCESS_RUNTIME_CORE_DIR=D:/Projects/SiligenSuite/packages/process-runtime-core -DSILIGEN_SHARED_COMPAT_INCLUDE_ROOT=D:/Projects/SiligenSuite/control-core/src`
- `cmake --build build/device-adapters-legacy-standalone --target siligen_device_adapters_legacy_bridges --parallel 8`

失败原因：

- 卡在 `packages/process-runtime-core/src` 与 `control-core/src/shared` 的 Boost 依赖，而不是再卡在 `modules/device-hal`

这说明：

- device-hal 的镜像源码 owner 切换已经推进到 package
- 当前真正阻止 legacy bridge 彻底 standalone 的 blocker 已转移为 third-party ownership 问题
