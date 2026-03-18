# virtual-devices

这里承接虚拟硬件。

职责：

- 虚拟轴组
- 虚拟编码器与反馈
- home / soft-limit / limit 状态
- 虚拟 IO 与触发反馈

规则：

- 只实现设备能力，不吸入业务状态机
- 只依赖 contracts，不依赖真实 device-adapters
