# application

这里承接仿真会话编排与公开入口。

职责：

- 读取场景配置
- 装配 `runtime-bridge`、`virtual-time`、`virtual-devices`
- 启动和停止一次仿真会话
- 汇总 `recording` 结果
- 公开 `loadCanonicalSimulationInputJson/File` 与 `runMinimalClosedLoop/File`

不负责：

- 真实控制算法实现
- 虚拟硬件细节
- 结果持久化之外的运行时装配
- `io_delays` / `triggers` / `valve` 的完整 deterministic replay 仍按 `SC-NB-005` 跟踪
