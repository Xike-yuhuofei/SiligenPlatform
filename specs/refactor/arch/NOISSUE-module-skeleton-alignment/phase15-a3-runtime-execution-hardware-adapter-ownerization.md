# Phase 15 A3: runtime-execution hardware adapter ownerization

更新时间：`2026-03-25`

## 目标

把 `runtime-execution` 首轮硬件 smoke 主链从 legacy bridge sources 提升到 canonical owner 面，优先完成 `HardwareTestAdapter` 公开头与主构建链的 canonical 化，并确保 `runtime/host` 仍能通过稳定 target 名称完成装配。

## 本次 owner 化结果

### 1. 已完成 canonical owner 化的 public headers

以下 public headers 已从回指 `src/legacy` 改为回指 canonical `src/adapters/**`：

- `modules/runtime-execution/adapters/device/include/siligen/device/adapters/hardware/HardwareTestAdapter.h`
- `modules/runtime-execution/adapters/device/include/siligen/device/adapters/hardware/HardwareConnectionAdapter.h`
- `modules/runtime-execution/adapters/device/include/siligen/device/adapters/motion/MultiCardMotionAdapter.h`
- `modules/runtime-execution/adapters/device/include/siligen/device/adapters/motion/InterpolationAdapter.h`
- `modules/runtime-execution/adapters/device/include/siligen/device/adapters/motion/HomingSupport.h`
- `modules/runtime-execution/adapters/device/include/siligen/device/adapters/motion/HomingPortAdapter.h`
- `modules/runtime-execution/adapters/device/include/siligen/device/adapters/motion/MotionRuntimeFacade.h`
- `modules/runtime-execution/adapters/device/include/siligen/device/adapters/motion/MotionRuntimeConnectionAdapter.h`
- `modules/runtime-execution/adapters/device/include/siligen/device/adapters/dispensing/ValveAdapter.h`
- `modules/runtime-execution/adapters/device/include/siligen/device/adapters/dispensing/TriggerControllerAdapter.h`

其中 `HardwareConnectionAdapter` 对 `MultiCardAdapter` 的依赖已收口为前置声明，不再把 `legacy/drivers` 私有实现头暴露到 `runtime/host` 的公开接线链上。

### 2. 已迁入 canonical source roots 的硬件 smoke 主链资产

以下实现已从 `modules/runtime-execution/adapters/device/src/legacy/adapters/**` 迁到 `modules/runtime-execution/adapters/device/src/adapters/**`：

- `diagnostics/health/testing/HardwareTestAdapter.h`
- `diagnostics/health/testing/HardwareTestAdapter.cpp`
- `diagnostics/health/testing/HardwareTestAdapter.Connection.cpp`
- `diagnostics/health/testing/HardwareTestAdapter.Jog.cpp`
- `diagnostics/health/testing/HardwareTestAdapter.Homing.cpp`
- `diagnostics/health/testing/HardwareTestAdapter.IO.cpp`
- `diagnostics/health/testing/HardwareTestAdapter.Trigger.cpp`
- `diagnostics/health/testing/HardwareTestAdapter.Cmp.cpp`
- `diagnostics/health/testing/HardwareTestAdapter.Interpolation.cpp`
- `diagnostics/health/testing/HardwareTestAdapter.Diagnostics.cpp`
- `diagnostics/health/testing/HardwareTestAdapter.Safety.cpp`
- `diagnostics/health/testing/HardwareTestAdapter.Helpers.cpp`
- `motion/controller/connection/HardwareConnectionAdapter.h`
- `motion/controller/connection/HardwareConnectionAdapter.cpp`
- `motion/controller/control/MultiCardMotionAdapter.h`
- `motion/controller/control/MultiCardMotionAdapter.cpp`
- `motion/controller/control/MultiCardMotionAdapter.Status.cpp`
- `motion/controller/control/MultiCardMotionAdapter.Motion.cpp`
- `motion/controller/control/MultiCardMotionAdapter.Settings.cpp`
- `motion/controller/control/MultiCardMotionAdapter.Helpers.cpp`
- `motion/controller/homing/HomingSupport.h`
- `motion/controller/homing/HomingPortAdapter.h`
- `motion/controller/homing/HomingPortAdapter.cpp`
- `motion/controller/interpolation/InterpolationAdapter.h`
- `motion/controller/interpolation/InterpolationAdapter.cpp`
- `motion/controller/internal/UnitConverter.h`
- `motion/controller/internal/UnitConverter.cpp`
- `motion/controller/runtime/MotionRuntimeFacade.h`
- `motion/controller/runtime/MotionRuntimeFacade.cpp`
- `motion/controller/runtime/MotionRuntimeConnectionAdapter.h`
- `motion/controller/runtime/MotionRuntimeConnectionAdapter.cpp`
- `dispensing/dispenser/ValveAdapter.h`
- `dispensing/dispenser/ValveAdapter.cpp`
- `dispensing/dispenser/ValveAdapter.Dispenser.cpp`
- `dispensing/dispenser/ValveAdapter.Supply.cpp`
- `dispensing/dispenser/ValveAdapter.Hardware.cpp`
- `dispensing/dispenser/triggering/TriggerControllerAdapter.h`
- `dispensing/dispenser/triggering/TriggerControllerAdapter.cpp`

### 3. 构建口径调整

`modules/runtime-execution/adapters/device/CMakeLists.txt` 当前已把上述硬件 smoke 主链实现纳入 `SILIGEN_DEVICE_ADAPTERS_SOURCES`，并把 `SILIGEN_DEVICE_ADAPTERS_LEGACY_BRIDGE_SOURCES` 收缩到：

- `modules/runtime-execution/adapters/device/src/legacy/drivers/multicard/MultiCardAdapter.cpp`

此外，在未提供真实 `MultiCard.lib` 的构建条件下，仍会额外编译：

- `modules/runtime-execution/adapters/device/src/legacy/drivers/multicard/MultiCardStub_Class.cpp`

## 剩余 legacy bridge sources 及其不阻塞原因

### 1. 剩余文件

- `modules/runtime-execution/adapters/device/src/legacy/drivers/multicard/MultiCardAdapter.cpp`
- `modules/runtime-execution/adapters/device/src/legacy/drivers/multicard/MultiCardStub_Class.cpp`（仅在无真实卡库时启用）

### 2. 为什么这些残留不阻塞首轮硬件 smoke

- 这两个文件位于 vendor/driver 边界，负责封装 MultiCard SDK 或 mock/stub 回退，不再承载 `HardwareTestAdapter`、运动执行、回零、插补、阀控、触发控制等公开 owner 适配面。
- 首轮硬件 smoke 经过的 public headers 已全部从 canonical `include -> src/adapters/**` 路径解析，不再回指 `src/legacy`。
- `runtime/host` 的接线和工厂编译已通过，说明外部消费者不需要知道 `legacy/drivers` 的私有目录布局即可完成构建。
- 真实硬件与无卡构建两条路径仍保持 target 名称与 vendor 边界稳定，满足本阶段“先 canonical 化主链，不改上层 app 入口”的约束。

## 验证结果

- 通过：`rg -n "src/legacy|process-runtime-core/src" D:\Projects\SS-dispense-align\modules\runtime-execution\adapters\device\include D:\Projects\SS-dispense-align\modules\runtime-execution\adapters\device\src D:\Projects\SS-dispense-align\modules\runtime-execution\runtime\host`
  - 无匹配
- 通过：`cmake -S D:\Projects\SS-dispense-align -B C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build`
- 通过：`cmake --build C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build --config Debug --target siligen_device_adapters siligen_runtime_host siligen_runtime_host_unit_tests`
- 通过：`ctest --test-dir C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build -C Debug -R "siligen_runtime_host_unit_tests" --output-on-failure`

## 对 A4 的实际输入

A4 可以直接基于以下已固定事实继续推进：

- `runtime-execution` 首轮硬件 smoke 主链的 public surface 已 canonical 化，后续不应再把 owner 面回退到 `src/legacy`。
- `runtime/host` 当前已能通过 canonical public surface 和工厂装配完成编译，A4 可在此基础上继续做更深的 runtime wiring 或集成面清理。
- 仍未迁出的 residual 已收敛到 MultiCard vendor/driver 边界；A4 若要继续推进，应明确是否要把这部分进一步抽象或继续保留为受控 residual，而不是重新把上层 adapter 主链拉回 legacy。

## 结论

- `runtime-execution` 的硬件适配主链已完成本轮 canonical owner 化。
- `HardwareTestAdapter` 公开面与 `runtime/host` 主构建链均已退出对 `src/legacy` 的公开依赖。
- 当前剩余 legacy bridge sources 仅位于 MultiCard vendor/driver 边界，不阻塞 A4 继续推进 runtime-execution 的后续 owner 收敛。
