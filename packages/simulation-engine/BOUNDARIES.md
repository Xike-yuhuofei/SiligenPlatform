# simulation-engine 边界说明

## 1. 目标

本包只负责运动仿真，不负责胶量、温度、压力、材料等工艺仿真。

方案 C 下，本包的职责是：

- 用虚拟时钟和虚拟设备承接真实控制逻辑
- 提供 deterministic 的运动执行环境
- 提供场景回归和结果记录能力

## 2. 内部模块

| 模块 | 职责 |
|---|---|
| `application/` | 仿真会话编排、场景装配、对外最小服务入口 |
| `runtime-bridge/` | 把 `process-runtime-core` 的控制/执行语义映射到虚拟端口 |
| `virtual-time/` | 固定步长时钟、tick 调度、暂停/恢复/超时控制 |
| `virtual-devices/` | 虚拟轴、虚拟编码器、home/limit、虚拟 IO、虚拟反馈 |
| `recording/` | timeline、trace、summary、结果导出与断言 |
| `include/` + `src/` | 迁移期兼容时序引擎，实现既有基线回归 |
| `tests/` | 单测、兼容测试、最小场景回归 |

## 3. 允许依赖

- `packages/process-runtime-core`
- `packages/device-contracts`
- `packages/engineering-contracts`
- `packages/engineering-data` 的离线产物与 fixture
- `packages/shared-kernel`

## 4. 禁止依赖

- `packages/runtime-host`
- `packages/transport-gateway`
- `packages/device-adapters`
- `apps/*`
- `control-core/src/infrastructure/runtime/*`
- `control-core/src/adapters/tcp/*`

## 5. 子模块规则

- `application/` 可以依赖其余仿真子模块，但不直接依赖 UI、TCP 或 runtime-host。
- `runtime-bridge/` 可以依赖 `process-runtime-core` 与 `device-contracts`，但不能反向把宿主装配或传输边界卷入。
- `virtual-devices/` 只表达虚拟硬件能力，不承接业务状态机。
- `recording/` 不参与控制决策，只记录和导出仿真事实。
- 迁移期兼容时序引擎只负责保住既有回归，不再扩大为第二套长期控制内核。

## 6. 当前最小 owner 判定

从现在开始，以下内容的 owner 归 `packages/simulation-engine`：

- 运动仿真源码
- 仿真 smoke/json-io 测试
- 仿真示例可执行程序
- 仿真回归脚本对接路径

根级旧 `simulation-engine/` 目录不再作为 owner 保留。

## 7. 正式验证入口

- 包内 build root：
  `D:\Projects\SiligenSuite\build\simulation-engine`
- 包内最小闭环 executable：
  `simulate_scheme_c_closed_loop.exe`
- 包内 scheme C 测试入口：
  `ctest --test-dir D:\Projects\SiligenSuite\build\simulation-engine -C Debug --output-on-failure -R "simulation_engine_scheme_c_.*_test"`
- 根级仿真 suite：
  `.\build.ps1 -Profile Local -Suite simulation`
  `.\test.ps1 -Profile Local -Suite simulation`
- 跨包最小场景回归：
  `integration/simulated-line/run_simulated_line.py`

以上入口都不依赖 `runtime-host`、`apps/*` 或 `control-core/src/infrastructure/runtime/*`。
