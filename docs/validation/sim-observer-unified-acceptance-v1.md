# Sim Observer 统一验收矩阵 V1

> 本说明以《数据契约规格说明 V1》为唯一数据契约来源，并聚合 `P0` 与 `P1` 的正式验收结果。

## 摘要

本文件用于执行 `D6` 的统一行为级验收，覆盖 `S1-S5`、失败分支和不可接受状态拦截。  
它不替代分面文档，而是作为统一索引与发布判定入口：

- `P0` 细化验收：`docs/validation/sim-observer-p0-acceptance-v1.md`
- `P1` 细化验收：`docs/validation/sim-observer-p1-acceptance-v1.md`
- 真实样本浏览补充验证：`docs/validation/sim-observer-real-recording-validation-v1.md`（不改变 `S1-S5` 契约验收口径）
- V1.1 几何增强补充：`P0` 的 `ARC` 真圆弧渲染与 `time_mapping_hints` arc 几何距离口径一致

判定总门槛固定为：

- `S1-S5` 全部具备自动化证据
- `P0/P1` 失败分支均输出类型化失败语义
- `P0/P1` 红线状态均被显式拦截
- `sim_observer` 回归、smoke、语法检查全部通过

## 统一场景矩阵

| 场景 | 工作面 | Canonical Case IDs | 自动化证据入口 | 通过判定 | 当前状态 |
| --- | --- | --- | --- | --- | --- |
| `S1` 从摘要确认结果异常 | `P0` | `P0-S1-01`、`P0-S1-02` | `test_p0_acceptance.py` | 默认入口稳定，摘要切换后详情与 `SourceRefs` 一致 | 已覆盖 |
| `S2` 从画布确认路径是否异常 | `P0` | `P0-S2-01`、`P0-S2-02` | `test_p0_acceptance.py` | 失败态不伪高亮，组落点维持组语义 | 已覆盖 |
| `S3` 从结构确认顺序与衔接 | `P0` | `P0-S3-01`、`P0-S3-02`、`P0-S3-03` | `test_p0_acceptance.py` | 当前对象聚焦稳定，唯一回接成立，多命中不误切换 | 已覆盖 |
| `S4` 从报警项复盘前后窗口 | `P1` | `P1-S4-01`、`P1-S4-02`、`P1-S4-03` | `test_p1_acceptance.py` | 上下文窗口可解释，fallback 明示，报警链路可回接 `P0` | 已覆盖 |
| `S5` 从时间锚点理解结果如何形成 | `P1` | `P1-S5-01`、`P1-S5-02`、`P1-S5-03` | `test_p1_acceptance.py` | 默认进入稳定，时间锚点落点/关键窗口可解释，映射不足可降级 | 已覆盖 |
| `P0` 失败与红线 | `P0` | `P0-F-01`、`P0-F-02`、`P0-R-01` | `test_p0_acceptance.py` | 空态可解释，不可接受状态显式拦截 | 已覆盖 |
| `P1` 失败与红线 | `P1` | `P1-F-01`、`P1-R-01` | `test_p1_acceptance.py` | 窗口未收敛可回退，非法映射/回接状态显式拦截 | 已覆盖 |

## 自动化入口

统一验收命令固定为：

```powershell
python -m unittest discover -s D:\Projects\SiligenSuite\apps\hmi-app\tests\unit\sim_observer -p "test_*.py"
python -m unittest D:\Projects\SiligenSuite\apps\hmi-app\tests\unit\test_smoke.py
python -m py_compile D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\features\sim_observer\state\observer_store.py D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\features\sim_observer\ui\p0_workspace.py D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\features\sim_observer\ui\p1_workspace.py D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\features\sim_observer\ui\sim_observer_workspace.py D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\ui\main_window.py
```

统一验收自动化映射：

- `P0` 行为验收：`apps/hmi-app/tests/unit/sim_observer/test_p0_acceptance.py`
- `P1` 行为验收：`apps/hmi-app/tests/unit/sim_observer/test_p1_acceptance.py`
- `P0/P1` 工作面回归：`test_p0_workspace.py`、`test_p1_workspace.py`
- 状态与回接回归：`test_observer_store.py`、`test_observer_store_p1.py`
- 规则层回归：`test_rule_r1_primary_association.py`、`test_rule_r2_minimize_group.py`、`test_rule_r5_time_mapping.py`、`test_rule_r6_alert_context.py`、`test_rule_r7_key_window.py`
- 真实样本语义提示回归：`apps/hmi-app/tests/unit/sim_observer/test_time_mapping_hints.py`、`apps/hmi-app/tests/unit/sim_observer/test_real_recording_validation.py`

V1.1 几何增强补充验证：

- `test_real_recording_validation.py` 覆盖真实样本 arc 元数据与 P0 画布 arc 图元渲染。
- `test_time_mapping_hints.py` 覆盖 arc 几何距离优先 + polyline 回退策略。

## 发布判定

统一结论只允许填写：

- `通过`
- `有条件通过`
- `阻塞`

判定规则固定为：

- 所有统一验收命令通过，且无阻塞 case：`通过`
- 所有统一验收命令通过，但存在已知非阻断说明项：`有条件通过`
- 任一 unified case 或关键命令失败：`阻塞`
