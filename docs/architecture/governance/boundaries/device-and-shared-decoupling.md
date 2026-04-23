# Device And Shared Decoupling

> Historical migration note: 本文记录 `control-core` 时代的 legacy device/shared 拆分阻塞；下文出现的已退役历史管理路径与类型仅用于迁移追溯，不代表当前主线仍承载该能力。

更新时间：`2026-03-18`

## 1. 目标

本次收口的目标是切断以下 package 对 `control-core/modules/*` 的真实依赖：

- `packages/shared-kernel`
- `packages/device-contracts`
- `packages/device-adapters`

约束保持不变：

- 不把业务状态机塞进 `device-adapters`
- `shared-kernel` 只承接小而稳的共享基础
- `device-contracts` 不依赖任何实现目录

## 2. 已推进的真实承接

### 2.1 `packages/shared-kernel`

`packages/shared-kernel` 已承接以下真实代码：

- `include/siligen/shared/numeric_types.h`
- `include/siligen/shared/axis_types.h`
- `include/siligen/shared/basic_types.h`
- `include/siligen/shared/point2d.h`
- `include/siligen/shared/error.h`
- `include/siligen/shared/result.h`
- `include/siligen/shared/result_types.h`
- `include/siligen/shared/strings/string_manipulator.h`
- `src/strings/string_manipulator.cpp`

构建切换：

- 新增 `packages/shared-kernel/CMakeLists.txt`
- `control-core/modules/CMakeLists.txt` 现在优先注册 `packages/shared-kernel` 的 `siligen_shared_kernel`
- `control-core/modules/shared-kernel` 不再是默认 target owner

边界说明：

- 没有把 `process` / `motion` / `device` 的业务模型吸进 `shared-kernel`
- 只承接原 legacy `shared-kernel` 中稳定基础类型和字符串工具

### 2.2 `packages/device-contracts`

当前状态已经调整为“纯契约化”：

- `CMake` 不再直接 include `control-core/modules/shared-kernel/include`
- 只通过 `target_link_libraries(... siligen_shared_kernel)` 获取共享基础头
- 若独立接入 `device-contracts`，会先补注册 `packages/shared-kernel`

本次没有往 `device-contracts` 中加入任何实现代码，保持其 owner 仍然是：

- capability / command / state / event / fault 模型
- runtime-facing device ports

### 2.3 `packages/device-adapters`

当前仍由 `packages/device-adapters` 承接的真实设备实现包括：

- `drivers/multicard/error_mapper.*`
- `drivers/multicard/IMultiCardWrapper.h`
- `drivers/multicard/MockMultiCard*`
- `drivers/multicard/RealMultiCardWrapper.*`
- `drivers/multicard/MultiCardCPP.h`
- `motion/multicard_motion_device.*`
- `io/multicard_digital_io_adapter.*`
- `fake/fake_motion_device.*`
- `fake/fake_dispenser_device.*`

构建切换：

- 不再直接 include `control-core/modules/shared-kernel/include`
- 不再直接 include `../device-contracts/include`
- 改为通过 `siligen_device_contracts`、`siligen_shared_kernel` 传递公开 include 面
- 若单独接入 `device-adapters`，会先补注册 `packages/shared-kernel` 与 `packages/device-contracts`

### 2.4 旧 include/link 路径清理

已切掉的真实依赖面：

- `packages/device-contracts/CMakeLists.txt -> ../../control-core/modules/shared-kernel/include`
- `packages/device-adapters/CMakeLists.txt -> ../../control-core/modules/shared-kernel/include`
- `control-core/modules/CMakeLists.txt -> add_subdirectory(shared-kernel)` 的默认 owner 身份
- `control-core/CMakeLists.txt` 与 `control-core/src/shared/CMakeLists.txt` 中对 `modules/shared-kernel/include` 的默认 include 入口

## 3. 当前仍阻止删除的剩余点

### 3.1 `control-core/modules/shared-kernel`

代码级阻塞已基本清零：

- `siligen_shared_kernel` 的默认构建 owner 已切到 `packages/shared-kernel`
- `device-contracts` / `device-adapters` 已不再从 legacy shared-kernel include

剩余点：

- 目录本身还未执行物理删除
- 仓内多份架构/清理文档仍把该目录写成 legacy 参考路径
- `control-core/modules/shared-kernel/CMakeLists.txt` 与 `README.md` 仍作为兼容说明留存

结论：

- 删除门槛已从“代码依赖阻塞”降为“文档与目录清理阻塞”

### 3.2 `control-core/modules/device-hal`

`device-hal` 仍不能整体删除，剩余阻塞点必须按实现切片精确列出：

- `src/adapters/recipes/AuditFileRepository.*`
- `src/adapters/recipes/ParameterSchemaFileProvider.*`
- `src/adapters/recipes/RecipeBundleSerializer.*`
- `src/adapters/recipes/RecipeFileRepository.*`
- `src/adapters/recipes/RecipeJsonSerializer.*`
- `src/adapters/recipes/TemplateFileRepository.*`
- `src/adapters/diagnostics/logging/SpdlogLoggingAdapter.*`
- `src/adapters/diagnostics/logging/sinks/MemorySink.h`
- `src/adapters/diagnostics/health/HardwareValidationService.*`
- `src/adapters/diagnostics/health/serializers/*`
- `src/adapters/diagnostics/health/testing/HardwareTestAdapter.*`
- `src/adapters/diagnostics/health/testing/HardwareTestAdapter.Connection.cpp`
- `src/adapters/diagnostics/health/testing/HardwareTestAdapter.Jog.cpp`
- `src/adapters/diagnostics/health/testing/HardwareTestAdapter.Homing.cpp`
- `src/adapters/diagnostics/health/testing/HardwareTestAdapter.IO.cpp`
- `src/adapters/diagnostics/health/testing/HardwareTestAdapter.Trigger.cpp`
- `src/adapters/diagnostics/health/testing/HardwareTestAdapter.Cmp.cpp`
- `src/adapters/diagnostics/health/testing/HardwareTestAdapter.Interpolation.cpp`
- `src/adapters/diagnostics/health/testing/HardwareTestAdapter.Diagnostics.cpp`
- `src/adapters/diagnostics/health/testing/HardwareTestAdapter.Safety.cpp`
- `src/adapters/diagnostics/health/testing/HardwareTestAdapter.Helpers.cpp`
- `src/adapters/motion/controller/connection/HardwareConnectionAdapter.*`
- `src/adapters/motion/controller/control/MultiCardMotionAdapter.*`
- `src/adapters/motion/controller/homing/HomingPortAdapter.*`
- `src/adapters/motion/controller/homing/HomingSupport.h`
- `src/adapters/motion/controller/interpolation/InterpolationAdapter.*`
- `src/adapters/motion/controller/internal/UnitConverter.*`
- `src/adapters/motion/controller/runtime/MotionRuntimeConnectionAdapter.*`
- `src/adapters/motion/controller/runtime/MotionRuntimeFacade.*`
- `src/adapters/dispensing/dispenser/ValveAdapter.*`
- `src/adapters/dispensing/dispenser/triggering/TriggerControllerAdapter.*`
- `src/drivers/multicard/MultiCardAdapter.*`
- `src/drivers/multicard/MultiCardErrorCodes.*`
- `src/drivers/multicard/PositionTriggerController.cpp`
- `src/drivers/multicard/HomingController.h`

这些文件里仍混有以下非 package 化职责：

- 历史文件仓储与序列化
- diagnostics logging / query 前置实现
- runtime-facing hardware test / validation
- 更粗粒度的 motion / dispensing adapter

## 4. 验证与风险

### 4.1 本次新增/更新的验证

- `tools/migration/validate_device_split.py`
  - 校验 `shared-kernel` 是否已承接真实代码文件
  - 校验 `device-contracts` / `device-adapters` / `shared-kernel` 不再引用 `control-core/modules/*`
  - 保持对 `device-hal` legacy 混杂实现的可见性
- `packages/test-kit/src/test_kit/workspace_validation.py`
  - `packages` suite 新增 `device-shared-boundary` 校验

### 4.2 当前风险

- `packages/shared-kernel` 仍通过 `control-core` 的 CMake configure/build 间接编译，尚无完全独立的根级 C++ build/test 入口
- `device-hal` 的复杂 motion/dispensing/diagnostics/legacy-persistence 实现仍散落在 legacy 目录，删除顺序必须继续按职责拆分
- `packages/device-adapters` 仍使用 `control-core/third_party` 中的 Boost 头；这不是 `modules/*` 依赖，但后续若要完全脱离 `control-core` 目录，还要继续处理第三方头布局
