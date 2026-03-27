# shared/contracts/device

`shared/contracts/device/` 是设备侧稳定跨模块契约的 canonical root。

- 构建 target：`siligen_device_contracts`
- public include root：`shared/contracts/device/include`

## 归位范围

以下契约属于跨模块稳定面，应在该根统一维护：

- 设备命令模型（运动、点胶、IO、连接）
- 设备状态/能力/故障/事件模型
- 设备端口协议（`MotionDevicePort`、`DigitalIoPort`、`DispenserDevicePort`、`MachineHealthPort`）
- 面向多 consumer 的稳定兼容约束

## 与 `M9 runtime-execution` 的职责分层

- 本目录只负责“跨模块稳定设备契约”。
- `modules/runtime-execution/contracts/` 负责 `M9` 运行时 owner 私有执行契约。
- `modules/runtime-execution/adapters/` 负责设备/协议适配实现，不在本目录承载实现细节。

## 边界约束

- 仅保留纯契约定义，不承载 runtime 专属适配实现。
- 运行时 owner 契约与适配应回到 `modules/runtime-execution/contracts/` 与 `modules/runtime-execution/adapters/`。
- 禁止在 canonical 根之外再维护第二套设备契约事实。

## 当前对照面

- 稳定跨模块设备契约：`shared/contracts/device/`
- runtime 私有执行契约入口：`modules/runtime-execution/contracts/device/`（兼容装配入口，不再定义 canonical target）
- 设备与协议适配实现：`modules/runtime-execution/adapters/device/`

## 验证要求

- 设备契约不得直接依赖实现包，仅允许稳定共享基础依赖。
- 契约拆分必须保持当前 consumer 的行为兼容，避免破坏执行链路。
