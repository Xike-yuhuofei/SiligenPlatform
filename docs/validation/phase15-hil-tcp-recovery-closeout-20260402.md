# Phase 15 HIL TCP Recovery Closeout

更新时间：`2026-04-02`

## 1. 范围结论

本轮只实现 `Phase 15 / P1-01`：

- 新增独立的 HIL TCP recovery authority
- 固定语义为 `TCP session disconnect/reconnect recovery`

本轮未改动：

- `run_hil_controlled_test.ps1`
- `verify_hil_controlled_gate.py`
- `render_hil_controlled_release_summary.py`
- `P1-03`、`P1-05`
- 任何物理断连、gateway crash/restart、控制卡掉电语义

## 2. 变更摘要

- 新增 `tests/e2e/hardware-in-loop/run_hil_tcp_recovery.py`
  - admission / safety preflight 复用现有 limited-HIL 口径
  - 每轮固定执行：
    - `connect`
    - `status`
    - `dxf.load(rect_diag.dxf)`
    - `disconnect`
    - `connect`
    - `status`
    - `dxf.load(rect_diag.dxf)`
    - `disconnect`
  - 输出：
    - `hil-tcp-recovery-summary.json`
    - `hil-tcp-recovery-summary.md`
    - `case-index.json`
    - `validation-evidence-bundle.json`
    - `report-manifest.json`
    - `report-index.json`
    - `evidence-links.md`
    - `failure-details.json`（非通过时）
- 更新 `docs/validation/online-test-matrix-v1.md`
  - 将 `P1-01` 从 `planned` 更新为 `existing`
  - 明确其为独立 authority，不接 formal gate
- 更新 `tests/e2e/hardware-in-loop/README.md`
  - 补充 `run_hil_tcp_recovery.py` 入口、命令和范围约束

## 3. Authority 与契约

- authority artifact：
  - `hil-tcp-recovery-summary.json`
  - `hil-tcp-recovery-summary.md`
- owner 层：
  - `tests/e2e/hardware-in-loop/`
- consumer-only：
  - `docs/validation/`
  - 阶段 closeout
- 当前冻结语义：
  - 只接受脚本内 `disconnect -> reconnect` 的 session 恢复语义
  - `dxf.load(rect_diag.dxf)` 是唯一 recovery probe

## 4. 验证结果

### 4.1 L0 / 结构与语法

- `python -m py_compile tests/e2e/hardware-in-loop/run_hil_tcp_recovery.py`
  - 结果：`passed`

### 4.2 L2 / 报告产物链路自检

- `python .\tests\e2e\hardware-in-loop\run_hil_tcp_recovery.py --report-dir .\tests\reports\adhoc\hil-tcp-recovery-selfcheck --gateway-exe .\build\bin\Debug\__missing_gateway__.exe --allow-skip-on-missing-gateway --operator-override-reason "self-check: artifact emission without hardware"`
  - 结果：summary 结论为 `skipped`
  - 目的：验证缺少 gateway 时的 summary / evidence bundle 落盘链路

自检输出：

- `tests/reports/adhoc/hil-tcp-recovery-selfcheck/hil-tcp-recovery-summary.json`
- `tests/reports/adhoc/hil-tcp-recovery-selfcheck/validation-evidence-bundle.json`
- `tests/reports/adhoc/hil-tcp-recovery-selfcheck/report-manifest.json`
- `tests/reports/adhoc/hil-tcp-recovery-selfcheck/report-index.json`

### 4.3 L5 / 受限联机验证

正式命令：

```powershell
python .\tests\e2e\hardware-in-loop\run_hil_tcp_recovery.py `
  --report-dir .\tests\reports\verify\phase15-hil-tcp-recovery\20260402T121317Z `
  --gateway-exe .\build\phase14-control-apps\bin\Debug\siligen_runtime_gateway.exe `
  --offline-prereq-report .\tests\reports\verify\phase13-limited-hil\20260402T023734Z\offline-prereq\workspace-validation.json `
  --rounds 10
```

结果：`passed`

正式证据：

- `tests/reports/verify/phase15-hil-tcp-recovery/20260402T121317Z/hil-tcp-recovery-summary.json`
- `tests/reports/verify/phase15-hil-tcp-recovery/20260402T121317Z/hil-tcp-recovery-summary.md`
- `tests/reports/verify/phase15-hil-tcp-recovery/20260402T121317Z/validation-evidence-bundle.json`
- `tests/reports/verify/phase15-hil-tcp-recovery/20260402T121317Z/report-manifest.json`
- `tests/reports/verify/phase15-hil-tcp-recovery/20260402T121317Z/report-index.json`
- `tests/reports/verify/phase15-hil-tcp-recovery/20260402T121317Z/case-index.json`
- `tests/reports/verify/phase15-hil-tcp-recovery/20260402T121317Z/evidence-links.md`

关键结果：

- `rounds_requested = 10`
- `rounds_passed = 10`
- `rounds_failed = 0`
- `reconnect_count = 10`
- `recovery_success_count = 10`
- `elapsed_seconds = 59.254`
- `offline_prerequisites_passed = true`
- `safety_preflight_passed = true`
- 每轮 `disconnect_ack.disconnected = true`
- 每轮 `post_reconnect_status.connected = true`
- 每轮 `probe_after_reconnect.status = passed`

### 4.4 本轮有意未执行

- 未把 `P1-01` 接入 controlled gate
- 未把 `P1-01` 纳入 release blocking policy

原因：

- 本轮目标是先收敛独立 authority，并完成一次受限 recovery 联机验证
- formal gate 仍保持 Phase 13/14 的冻结口径，不在本轮擅自扩面

### 4.5 L5 / 30 轮 recovery soak

正式命令：

```powershell
python .\tests\e2e\hardware-in-loop\run_hil_tcp_recovery.py `
  --report-dir .\tests\reports\verify\phase15-hil-tcp-recovery-soak\20260402T121602Z `
  --gateway-exe .\build\phase14-control-apps\bin\Debug\siligen_runtime_gateway.exe `
  --offline-prereq-report .\tests\reports\verify\phase13-limited-hil\20260402T023734Z\offline-prereq\workspace-validation.json `
  --rounds 30
```

结果：`passed`

正式证据：

- `tests/reports/verify/phase15-hil-tcp-recovery-soak/20260402T121602Z/hil-tcp-recovery-summary.json`
- `tests/reports/verify/phase15-hil-tcp-recovery-soak/20260402T121602Z/hil-tcp-recovery-summary.md`
- `tests/reports/verify/phase15-hil-tcp-recovery-soak/20260402T121602Z/validation-evidence-bundle.json`
- `tests/reports/verify/phase15-hil-tcp-recovery-soak/20260402T121602Z/report-manifest.json`
- `tests/reports/verify/phase15-hil-tcp-recovery-soak/20260402T121602Z/report-index.json`
- `tests/reports/verify/phase15-hil-tcp-recovery-soak/20260402T121602Z/case-index.json`
- `tests/reports/verify/phase15-hil-tcp-recovery-soak/20260402T121602Z/evidence-links.md`

关键结果：

- `rounds_requested = 30`
- `rounds_passed = 30`
- `rounds_failed = 0`
- `reconnect_count = 30`
- `recovery_success_count = 30`
- `elapsed_seconds = 179.656`
- `offline_prerequisites_passed = true`
- `safety_preflight_passed = true`
- 每轮 `disconnect_ack.disconnected = true`
- 每轮 `post_reconnect_status.connected = true`
- 每轮 `probe_after_reconnect.status = passed`

## 5. 残余风险

1. 当前验证结论已覆盖 `10` 轮受限联机验证和 `30` 轮 recovery soak，但仍只适用于 `TCP session disconnect/reconnect + dxf.load(rect_diag.dxf)`，不外推到物理断线、gateway crash/restart、控制卡掉电。
2. 若真实执行中出现 `status` 或 `dxf.load` 在 reconnect 后不稳定恢复，应转入 `incident`，而不是继续扩大 recovery 语义。
3. 当前 `P1-01` 未进入 formal gate，因此不会自动阻断 controlled release。

## 6. 文档同步

本 closeout 已记录 `P1-01` 的实现、联机验证与 soak 证据。

说明：

- `docs/validation/online-test-matrix-v1.md`
- `tests/e2e/hardware-in-loop/README.md`

上述矩阵/入口文档在当前任务分支工作树中另有更新，但不属于本次最小提交范围。

## 7. 下一步建议

1. 若要继续扩深，下一步应转向更长时长或跨班次 soak，而不是再重复同一 `30` 轮短周期验证。
2. 若后续需要纳入更高层 suite，应先明确 gate policy、失败归类和 operator override 规则。
3. 若再次出现恢复失败，按 `incident` 路线收集 `failure_context`、`recent_status_snapshot` 与 round 级 probe 证据继续定位。

## 8. 当前交付结论

- `P1-01` 当前以独立专项 authority 交付，结论已由 `10` 轮受限联机验证和 `30` 轮 soak 共同支撑。
- 当前阶段不把 `P1-01` 接入 `run_hil_controlled_test.ps1`、`verify_hil_controlled_gate.py` 或 `release-check.ps1`。
- 后续若无新的发布策略冻结或 incident 证据，不再继续扩写本阶段实现。
