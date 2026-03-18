# device-adapters

`device-adapters` 现在承接设备实现层，包含控制卡、IO、厂商 SDK 包装和 fake device。

当前落地内容：

- `drivers/multicard/`：MultiCard SDK wrapper、错误码映射、mock wrapper
- `motion/`：`MultiCardMotionDevice`
- `io/`：`MultiCardDigitalIoAdapter`
- `fake/`：`FakeMotionDevice`、`FakeDispenserDevice`
- `vendor/multicard/`：厂商 SDK 二进制与说明

依赖约束：

- 只能依赖 `device-contracts` 和 `shared-kernel`
- 不能反向依赖 `process-runtime-core`
- 不得直接 include / link `control-core/modules/*`

明确不放入本包的内容：

- 业务状态机
- 配方仓储
- 日志查询模型
- 追溯/观测持久化

当前构建口径：

- 通过 `siligen_device_contracts` 和 `siligen_shared_kernel` 获取公开头
- 不再显式 include legacy `control-core/modules/shared-kernel/include`
- `control-core/modules/device-hal` 中残留的 `recipes/`、`diagnostics/`、复杂 motion/dispensing adapter 仍需后续精确迁移，不回灌到本包

当前最小可用切分：

- `MultiCardMotionDevice` 仍然同时承担连接和基础运动执行
- `MultiCardDigitalIoAdapter` 仅覆盖最基础 DI/DO 读取与写入
- `FakeDispenserDevice` 先提供最小的 Prime/Start/Pause/Resume/Stop 行为

后续拆分点：

- `MultiCardMotionDevice` -> `ConnectionAdapter` / `AxisMotionAdapter` / `HomingAdapter`
- `drivers/multicard` 的历史命名空间统一迁到 `Siligen::Device::Adapters::Drivers::MultiCard`
- 位置触发/CMP、插补、供胶阀等粗粒度能力继续拆小
