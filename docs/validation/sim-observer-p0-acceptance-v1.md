# Sim Observer P0 验收矩阵 V1

> 本说明以《数据契约规格说明 V1》为唯一数据契约来源。

## 摘要

本文件用于执行 `P0` 的细化验收，只覆盖 `S1-S3`、失败分支和不可接受状态拦截，不覆盖 `P1`。  
判定总门槛固定为：

- `S1-S3` 全通过
- 失败分支全部输出类型化失败语义
- 不可接受状态被显式拦截
- `sim_observer` 回归测试与 smoke 检查不回退

## 场景矩阵

| Case ID | 链路 | 关注点 | 通过判定 |
| --- | --- | --- | --- |
| `P0-S1-01` | 摘要 | 默认入口 | 首个 `resolved` issue 被自动选中；状态栏为 `mode=resolved` |
| `P0-S1-02` | 摘要 | 详情一致性 | 摘要切换后，详情同步刷新并展示 `SourceRefs` 摘要 |
| `P0-S2-01` | 画布 | 失败保护 | `summary_only` / 失败态不出现伪对象高亮 |
| `P0-S2-02` | 画布 | 组落点 | `object_group` 维持组语义，不伪装为单对象 |
| `P0-S3-01` | 结构 | 当前对象聚焦 | 点击当前主对象或组成员时不切换摘要 |
| `P0-S3-02` | 结构 | 唯一回接 | 点击唯一命中对象时自动回接到该 issue |
| `P0-S3-03` | 结构 | 多命中保护 | 多 issue 命中时不自动切换摘要，并显式提示冲突 |
| `P0-F-01` | 空态 | 无 recording | 四视图进入统一空态 |
| `P0-F-02` | 空态 | 无 issue | 摘要显示无入口，画布/结构只保留路径事实 |
| `P0-R-01` | 红线 | 不可接受状态 | 未知状态或非法渲染状态被显式拦截，测试失败而非静默继续 |

## 自动化入口

执行命令：

```powershell
python -m unittest discover -s D:\Projects\SiligenSuite\apps\hmi-app\tests\unit\sim_observer -p "test_*.py"
python -m unittest D:\Projects\SiligenSuite\apps\hmi-app\tests\unit\test_smoke.py
python -m py_compile D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\ui\main_window.py D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\features\sim_observer\ui\p0_workspace.py
```

场景自动化对应：

- `P0-S1-01` 至 `P0-R-01`：`apps/hmi-app/tests/unit/sim_observer/test_p0_acceptance.py`
- `ObserverStore` 基础契约与回归：`apps/hmi-app/tests/unit/sim_observer/test_observer_store.py`
- 规则层回归：`apps/hmi-app/tests/unit/sim_observer/test_rule_r1_primary_association.py`、`test_rule_r2_minimize_group.py`

## 验收记录

发布前只记录三项：

- 执行日期
- 自动化结果：`通过 / 阻塞`
- 阻塞 case 与原因
