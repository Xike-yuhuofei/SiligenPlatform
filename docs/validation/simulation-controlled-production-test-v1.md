# Simulation 受控生产测试说明 V1

更新时间：`2026-03-19`

## 1. 目标

本页用于把 `simulation` 固定为进入 HIL / 真机前的受控生产测试入口。
它只覆盖可重复的仿真证据，不替代整机、工艺或真实硬件验收。

## 2. 适用范围

当前受控生产测试只包含：

- 运动路径执行、home / move / path / stop / pause / resume 的确定性回归
- compare output、IO delay、valve replay 的确定性回放
- canonical simulation input 的结构化失败结果
- `motion_realism` 可选增强下的 following error / encoder quantization 可重复性
- `PbPathSourceAdapter` 在 `SILIGEN_ENABLE_PROTOBUF=OFF` 下的显式降级合同

当前明确不包含：

- PB path 生产能力恢复
- 真实控制卡、真实 IO、TCP/HMI/UI、runtime-host 联调
- 胶量、温度、压力、材料等工艺真实性
- 用仿真替代整机或真实硬件签收

## 3. 固定入口

### 3.1 一键串行入口

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File D:\Projects\SiligenSuite\integration\simulated-line\run_controlled_production_test.ps1 -Profile Local
```

该入口固定串行执行：

1. `build.ps1 -Suite simulation`
2. `test.ps1 -Suite simulation -FailOnKnownFailure`

禁止并发 build/test，避免 Windows 链接锁导致的伪失败。

### 3.2 单独回归入口

```powershell
python D:\Projects\SiligenSuite\integration\simulated-line\run_simulated_line.py --mode both --report-dir D:\Projects\SiligenSuite\integration\reports\local\simulation\simulated-line
```

### 3.3 Standalone fresh build 入口

```powershell
cmake --fresh -S D:\Projects\SiligenSuite\packages\simulation-engine -B D:\Projects\SiligenSuite\build\simulation-engine -DSIM_ENGINE_BUILD_EXAMPLES=ON -DSIM_ENGINE_BUILD_TESTS=ON
cmake --build D:\Projects\SiligenSuite\build\simulation-engine --config Debug
ctest --test-dir D:\Projects\SiligenSuite\build\simulation-engine -C Debug --output-on-failure
```

## 4. 通过判定

只有同时满足以下条件，才允许填写“受控生产测试通过”：

- `simulation` suite 全部 `passed`
- `process_runtime_core_deterministic_path_execution_test`、`process_runtime_core_motion_runtime_assembly_test`、`process_runtime_core_pb_path_source_adapter_test` 全部通过
- `simulated-line` 的 compat + scheme C 场景全部通过
- `following_error_quantized` 的统计值与 baseline 一致
- 所有场景 `deterministic_replay_passed=true`
- 报告文件全部落盘：
  - `workspace-validation.json`
  - `workspace-validation.md`
  - `simulation/simulated-line/simulated-line-summary.json`
  - `simulation/simulated-line/simulated-line-summary.md`
- 不允许出现 `known_failure` / `skipped`

## 5. 升级到 HIL 的前置条件

只有本页通过后，才允许推进：

- HIL smoke
- HIL soak / 启停恢复 / 报警恢复
- 真机 smoke / 试运行 / 现场签收

在以上升级动作完成前，`simulation` 的结论仍然只表示：

- `受控仿真验收通过`
- `不等于整机/工艺/真机验收通过`
