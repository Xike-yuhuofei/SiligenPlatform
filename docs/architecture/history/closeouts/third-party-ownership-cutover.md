# Third-Party Ownership Cutover

更新时间：`2026-03-19`

## 1. 目标

本文件只回答一件事：

`control-core/third_party` 里的依赖，哪些已经不再被 shared/device 层真实消费，哪些仍由谁在用，以及它们怎样迁到 canonical owner 而不制造多份 vendor 漂移。

边界保持：

- 不新增对 `control-core/third_party` 的 include/link
- 不为了“看起来独立”复制一份 Boost / Protobuf / json vendor
- 现场 SDK 与代码 owner 分开处理，不能把“代码已切”与“现场验收已做”混成一句话

## 2. 本轮已完成的 third-party owner 切换

### 2.1 `packages/shared-kernel`

已完成：

- `packages/shared-kernel/CMakeLists.txt`
  - 不再回落到 `control-core/third_party`
- `packages/shared-kernel/include/siligen/shared/point2d.h`
  - 已移除 `boost::geometry`
- `packages/shared-kernel/src/strings/string_manipulator.cpp`
  - 已移除 `boost/algorithm/string`

结论：

- `shared-kernel` 已不再依赖 `control-core/third_party`

### 2.2 `packages/device-contracts`

已完成：

- `packages/device-contracts` 只链接 `siligen_shared_kernel`
- 未发现 `third_party`、`modules/device-hal`、实现包依赖

结论：

- `device-contracts` 已不再依赖 `control-core/third_party`

### 2.3 `packages/device-adapters`

已完成：

- `packages/device-adapters/CMakeLists.txt`
  - 不再回落到 `control-core/third_party`
- `src/drivers/multicard/MockMultiCard.cpp`
  - 已移除 `boost::geometry`
- `siligen_device_adapters`
  - 现代 target 只正向依赖 `device-contracts` 与 `shared-kernel`

结论：

- modern `device-adapters` 已不再依赖 `control-core/third_party`

### 2.4 现场 SDK：MultiCard

已完成的代码 owner 切换：

- canonical binary owner：`packages/device-adapters/vendor/multicard`

当前状态：

- `MultiCard.lib` / `MultiCard.dll` 的代码 owner 已从 `control-core` 侧迁出
- 但现场联机 / HIL / 产线验收尚未完成

这部分必须拆开表述：

- 代码 owner：已切换
- 现场验收：未完成

## 3. 当前仍阻止删除 `control-core/third_party` 的依赖矩阵

| 依赖 | 当前 live 使用方 | 当前临时 owner | canonical owner / 切换方式 | 退出条件 |
|---|---|---|---|---|
| `boost` | `packages/process-runtime-core/src`、`control-core/src/shared`、`control-core/src/infrastructure/adapters/planning/spatial` | `control-core/third_party/boost` | 不新增副本。真正消费方改为显式依赖 workspace 预注册的 Boost headers target，或直接移除 Boost 用法 | `process-runtime-core` 与 shared compat 不再 include `boost/describe`、`boost::geometry`；`siligen_device_adapters_legacy_bridges` standalone 可通过 |
| `protobuf` | `packages/process-runtime-core/CMakeLists.txt`、`control-core/src/infrastructure/adapters/planning/dxf/CMakeLists.txt` | `control-core/third_party/protobuf` | 不复制 vendor。由实际消费者改为链接上层预注册的 `protobuf` targets | `process-runtime-core` 与 DXF planning 均不再从 `control-core/third_party/protobuf` add_subdirectory |
| `spdlog` | `control-core/src/infrastructure/CMakeLists.txt`、`control-core/src/shared/CMakeLists.txt` | `control-core/third_party/spdlog` | logging 迁到独立 canonical package 或 workspace 预注册 target；停止手写 include path | `siligen_spdlog_adapter` 不再编译 `modules/device-hal` logging 源；shared/infrastructure 不再 include `${CMAKE_SOURCE_DIR}/third_party/spdlog/include` |
| `nlohmann/json` | `packages/transport-gateway/src/tcp/**`、`packages/process-runtime-core/src/domain/**`、`control-core/src/infrastructure/CMakeLists.txt` | 当前实际仍靠 `control-core` 侧 include root 暴露 | 不复制 vendor。由 `transport-gateway`、`process-runtime-core`、recipe/serialization canonical owner 显式链接 imported target | `transport-gateway` 与 serialization 相关目标不再从 `control-core` 继承 json include root |

## 4. 当前已精确确认的 blocker

### 4.1 `boost`

已确认的 live 使用点包括：

- `packages/process-runtime-core/src/domain/diagnostics/value-objects/DiagnosticTypes.h`
- `packages/process-runtime-core/src/domain/dispensing/value-objects/DispensingPlan.h`
- `packages/process-runtime-core/src/domain/motion/value-objects/HardwareTestTypes.h`
- `packages/process-runtime-core/src/domain/motion/value-objects/MotionTypes.h`
- `packages/process-runtime-core/src/domain/motion/value-objects/TrajectoryTypes.h`
- `packages/process-runtime-core/src/domain/geometry/GeometryTypes.h`
- `packages/process-runtime-core/src/domain/trajectory/value-objects/GeometryBoostAdapter.h`
- `control-core/src/shared/types/Point2D.h`
- `control-core/src/shared/types/OptimizedTypes.h`
- `control-core/src/shared/types/DispensingStrategy.h`
- `control-core/src/shared/geometry/BoostGeometryAdapter.h`

这正是当前 `siligen_device_adapters_legacy_bridges` standalone 仍然失败的直接原因。

### 4.2 `protobuf`

`packages/process-runtime-core/CMakeLists.txt` 当前仍显式从：

- `${SILIGEN_THIRD_PARTY_DIR}/protobuf`

引入依赖。

这说明：

- `process-runtime-core` 仍把部分 `control-core/third_party` 资产当作临时 vendor owner
- 若不先把依赖注册方式改成 package-consumer-owned，`control-core/third_party` 不能删

### 4.3 `spdlog`

`control-core/src/infrastructure/CMakeLists.txt` 当前仍直接：

- 编译 `modules/device-hal/src/adapters/diagnostics/logging/SpdlogLoggingAdapter.cpp`
- include `${CMAKE_SOURCE_DIR}/third_party/spdlog/include`

`control-core/src/shared/CMakeLists.txt` 当前也仍引用：

- `${SILIGEN_SHARED_LAYER_THIRD_PARTY_DIR}/spdlog/include`

### 4.4 `nlohmann/json`

当前仍有两类真实暴露：

1. `control-core/src/infrastructure/CMakeLists.txt`
   - 仍把 `${CMAKE_SOURCE_DIR}/third_party` 作为 json include root
2. `packages/transport-gateway/src/tcp/TcpCommandDispatcher.*`
   - 真实使用 `nlohmann::json`

同时，`packages/transport-gateway/src/tcp/TcpCommandDispatcher.cpp:15` 还直接 include：

- `modules/device-hal/src/adapters/recipes/RecipeJsonSerializer.h`

所以 json owner 切换不能单独做，必须与 recipe serializer owner 一起切。

## 5. 切换顺序建议

### 5.1 第一优先级：先把 shared/device 层彻底留空

目标：

- `shared-kernel`
- `device-contracts`
- `device-adapters` modern target

现状：

- 已完成，不要再给它们新增任何 `control-core/third_party` include/link

### 5.2 第二优先级：清掉 device legacy bridge 的上游 Boost 阻塞

目标：

- `packages/process-runtime-core`
- `control-core/src/shared`

动作：

- 优先移除 `boost::geometry` / `boost::describe` 的直接使用
- 无法立即移除时，也必须改为 package consumer 显式依赖，而不是让 `device-adapters` 借道 `control-core/third_party`

### 5.3 第三优先级：拆出 logging / recipe 的 canonical owner

目标：

- diagnostics logging
- recipes persistence / serializer

动作：

- 先迁 owner，再切 include/link，再删 `modules/device-hal/src/adapters/diagnostics/logging`
- 先迁 owner，再切 include/link，再删 `modules/device-hal/src/adapters/recipes`

### 5.4 第四优先级：收口 `protobuf` / `json`

目标：

- `packages/process-runtime-core`
- `packages/transport-gateway`
- DXF planning adapter

动作：

- 由真正消费者声明依赖
- 由上层 superbuild / package manager / 预注册 target 提供 vendor materialization
- 不复制 vendor 目录，不新增第二份第三方源码

## 6. 风险

1. 如果把 `boost`、`protobuf` 直接复制到多个 package，会制造多份 vendor 漂移，后续无法收敛。
2. 如果把 recipes 或 logging 继续塞进 `device-adapters`，会破坏“设备层只承接设备实现”的边界。
3. 如果把现场 SDK 验收与代码 owner 迁移混写，会导致删除门禁判断失真。
## 7. 验证结果

### 7.1 已通过

- `cmake -S control-core -B build/shared-device-cutover -G Ninja -DSILIGEN_BUILD_TESTS=ON -DSILIGEN_FETCH_GTEST=ON -DSILIGEN_USE_PCH=OFF -DSILIGEN_PARALLEL_COMPILE=OFF`
- `cmake --build build/shared-device-cutover --target siligen_shared_kernel siligen_shared_kernel_tests siligen_device_adapters --parallel 8`
- `ctest --test-dir build/shared-device-cutover --output-on-failure -R siligen_shared_kernel_tests -C Debug`
- `cmake -S packages/device-adapters -B build/device-adapters-standalone -G Ninja -DSILIGEN_ENABLE_DEVICE_ADAPTERS_LEGACY_BRIDGES=OFF`
- `cmake --build build/device-adapters-standalone --target siligen_device_adapters --parallel 8`
- `cmake -S packages/shared-kernel -B build/shared-kernel-standalone -G Ninja`
- `cmake --build build/shared-kernel-standalone --target siligen_shared_kernel --parallel 8`
- `powershell -ExecutionPolicy Bypass -File .\legacy-exit-check.ps1 -Profile Local -ReportDir integration\reports\legacy-exit\shared-device-cutover`

结论：

- `shared-kernel`、`device-contracts`、modern `device-adapters` 已真实脱离 `control-core/third_party`
- legacy exit checks 通过

### 7.2 未通过但已定位原因

失败命令：

- `cmake -S packages/device-adapters -B build/device-adapters-legacy-standalone -G Ninja -DSILIGEN_ENABLE_DEVICE_ADAPTERS_LEGACY_BRIDGES=ON -DSILIGEN_PROCESS_RUNTIME_CORE_DIR=D:/Projects/SiligenSuite/packages/process-runtime-core -DSILIGEN_SHARED_COMPAT_INCLUDE_ROOT=D:/Projects/SiligenSuite/control-core/src`
- `cmake --build build/device-adapters-legacy-standalone --target siligen_device_adapters_legacy_bridges --parallel 8`

失败原因：

- `process-runtime-core/src` 与 `control-core/src/shared` 的 Boost include 仍在
- 失败点已经不是 `modules/device-hal`，而是 `third_party` owner 还未完全切走

另一个已知阻塞：

- control-core 大图继续向前验证时，会先遇到既有 `siligen_types` include-root 问题：
  - `shared/errors/ErrorHandler.h` not found
- 这是既有全图问题，不是本轮 shared/device cutover 新引入

