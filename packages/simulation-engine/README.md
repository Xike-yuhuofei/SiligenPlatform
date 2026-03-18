# simulation-engine

`packages/simulation-engine` 是 SiligenSuite 运动仿真的 canonical owner。

当前采用的总体方向是方案 C：

- 尽量复用 `packages/process-runtime-core` 的真实控制/执行逻辑
- 在本包内提供确定性虚拟时钟、虚拟设备、记录器和场景编排
- 让“真实控制逻辑跑在假硬件上”，而不是长期维护一套平行控制器

## 当前状态

- 原顶层 `simulation-engine/` 的源码、示例和测试已收口到本包
- `include/`、`src/`、`tests/`、`examples/` 当前仍承接兼容时序引擎与既有基线
- 方案 C 的新框架骨架从以下目录开始落位：
  - `application/`
  - `runtime-bridge/`
  - `virtual-time/`
  - `virtual-devices/`
  - `recording/`

这意味着当前包内同时存在两层内容：

1. 兼容层：保留现有时序引擎与回归资产，保证迁移期间 build/test 不断链。
2. 目标层：按方案 C 搭建真实控制逻辑 + 虚拟硬件的最终框架。

## 目录约定

- `application/`：仿真会话编排、场景装配、最小公开入口
- `runtime-bridge/`：把 `process-runtime-core` 的 use case / 端口接到虚拟设备
- `virtual-time/`：确定性时钟、tick 驱动、调度顺序
- `virtual-devices/`：虚拟轴、虚拟编码器、home/limit、虚拟 IO
- `recording/`：timeline、trace、结果汇总、断言与导出
- `include/`、`src/`：当前兼容时序引擎实现
- `tests/`：包内单测与兼容验证
- `examples/`：包内最小示例；跨包示例逐步收口到根级 `examples/simulation`

## 最小闭环

首个必须稳定跑通的闭环是：

1. 读取 canonical `simulation-input.json`
2. 创建 `VirtualClock`
3. 装配 `RuntimeBridge`
4. 绑定 `VirtualAxisGroup` / `VirtualIo`
5. 固定步长 tick 驱动执行
6. 记录 motion timeline
7. 导出 deterministic result

详细边界见 [BOUNDARIES.md](./BOUNDARIES.md)。
最小闭环说明见 [docs/minimal-closed-loop.md](./docs/minimal-closed-loop.md)。
方案 C 的接口冻结基线见 [docs/scheme-c-interface-baseline.md](./docs/scheme-c-interface-baseline.md)。
