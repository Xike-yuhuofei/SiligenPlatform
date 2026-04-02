# Simulation 受控生产测试说明 V1

更新时间：`2026-04-01`

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
python D:\Projects\ss-e\tests\e2e\simulated-line\run_controlled_production_test.py --profile Local --report-dir D:\Projects\ss-e\tests\reports\controlled-production-test
```

该入口固定串行执行：

1. `build.ps1 -Suite e2e`
2. `test.ps1 -Suite e2e -FailOnKnownFailure`
3. `verify_controlled_production_gate.py` 受控生产 Gate 校验
4. `render_controlled_production_release_summary.py` 自动生成发布结论摘要

说明：

- 即使 `test.ps1 -FailOnKnownFailure` 返回非 `0`，wrapper 仍会继续生成 gate / release summary 证据。
- 最终返回码仍保持阻断语义，不会因为报告生成成功而误报为通过。
- `run_simulated_line.py` 优先使用现有 executable；若仓库不存在 legacy `simulate_*` binary，则回退到 canonical replay exports 与 summary fixtures，不再把缺失旧 binary 视为已知失败。

禁止并发 build/test，避免 Windows 链接锁导致的伪失败。

常用双轨发布命令（时间戳目录运行 + latest 发布）：

```powershell
python D:\Projects\ss-e\tests\e2e\simulated-line\run_controlled_production_test.py `
  --profile Local `
  --use-timestamped-report-dir `
  --publish-latest-on-pass true `
  --publish-latest-report-dir tests\reports\controlled-production-test
```

### 3.2 单独回归入口

```powershell
python D:\Projects\ss-e\tests\e2e\simulated-line\run_simulated_line.py --mode both --report-dir D:\Projects\ss-e\tests\reports\local\simulation\simulated-line
```

### 3.3 Gate 校验入口

```powershell
python D:\Projects\ss-e\tests\e2e\simulated-line\verify_controlled_production_gate.py --report-dir D:\Projects\ss-e\tests\reports\controlled-production-test
```

### 3.4 发布结论摘要生成入口

```powershell
python D:\Projects\ss-e\tests\e2e\simulated-line\render_controlled_production_release_summary.py --report-dir D:\Projects\ss-e\tests\reports\controlled-production-test --profile Local
```

## 4. 通过判定

只有同时满足以下条件，才允许填写“受控生产测试通过”：

- `controlled-production-gate-summary.json` 的 `overall_status=passed`
- `e2e` workspace gate 无 `failed` / `known_failure` / `skipped`
- `engineering-regression` 通过
- `simulated-line` workspace case 通过
- `simulated-line` 的 scheme C 场景通过
- `following_error_quantized` 的统计值与 baseline 一致
- 所有场景 `deterministic_replay_passed=true`
- `validation-evidence-bundle.json` 包含：
  - `fault_matrix_id = fault-matrix.simulated-line.v1`
  - `selected_fault_ids`
  - `deterministic_seed`
  - `clock_profile`
  - `double_surface`
- 报告文件全部落盘：
  - `workspace-validation.json`
  - `workspace-validation.md`
  - `e2e/simulated-line/simulated-line-summary.json`
  - `e2e/simulated-line/simulated-line-summary.md`
  - `e2e/simulated-line/validation-evidence-bundle.json`
  - `controlled-production-gate-summary.json`
  - `controlled-production-gate-summary.md`
  - `simulation-controlled-production-release-summary.md`
- 不允许出现 `known_failure` / `skipped`

双轨证据约定：

- 时间戳目录保留原始批次（例如 `.../controlled-production-test/20260320T...Z`）
- 固定目录发布 latest 通过结果（默认 `tests/reports/controlled-production-test`）
- 来源追溯使用 `tests/reports/controlled-production-test/latest-source.txt`

## 5. 升级到 HIL 的前置条件

只有本页通过后，才允许推进：

- HIL smoke
- HIL soak / 启停恢复 / 报警恢复
- 真机 smoke / 试运行 / 现场签收

在以上升级动作完成前，`simulation` 的结论仍然只表示：

- `受控仿真验收通过`
- `不等于整机/工艺/真机验收通过`
