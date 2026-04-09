# runtime-execution/adapters

`modules/runtime-execution/adapters/` 是 `M9 runtime-execution` 设备/协议接入适配的 canonical owner 根。

## 归位范围

- 设备驱动适配（运动、IO、点胶、健康等）与运行时接入桥接。
- 运行时链所需的 fake/simulation 适配层。
- 面向 `apps/runtime-service/`、`apps/runtime-gateway/` 的可装配适配实现。

## 边界约束

- 本目录实现应遵循 `shared/contracts/device/` 的稳定设备契约。
- 不承载业务状态机、规划计算、配方 owner 或追溯持久化等非适配职责。
- 不得回写上游规划事实；仅作为运行时执行链的下游接入层。

## 当前协作面

- `modules/runtime-execution/adapters/device/`
- `shared/contracts/device/`
- `apps/runtime-gateway/transport-gateway/`（协议接入与网关装配面）

## 验证要求

- 适配层依赖应收敛到设备契约与共享基础能力，不引入上游规划 owner 依赖。
- 宿主应用必须通过 `apps/runtime-service/`、`apps/runtime-gateway/` 装配新路径，不回退到 legacy owner。

## 当前收口状态（2026-04-09）

- `device/adapters` public surface 已不再依赖 `src/**` forwarding shell；对 motion value objects 的消费统一经 canonical include root 解析。
- `src/adapters/dispensing/dispenser/ValveAdapter.Dispenser.cpp` 已从 workflow-style `domain/dispensing/domain-services/DispenseCompensationService.h` 歧义入口，retarget 到 `modules/dispense-packaging/domain/dispensing/domain-services/DispenseCompensationService.h` canonical owner。
- motion connection 已收敛到 `MotionRuntimeFacade` + `DeviceConnectionPort` stable device contracts，不再依赖 `coordinate-alignment/domain/machine/IHardwareConnectionPort`。
- `HardwareTestAdapter` / `TriggerControllerAdapter` 保留为 `runtime-execution/adapters/device` 内部 concrete，但已不再继承或消费 `coordinate-alignment/domain/machine/IHardwareTestPort`。
