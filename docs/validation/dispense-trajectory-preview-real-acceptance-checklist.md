# 点胶轨迹真实预览验收清单

- 状态：Ready
- 日期：2026-03-27
- 目标：只接受 `planned_glue_snapshot + glue_points` 作为真实在线轨迹预览通过来源，并证明其与执行准备数据语义一致

## 1. 在线就绪门禁

- [ ] 已先运行 `apps/hmi-app/scripts/online-smoke.ps1`。
- [ ] smoke 输出包含最小阶段序列 `backend_starting -> backend_ready -> tcp_connecting -> hardware_probing -> online_ready`。
- [ ] smoke 输出包含 `SUPERVISOR_DIAG ... online_ready=true`。
- [ ] 若未达到 `online_ready`，本次验收结论必须落为 `not-ready`，不得继续记为真实通过。

## 2. 证据文件

- [ ] 当前证据目录位于 `tests/reports/online-preview/<timestamp>/` 或 `tests/reports/adhoc/real-dxf-preview-snapshot-canonical/<timestamp>/`。
- [ ] 存在 `plan-prepare.json`。
- [ ] 存在 `snapshot.json`。
- [ ] 存在 `trajectory_polyline.json`。
- [ ] 存在 `preview-verdict.json`。
- [ ] 存在 `preview-evidence.md`。

## 3. 快照追溯

- [ ] `plan-prepare.json` 中记录了 `plan_id` 与 `plan_fingerprint`。
- [ ] `snapshot.json` 中记录了 `snapshot_id`、`snapshot_hash`、`plan_id`、`preview_source`。
- [ ] `preview-verdict.json` 中记录了同一 `plan_id / plan_fingerprint / snapshot_hash`。
- [ ] 若证据包包含运行态标识，只允许记录 `job_id`；不得再以 `task_id` / `active_task_id` 作为 live 执行回链键。
- [ ] `snapshot_hash == plan_fingerprint`，可证明预览与执行准备属于同一上下文。

## 4. 来源边界

- [ ] `preview_source == planned_glue_snapshot`。
- [ ] `preview_kind == glue_points` 且 `glue_points` 非空。
- [ ] 若 `preview_source == mock_synthetic`，结论必须为 `invalid-source`。
- [ ] 若 `preview_source == runtime_snapshot`，结论必须为 `invalid-source` 或诊断-only，不得记为通过。
- [ ] 若来源缺失、未知或无法回链到当前 plan，结论必须为 `invalid-source` 或 `mismatch`。
- [ ] HMI 必须明确展示来源标签，不得把 mock 或未知来源文案伪装成真实通过。

## 5. 语义一致性

- [ ] `preview-verdict.json` 中 `geometry_semantics_match == true`。
- [ ] `preview-verdict.json` 中 `order_semantics_match == true`。
- [ ] `preview-verdict.json` 中 `dispense_motion_semantics_match == true`。
- [ ] 对 `samples/dxf/rect_diag.dxf`，可辨认矩形边段与对角线段。
- [ ] 已运行 `python -m pytest tests/e2e/first-layer/test_real_preview_snapshot_geometry.py -q`，且结果通过。

## 6. 非通过边界

- [ ] 证据缺少任一必需文件时，结论必须为 `incomplete`。
- [ ] 只能证明几何外观相似、但无法证明顺序或点胶运动语义时，结论必须为 `mismatch`。
- [ ] 本次结论不外推真实胶量、铺展、过冲、停机距离或接线闭环通过。
