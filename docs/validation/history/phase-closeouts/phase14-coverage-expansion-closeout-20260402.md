# Phase 14 联机覆盖扩面 Closeout

更新时间：`2026-04-02`

## 1. 范围结论

本轮只实现并验证以下两项：

1. `P0-05` canonical dry-run negative matrix
2. `P1-04` HMI runtime actions matrix

本轮未改动：

1. Phase 13 signed latest authority
2. `nightly-performance` blocking config
3. `limited-hil` formal gate / publish 语义
4. `disconnect recovery`、`60min+ soak`、新的 gate policy

## 2. 变更摘要

### 2.1 `P0-05`

- `tests/e2e/hardware-in-loop/run_real_dxf_machine_dryrun.py`
  - 新增 `--mock-io-json`
  - preflight 现在同时消费 `effective_interlocks.home_boundary_x_active/home_boundary_y_active`
- `tests/e2e/hardware-in-loop/run_real_dxf_machine_dryrun_negative_matrix.py`
  - 新增 canonical dry-run negative matrix summary runner
  - 当前矩阵固定覆盖：
    - `estop`
    - `door_open`
    - `home_boundary_x_active`
    - `home_boundary_y_active`

### 2.2 `P1-04`

- `apps/hmi-app/src/hmi_client/tools/ui_qtest.py`
  - runtime action chain 拆成显式 profile：
    - `preview`
    - `home_move`
    - `jog`
    - `supply_dispenser`
    - `estop_reset`
    - `door_interlock`
    - `full`
- `apps/hmi-app/scripts/online-smoke.ps1`
  - 新增 `-RuntimeActionProfile`
- `apps/hmi-app/scripts/run_online_runtime_action_matrix.py`
  - 新增 HMI runtime action matrix summary runner

## 3. Authority 与证据

### 3.1 `P0-05`

- matrix summary：
  - `tests/reports/adhoc/real-dxf-machine-dryrun-negative-matrix/20260402-115108/real-dxf-machine-dryrun-negative-matrix.json`
  - `tests/reports/adhoc/real-dxf-machine-dryrun-negative-matrix/20260402-115108/real-dxf-machine-dryrun-negative-matrix.md`
- per-case dry-run reports：
  - `dryrun-estop-active`
  - `dryrun-door-open`
  - `dryrun-limit-x-neg`
  - `dryrun-limit-y-neg`

### 3.2 `P1-04`

- matrix summary：
  - `tests/reports/adhoc/hmi-runtime-action-matrix/20260402-114150/online-runtime-action-matrix-summary.json`
  - `tests/reports/adhoc/hmi-runtime-action-matrix/20260402-114150/online-runtime-action-matrix-summary.md`
- per-profile evidence：
  - `runtime-preview`
  - `runtime-home-move`
  - `runtime-jog`
  - `runtime-supply-dispenser`
  - `runtime-estop-reset`
  - `runtime-door-interlock`

## 4. 验证结果

### 4.1 L0 / 结构与脚本语法

- `python -m py_compile`：
  - `tests/e2e/hardware-in-loop/run_real_dxf_machine_dryrun.py`
  - `tests/e2e/hardware-in-loop/run_real_dxf_machine_dryrun_negative_matrix.py`
  - `apps/hmi-app/src/hmi_client/tools/ui_qtest.py`
  - `apps/hmi-app/scripts/run_online_runtime_action_matrix.py`
- PowerShell AST parse：
  - `apps/hmi-app/scripts/online-smoke.ps1`

### 4.2 L2 / L3 / L5 目标验证

- `python .\apps\hmi-app\scripts\run_online_runtime_action_matrix.py`
  - 结果：`passed`
- `python .\tests\e2e\hardware-in-loop\run_real_dxf_machine_dryrun_negative_matrix.py --gateway-exe D:\Projects\ss-e\build\phase14-control-apps\bin\Debug\siligen_runtime_gateway.exe`
  - 结果：`passed`

### 4.3 构建前提

- 为避免污染用户现有 `%LOCALAPPDATA%\SiligenSuite\control-apps-build`，本轮使用独立 build root：
  - `D:\Projects\ss-e\build\phase14-control-apps`
- 执行：
  - `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile Local -Suite apps`
- 结果：`passed`

## 5. 剩余风险

1. `limit_x_pos/limit_y_pos` 不在当前 `mock.io.set` 的正式 authority 范围内，本轮未把它们伪装成已覆盖。
2. `P0-05` 与 `P1-04` 当前只形成独立 summary authority，未接入 `limited-hil` formal blocking gate。
3. 本轮验证使用的是独立 build root `build/phase14-control-apps`，不等同于 fixed latest publish。

## 6. 文档同步

已同步：

1. `docs/validation/online-test-matrix-v1.md`
2. `tests/e2e/hardware-in-loop/README.md`
