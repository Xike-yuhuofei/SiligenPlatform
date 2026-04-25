# Demo-1 HMI Production Operator Test 2026-04-24

更新时间：`2026-04-24`

## 1. 记录目的

- 冻结 `Demo-1.dxf` 在 HMI operator production 正式入口上的本地 strict-pass 正例。
- 明确这次记录对应 `20260424-164007` 证据批次，不再沿用 `2026-04-22` 的 `BLOCK` 口径。
- 将 `operator_execution`、`traceability_correspondence`、`observation_integrity` 三类结论拆开保存，避免混写。

## 2. 本次 run 锚点

- 报告目录：`tests/reports/online-validation/hmi-production-operator-test/20260424-164007/`
- DXF：`D:\Projects\SiligenSuite\samples\dxf\Demo-1.dxf`
- HMI 入口：`tests/e2e/hardware-in-loop/run_hmi_operator_production_test.py`
- UI 自动化入口：`apps/hmi-app/src/hmi_client/tools/ui_qtest.py`
- 阶段序列：`preview-ready -> preview-refreshed -> production-started -> production-running -> production-completed -> next-job-ready`
- 阶段截图：
  - `hmi-screenshots/01-preview-ready.png`
  - `hmi-screenshots/02-preview-refreshed.png`
  - `hmi-screenshots/03-production-running.png`
  - `hmi-screenshots/04-production-completed.png`
  - `hmi-screenshots/05-next-job-ready.png`

## 3. 关键结论

- `operator_execution.status=passed`
  - `hmi_exit_code=0`
  - `issues=[]`
  - `contract_ok=true`
- `traceability_correspondence.status=passed`
  - `strict_one_to_one_proven=true`
  - `expected_trace_count=1011`
  - `actual_trace_count=1011`
  - `mismatch_count=0`
  - `terminal_state=completed`
- `observation_integrity.status=passed`
  - `job_status_sample_count=1363`
  - `machine_status_sample_count=1363`
  - `coord_status_sample_count=1363`
  - `observer_poll_error_count=0`
- `control_script_capability.status=allow`
  - 已实际覆盖 `production-started` 与 `next-job-ready`
  - 本轮不再是“脚本能力缺口导致 BLOCK”

## 4. 关键证据

- 正式报告：
  - `hmi-production-operator-test.json`
  - `hmi-production-operator-test.md`
- strict truth：
  - `job-traceability.json`
- 状态观测：
  - `job-status-history.json`
  - `machine-status-history.json`
  - `coord-status-history.json`
- gateway / HMI：
  - `gateway-launch.json`
  - `gateway-stdout.log`
  - `gateway-stderr.log`
  - `tcp_server.log`
  - `hmi-stdout.log`
  - `hmi-stderr.log`

## 5. 边界说明

- 该记录是 `P1-06` 的历史正例锚点，不替代 `P1-04` runtime action matrix。
- 严格一一对应的真值来源仍是 `job-traceability.json` / `dxf.job.traceability`，不是 summary-level alignment。
- 若后续正式口径需要长期生效，应以上收后的正式矩阵与 contract tests 为准，而不是只引用本历史文件。
