# Reliability Baseline

更新时间：`2026-03-18`

## 1. 目的

本文档用于把当前重构后系统的关键流程可靠性基线固化下来。这里的“可靠性”不只统计成功/失败，还要求把失败与错误类型绑定，便于后续回归判断到底是协议缺口、启动问题还是环境问题。

## 2. 采样环境与来源

- 工作区：`D:\Projects\SiligenSuite`
- 平台：`Windows-11-10.0.26100-SP0`
- Python：`3.12.1`
- 自动化采样脚本：`integration/performance/collect_baselines.py`
- 原始结果：`integration/reports/performance/2026-03-18-baseline.json`
- 历史参照：`integration/reports/field-acceptance/2026-03-18-field-acceptance-summary.md`

本次自动化采样次数：

- mock 关键流程：`20`
- 已知缺失 RPC：`5`
- HIL smoke：`3`

## 3. 当前可靠性基线

| 流程 | 采样数 | 失败数 | 失败率 | 当前错误分类 | 可信度 |
|---|---:|---:|---:|---|---|
| `mock_startup_and_dxf_cycle` | `20` | `0` | `0.0000` | `none` | `high` |
| `dxf.resume` | `5` | `5` | `1.0000` | `protocol.unsupported_method` | `medium` |
| `recipe.list` | `5` | `5` | `1.0000` | `protocol.unsupported_method` | `medium` |
| `hardware_smoke_hil` | `3` | `0` | `0.0000` | `none` | `medium` |

## 4. 采样方法

### 4.1 Mock 关键流程

流程定义：

1. `ping`
2. `connect_hardware`
3. `home`
4. `move_to`
5. `dxf_load`
6. `dxf_execute(dry_run)`
7. `dxf_get_progress`

测量口径：

- 每轮任一步骤失败即计为该轮失败。
- 用 `apps/hmi-app/src/hmi_client/tools/mock_server.py` 作为稳定 backend。
- 该指标用于验证“重构后基础链路仍然可用”，不是现场硬件可靠性替代品。

当前结论：

- `20/20` 全部成功，说明 mock 场景下的启动、运动、DXF 基本链路当前稳定。

### 4.2 已知缺失 RPC

测量方法：

- 直接对 TCP backend 发送 `dxf.resume` 和 `recipe.list`
- 每个方法采样 `5` 次
- 只要响应里包含 `Unknown method` 就归类为 `protocol.unsupported_method`

当前结论：

- `dxf.resume`：`5/5` 失败，失败率 `100%`
- `recipe.list`：`5/5` 失败，失败率 `100%`
- 两者都属于“协议功能未实现”，不是偶发不稳定

### 4.3 HIL Smoke

测量方法：

- 直接执行 `integration/hardware-in-loop/run_hardware_smoke.py`
- 采样 `3` 次
- 以脚本返回码和输出内容判断是否存在启动类错误

当前结论：

- 本次自动化采样为 `0/3` 失败
- 因为样本数小，且 HIL 高度依赖当前 build/环境状态，所以只标记为 `medium`

## 5. 错误分类基线

本次自动化采样中真正出现的错误类别只有一类：

| 错误分类 | 次数 | 对应流程 | 含义 |
|---|---:|---|---|
| `protocol.unsupported_method` | `10` | `dxf.resume`、`recipe.list` | 当前 TCP backend 不支持该 RPC，属于功能缺口而非偶发超时 |

本轮未观察到但需要保留的分类：

- `startup.diagnostics_port_unregistered`
- `startup.backend_timeout`
- `transport.timeout`
- `transport.connection_refused`

这些分类仍保留在采样脚本里，后续只要再次出现，会自动归档到同一错误桶。

## 6. 不可比较项

- `mock_startup_and_dxf_cycle` 的 `0%` 失败率只对 mock backend 有效，不能外推到真实硬件。
- `hardware_smoke_hil` 当前 `0/3` 成功，不代表现场可靠性已稳定，只能说明本机当前构建+环境下 smoke 可达。
- `dxf.resume` 与 `recipe.list` 的 `100%` 失败属于“功能未实现”，未来一旦功能补齐，历史失败率将失去横向可比性。

## 7. 同日历史差异说明

需要明确记录一个同日差异：

- `integration/reports/field-acceptance/2026-03-18-field-acceptance-summary.md` 记录的是 `2026-03-18` 当天更早的一次验收，当时 `HIL` 状态为 `known_failure`，错误是 `IDiagnosticsPort 未注册`。
- 本次基线采样生成时间是 `2026-03-18T10:19:55.619514+00:00`，自动化 `hardware_smoke_hil` 结果为 `0/3` 失败。

结论：

- 这两组结果不能直接比较。
- 更合理的解释是：同一天内 build 或运行环境已经变化，导致 HIL 启动表现发生了变化。
- 后续如果要把 HIL 作为长期基线，必须固定构建产物、依赖和采样前置条件。

## 8. 当前自动化缺口与手工测量方式

当前尚未自动纳入的可靠性项：

- 真实硬件长稳运行失败率
- 真实告警恢复流程成功率
- HMI 首帧/UI 自动化稳定性

建议的手工或后续自动化方式：

1. 真实硬件长稳：沿用 field acceptance 的 soak 思路，至少记录 `60 min` 内循环次数、失败次数、错误分类。
2. 告警恢复：对 `estop -> acknowledge -> clear -> recover` 形成固定脚本，并对每一步单独归类。
3. UI 稳定性：复用 `apps/hmi-app/src/hmi_client/tools/ui_qtest.py`，把退出码、超时、渲染失败单独分类，而不是混成一个“失败”。

## 9. 后续监控建议

- 把 `protocol.unsupported_method` 作为显式阻塞类错误统计，而不是隐藏在通用失败里。
- 把 HIL smoke 拆成“端口可达”“协议握手成功”“基础命令成功”三级指标，避免只有一个总结果。
- 对真实硬件和 mock 分开维护可靠性基线，禁止混用同一失败率。
- 每次发布候选版本都保留一份日期版可靠性报告，便于追踪某一类错误何时被引入或修复。
