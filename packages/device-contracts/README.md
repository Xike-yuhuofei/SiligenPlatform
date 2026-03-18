# device-contracts

`device-contracts` 现在承接设备边界的纯契约，不再让设备公开头直接依赖 `process-core` 或 `motion-core`。

当前落地内容：

- `commands/`：运动、点胶、IO、连接命令
- `state/`：设备会话、运动状态、IO 状态、点胶状态、健康快照
- `capabilities/`：轴能力、IO 能力、触发能力、点胶能力
- `faults/`：设备故障模型
- `events/`：设备事件模型
- `ports/`：`MotionDevicePort`、`DigitalIoPort`、`DispenserDevicePort`、`MachineHealthPort`

依赖约束：

- 只能依赖 `shared-kernel`
- 不能依赖任何实现包
- 不得直接 include / link `control-core/modules/*`

当前最小可用切分：

- `MotionDevicePort` 仍然聚合了连接、执行、状态和故障读取
- `DigitalIoPort` 暂时独立于运动端口，但还没有细分为输入、输出、触发三类

后续拆分点：

- `MotionDevicePort` -> `AxisControlPort` / `HomingPort` / `InterpolationPort`
- `DigitalIoPort` -> `DigitalInputPort` / `DigitalOutputPort` / `TriggerPort`
- `DispenserDevicePort` -> `DispenserValvePort` / `SupplyPort`

当前构建口径：

- `CMake` 只通过 `siligen_shared_kernel` 获取共享基础头
- 不再显式 include legacy `control-core/modules/shared-kernel/include`
