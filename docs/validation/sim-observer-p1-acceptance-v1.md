# Sim Observer P1 验收矩阵 V1

> 本说明以《数据契约规格说明 V1》为唯一数据契约来源。

## 摘要

本文件用于执行 `P1` 的细化验收，只覆盖 `S4-S5`、失败分支和不可接受状态拦截，不覆盖真实 replay 样本验证。  
判定总门槛固定为：

- `S4-S5` 可运行
- 时间与报警分析能回接对象、对象集合或明确暴露不足
- 失败分支全部输出类型化失败语义
- 不可接受状态被显式拦截
- `sim_observer` 回归测试与 smoke 检查不回退

## 场景矩阵

| Case ID | 链路 | 关注点 | 通过判定 |
| --- | --- | --- | --- |
| `P1-S4-01` | 报警 | 上下文窗口 | 从报警项进入后，详情展示 `context.state: resolved` 与 `key.state: resolved` |
| `P1-S4-02` | 报警 | fallback 保护 | 单锚点报警只显示 `anchor_only_fallback` / `single_anchor`，不伪造完整窗口 |
| `P1-S4-03` | 报警 | 回接到 P0 | 报警链路回接后返回 `P0`，且 `P0` 保留一致的回接结果 |
| `P1-S5-01` | 时间锚点 | 默认进入 | `P0` 仍是默认首屏；进入 `P1` 后默认优先时间锚点 |
| `P1-S5-02` | 时间锚点 | 稳定落点 | 时间锚点能展示单对象或顺序区间，并输出关键窗口 |
| `P1-S5-03` | 时间锚点 | 映射不足保护 | `mapping_insufficient` 时详情显式说明失败，回接按钮禁用 |
| `P1-F-01` | 失败分支 | 窗口未收敛 | `window_not_resolved` 时保留失败语义和返回路径 |
| `P1-R-01` | 红线 | 不可接受状态 | 非法时间映射状态或非法回接状态被显式拦截，测试失败而非静默继续 |

## 自动化入口

执行命令：

```powershell
python -m unittest discover -s D:\Projects\SiligenSuite\apps\hmi-app\tests\unit\sim_observer -p "test_*.py"
python -m unittest D:\Projects\SiligenSuite\apps\hmi-app\tests\unit\test_smoke.py
python -m py_compile D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\features\sim_observer\state\observer_store.py D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\features\sim_observer\ui\p1_workspace.py D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\features\sim_observer\ui\sim_observer_workspace.py
```

场景自动化对应：

- `P1-S4-01` 至 `P1-R-01`：`apps/hmi-app/tests/unit/sim_observer/test_p1_acceptance.py`
- `P1` 基础工作面与回接回归：`apps/hmi-app/tests/unit/sim_observer/test_p1_workspace.py`
- `ObserverStore` 的 `P1` 状态与回接契约：`apps/hmi-app/tests/unit/sim_observer/test_observer_store_p1.py`
- 规则层回归：`apps/hmi-app/tests/unit/sim_observer/test_rule_r5_time_mapping.py`、`test_rule_r6_alert_context.py`、`test_rule_r7_key_window.py`

## 验收记录

发布前只记录三项：

- 执行日期
- 自动化结果：`通过 / 阻塞`
- 阻塞 case 与原因
