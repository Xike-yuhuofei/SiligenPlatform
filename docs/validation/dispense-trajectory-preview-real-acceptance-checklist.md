# 点胶轨迹真实预览验收清单

- 状态：Draft
- 日期：2026-03-26
- 目标：防止将 mock 占位轨迹误签为真实 runtime 轨迹

## 1. 数据源门禁

- [ ] 本次验收记录了 `dxf.preview.snapshot` 原始响应。
- [ ] 响应内存在 `preview_source` 字段。
- [ ] `preview_source == runtime_snapshot`。
- [ ] 若 `preview_source == mock_synthetic`，本次验收立即终止，不进入“真实轨迹通过”结论。

## 2. 快照追溯

- [ ] 记录 `snapshot_id`。
- [ ] 记录 `snapshot_hash`。
- [ ] 记录 `plan_id`。
- [ ] 记录 `polyline_point_count` 与 `polyline_source_point_count`。
- [ ] 保存原始 `trajectory_polyline` 证据文件。

## 3. 几何一致性

- [ ] 预览几何与目标 DXF 主轮廓一致。
- [ ] 预览顺序与最终执行 plan 顺序一致。
- [ ] 不存在明显的 mock 正弦占位特征。
- [ ] 对 `rect_diag.dxf` 类样例，可辨认矩形边段与对角线段。
- [ ] 已运行 `python -m pytest tests/e2e/first-layer/test_real_preview_snapshot_geometry.py -q`，且结果通过。
- [ ] 对照 `tests/baselines/preview/rect_diag.preview-snapshot-baseline.json`，关键点位与范围未漂移。

## 4. HMI 一致性

- [ ] HMI 明确显示预览来源。
- [ ] HMI 渲染点数与 runtime `polyline_point_count` 一致。
- [ ] HMI 未对 runtime 点集做二次平滑或隐藏性过滤。

## 5. 结论边界

- [ ] 本次结论仅证明“真实 runtime 轨迹预览链路通过”。
- [ ] 本次结论不证明真实胶量、铺展、过冲、停机距离或接线闭环通过。
