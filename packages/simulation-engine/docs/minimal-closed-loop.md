# 最小闭环

## 1. 目标

方案 C 的第一个可交付闭环，不追求完整机台覆盖，只要求以下链路稳定：

1. 输入：canonical `simulation-input.json`
2. 驱动：固定步长 `VirtualClock`
3. 执行：`RuntimeBridge` 调用真实控制/执行语义
4. 反馈：`VirtualAxisGroup` / `VirtualIo`
5. 输出：`motion_profile`、`timeline`、`summary`

## 2. 闭环结构

```text
simulation-input.json
    -> application/session
    -> runtime-bridge
    -> virtual-time
    -> virtual-devices
    -> recording
    -> result.json
```

## 3. 首版范围

- 轴组：`X`、`Y`、可选 `Z`
- 时钟：固定步长，默认 `1 ms`
- 动作：home、move、execute path、stop
- 反馈：position、velocity、home、soft-limit、running、done
- 记录：时间线、最终位置、总时长、关键状态变化

## 4. 暂不进入首版的内容

- 胶量/胶宽/材料工艺
- 电机、电流、振动、背隙、摩擦等伺服物理
- UI、TCP、runtime-host 装配
- 真实硬件驱动

## 5. 实现顺序

1. 复用现有 compatibility timing engine 保住历史基线
2. 增加 `VirtualClock`
3. 增加 `VirtualAxisGroup` 与基础反馈
4. 增加 `RuntimeBridge`，把真实执行语义接入虚拟端口
5. 增加 `Recorder`，统一导出结果
6. 用 `integration/simulated-line` 做最小回归
7. 方案 C 收口后，将 compat 降级为 baseline-only

## 6. 当前正式入口

- 应用入口：
  `sim::scheme_c::loadCanonicalSimulationInputJson/File`
  `sim::scheme_c::runMinimalClosedLoop/File`
- 包内示例：
  `packages/simulation-engine/examples/simulate_scheme_c_closed_loop.cpp`
- 根级输入：
  `examples/simulation/rect_diag.simulation-input.json`
  `examples/simulation/sample_trajectory.json`
- 根级最小闭环回归：
  `integration/simulated-line/run_simulated_line.py`

## 7. 当前正式验证命令

```powershell
cmake --fresh -S D:\Projects\SiligenSuite\packages\simulation-engine -B D:\Projects\SiligenSuite\build\simulation-engine -DSIM_ENGINE_BUILD_EXAMPLES=ON -DSIM_ENGINE_BUILD_TESTS=ON
cmake --build D:\Projects\SiligenSuite\build\simulation-engine --config Debug
ctest --test-dir D:\Projects\SiligenSuite\build\simulation-engine -C Debug --output-on-failure
python D:\Projects\SiligenSuite\integration\simulated-line\run_simulated_line.py
```

当前 package + integration 矩阵实际覆盖：

- compat `simulation_engine_smoke_test.exe`
- compat `simulation_engine_json_io_test.exe`
- compat `simulate_dxf_path.exe` 对照 `rect_diag.simulation-baseline.json`
- 全量 package `ctest`
- scheme C `rect_diag` 长路径 / 多段连续执行
- scheme C `sample_trajectory` 线段 + 圆弧混合路径与 replay 组合
- scheme C `invalid_empty_segments` 结构化失败结果

compat 现状：

- 仅保留历史基线和对照验证
- 不再作为新增仿真能力入口
