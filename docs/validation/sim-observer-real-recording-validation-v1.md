# Sim Observer 真实 Recording 验证 v1

- 验证日期：2026-03-19
- 增补日期：2026-03-20（V1.1 几何增强）
- 验证范围：`sim_observer` 对真实 `scheme_c.recording.v1` 样本的加载、路径归一化、`P0` 空态可浏览闭环、`P1` 真实样本默认进入与返回链路、`P1` 时间锚点高置信语义增强
- 结论：通过。`P1` 现已支持保守高置信语义提示：唯一证据可解析为 `resolved`，歧义证据继续保持 `mapping_insufficient`，不做激进推断；`P0` 对 `ARC` 已支持真圆弧图元渲染。

## 样本

- `examples/replay-data/sample_trajectory.scheme_c.recording.json`
- `examples/replay-data/rect_diag.scheme_c.recording.json`

## 兼容策略

- 当 `recording.motion_profile` 本身已经是段式结构时，继续沿用原有解析逻辑。
- 当 `recording.motion_profile` 实际为按轴采样流时，从 `timeline` 中解析 `Loaded canonical simulation input from ...` 消息。
- 读取对应 canonical simulation input 的 `segments`，转换为 `sim_observer` 当前可消费的路径段对象。
- `LINE` 直接复用起终点；`ARC` 根据 `center/radius/start_angle/end_angle` 采样生成点列，并保留起终点与包围盒事实。

## 验证结果

### 样本 1：`sample_trajectory`

- 归一化后路径段：3
- 段类型：`line`, `arc`, `line`
- 路径点：13
- P0 issue：0
- Time anchors：30
- P0 工作面状态：`issues=0 | segments=3 | mode=empty`
- P1 默认入口：`time_anchor:trace:0`
- P1 映射状态：`mapping_insufficient`，`backlink.level=unavailable`

### 样本 2：`rect_diag`

- 归一化后路径段：5
- 段类型：全部为 `line`
- 路径点：10
- P0 issue：0
- Time anchors：34
- P0 工作面状态：`issues=0 | segments=5 | mode=empty`

## 行为覆盖补充

- `P0` 无 issue 模式下自动聚焦首段，并输出路径事实详情（`focused.segment`、`point_count`、`structure.focus`）。
- `P0` 无 issue 模式下结构视图切换可稳定更新焦点与详情，不再停留在纯说明文案。
- `P0` 画布对 `ARC` 段优先使用真圆弧图元渲染；无法解析 arc 几何时自动回退 polyline，不影响直线段行为。
- 顶层 `SimObserverWorkspace` 可在真实样本下完成：`P0 -> P1 -> P0`，并保持 `P0` 结构焦点不丢失。

## 语义增强结果（保守高置信）

- `sample_trajectory`：默认时间锚点 `time_anchor:trace:0` 解析为 `single_object`，`time.object=segment:0`，`time.reason.code=time_mapping_from_snapshot_position`，`backlink.level=single_object`。
- `rect_diag`：默认时间锚点 `time_anchor:trace:0` 保持 `mapping_insufficient`，`time.reason.code=time_mapping_snapshot_ambiguous`，`backlink.level=unavailable`。
- 判定原则：仅在快照位置与路径段匹配唯一且可解释时升级到 `resolved`；多段歧义（尤其非连续候选）保持降级。
- 几何一致性补充：当段对象可提供 `arc_geometry` 时，`time_mapping_hints` 优先使用圆弧解析距离；无法解析时回退 polyline 距离。

## 执行命令

```powershell
python -m py_compile `
  apps/hmi-app/src/hmi_client/features/sim_observer/adapters/recording_loader.py `
  apps/hmi-app/src/hmi_client/features/sim_observer/adapters/recording_normalizer.py `
  apps/hmi-app/src/hmi_client/features/sim_observer/ui/p0_workspace.py `
  apps/hmi-app/src/hmi_client/features/sim_observer/ui/p1_workspace.py `
  apps/hmi-app/src/hmi_client/features/sim_observer/ui/sim_observer_workspace.py `
  apps/hmi-app/tests/unit/sim_observer/test_real_recording_validation.py

python -m unittest apps/hmi-app/tests/unit/sim_observer/test_real_recording_validation.py
python -m unittest apps/hmi-app/tests/unit/sim_observer/test_time_mapping_hints.py
python -m unittest discover -s apps/hmi-app/tests/unit/sim_observer -p "test_*.py"
python -m unittest apps/hmi-app/tests/unit/test_smoke.py
```

## 实际结果

- `python -m py_compile ...`：通过
- `python -m unittest apps/hmi-app/tests/unit/sim_observer/test_real_recording_validation.py`：8 tests，通过
- `python -m unittest apps/hmi-app/tests/unit/sim_observer/test_time_mapping_hints.py`：5 tests，通过
- `python -m unittest discover -s apps/hmi-app/tests/unit/sim_observer -p "test_*.py"`：101 tests，通过
- `python -m unittest apps/hmi-app/tests/unit/test_smoke.py`：1 test，通过

## 风险备注

- `ARC` 真圆弧渲染已落地，原“起终点连线替代圆弧”的已知风险关闭；`arc_geometry` 不可解析时仍按兼容策略回退 polyline。
- 真实样本当前未携带 `summary.issues`，因此本轮验证重点是“可加载 + 可浏览 + 高置信语义提示”，而不是完整业务 issue 推断。
