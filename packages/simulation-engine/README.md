# simulation-engine

`packages/simulation-engine` 是 SiligenSuite 运动仿真的 canonical owner。

当前状态已经收口到方案 C：

- 尽量复用 `packages/process-runtime-core` 的真实控制/执行逻辑
- 在本包内提供确定性虚拟时钟、虚拟设备、记录器和场景编排
- 让“真实控制逻辑跑在假硬件上”，而不是长期维护一套平行控制器
- P6 起允许在 `virtual-devices/` 和 `recording/` 里做可选增强，但必须保持 deterministic replay

## 当前状态

- 方案 C 已成为完整的 canonical 运动仿真实现。
- `application/`、`runtime-bridge/`、`virtual-time/`、`virtual-devices/`、`recording/` 承担正式闭环。
- `include/`、`src/`、compat 示例与 compat 测试仅保留历史 baseline / smoke / 对照职责，不再承接新能力。
- 后续新增场景、导出契约、session 编排和 replay 语义都应落在 `sim::scheme_c::*`。
- 当前首个可选增强包为 `motion_realism`：
  可选启用跟随误差近似与编码器量化；默认关闭，因此现有 canonical 场景和 compat 基线保持不变。

## 目录约定

- `application/`：仿真会话编排、场景装配、最小公开入口
- `runtime-bridge/`：把 `process-runtime-core` 的 use case / 端口接到虚拟设备
- `virtual-time/`：确定性时钟、tick 驱动、调度顺序
- `virtual-devices/`：虚拟轴、虚拟编码器、home/limit、虚拟 IO
- `recording/`：timeline、trace、结果汇总、断言与导出
- `include/`、`src/`：compat 兼容时序引擎，只用于历史基线和最小 smoke
- `tests/`：包内 scheme C 回归矩阵 + compat smoke / 对照测试
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

## 正式入口

- 包内最小闭环示例：
  `examples/simulate_scheme_c_closed_loop.cpp`
- 包内公开入口：
  `include/sim/scheme_c/application_runner.h`
- 根级最小闭环集成回归：
  `integration/simulated-line/run_simulated_line.py`
- 受控生产测试串行入口：
  `integration/simulated-line/run_controlled_production_test.ps1`
- compat 基线入口：
  `simulate_dxf_path.exe` / `sim::SimulationEngine`
  仅用于历史 baseline 对照，不再扩展功能面

## 当前验证命令

```powershell
cmake --fresh -S D:\Projects\SiligenSuite\packages\simulation-engine -B D:\Projects\SiligenSuite\build\simulation-engine -DSIM_ENGINE_BUILD_EXAMPLES=ON -DSIM_ENGINE_BUILD_TESTS=ON
cmake --build D:\Projects\SiligenSuite\build\simulation-engine --config Debug
ctest --test-dir D:\Projects\SiligenSuite\build\simulation-engine -C Debug --output-on-failure
python D:\Projects\SiligenSuite\integration\simulated-line\run_simulated_line.py
python D:\Projects\SiligenSuite\integration\simulated-line\run_simulated_line.py --report-dir D:\Projects\SiligenSuite\integration\reports\local\simulation\simulated-line
powershell -NoProfile -ExecutionPolicy Bypass -File D:\Projects\SiligenSuite\integration\simulated-line\run_controlled_production_test.ps1 -Profile Local
```

当前 `integration/simulated-line` 默认覆盖：

- compat `rect_diag` 历史基线对照
- scheme C `rect_diag` 长路径 / 多段连续执行
- scheme C `sample_trajectory` 线段 + 圆弧混合路径与 compare output + delayed IO + valve delay 组合
- scheme C `invalid_empty_segments` 结构化失败结果与重复执行确定性
- scheme C `following_error_quantized` 可选 `motion_realism` 场景，校验跟随误差、编码器量化和 repeated-run deterministic

## 受控生产测试范围

当前允许用于受控生产测试的范围：

- 运动路径执行、home / move / path / stop / pause / resume 的确定性回归
- compare output、IO delay、valve replay 的确定性回放
- canonical simulation input 的结构化失败结果与 repeated-run deterministic
- `motion_realism` 可选增强下的跟随误差与编码器量化可重复性
- root `simulation` suite 落盘的 `workspace-validation.*` 与 `simulated-line-summary.*` 证据链

当前不作为受控生产测试结论的一部分：

- 胶量、温度、压力、材料等工艺真实性
- 完整伺服物理模型、wall-clock 抖动、UI、TCP、runtime-host
- 用仿真替代整机或真实硬件验收

## 可选运动真实性增强

当前仅实现一组可选增强：

- `motion_realism.enabled`
- `motion_realism.servo_lag_seconds`
- `motion_realism.encoder_quantization_mm`
- 可选 `motion_realism.following_error_threshold_mm`

用途：

- 让虚拟轴在 recording / timeline / summary 中呈现更接近真实驱动器读数的延迟反馈
- 在不引入第二套控制器的前提下，保留 deterministic replay

明确不模拟：

- 电流环 / 速度环 / 位置环的完整伺服物理模型
- 摩擦、振动、结构弹性、热漂移、材料或工艺过程
- wall-clock 抖动、UI、TCP、runtime-host

详细边界见 [BOUNDARIES.md](./BOUNDARIES.md)。
最小闭环说明见 [docs/minimal-closed-loop.md](./docs/minimal-closed-loop.md)。
方案 C 的接口冻结基线见 [docs/implementation/solution-c-interface-freeze.md](./docs/implementation/solution-c-interface-freeze.md)。
