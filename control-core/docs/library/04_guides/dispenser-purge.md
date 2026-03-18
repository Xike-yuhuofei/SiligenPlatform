---
Owner: @Xike
Status: active
Last reviewed: 2026-03-17
Scope: 排胶流程 / 胶路建压
---

# 排胶流程与参数说明

本文记录 `control-core` 当前排胶能力的权威入口、参数约束和使用边界。

## 1) 当前权威实现

- Application 入口：`src/application/usecases/dispensing/valve/ValveCommandUseCase.cpp`
- Domain 流程：`src/domain/dispensing/domain-services/PurgeDispenserProcess.h`
- 稳压策略：`src/domain/dispensing/domain-services/SupplyStabilizationPolicy.h`
- 阀门端口：`src/domain/dispensing/ports/IValvePort.h`

当前实现已经采用“领域层统一建压/稳压 + Application 仅编排调用”的结构，不再以 `docs/plans/` 中的方案稿作为实施依据。

## 2) 适用场景

- 开机预排胶
- 换胶、换针头后的排气
- 短暂停机后的胶路恢复
- 现场手动排胶

非目标：

- 不负责运动定位；排胶位置应由外部流程或人工先行确认。
- 不负责流量闭环控制；当前不依赖称重或流量传感器。

## 3) 当前请求参数

`PurgeDispenserRequest` 定义于 `src/domain/dispensing/domain-services/PurgeDispenserProcess.h`。

| 参数 | 默认值 | 说明 |
|---|---:|---|
| `manage_supply` | `true` | 是否由流程自动打开/关闭供胶阀 |
| `wait_for_completion` | `true` | 是否阻塞等待排胶结束 |
| `supply_stabilization_ms` | `0` | 供胶稳压覆盖值；`0` 表示使用配置默认值 |
| `poll_interval_ms` | `50` | 状态轮询间隔，范围 `10-1000ms` |
| `timeout_ms` | `30000` | 超时时间，范围 `100-600000ms` |

额外约束：

- 当 `manage_supply=true` 且 `supply_stabilization_ms>0` 时，覆盖值必须在 `0-5000ms`。
- 当 `wait_for_completion=false` 时，调用方必须承担后续停止点胶阀和关闭供胶阀的责任。

## 4) 执行顺序

当前排胶流程由 `PurgeDispenserProcess::Execute()` 统一处理：

1. 校验参数范围
2. 视请求决定是否打开供胶阀
3. 通过 `SupplyStabilizationPolicy` 解析稳压等待
4. 连续打开点胶阀
5. 如需等待完成，则轮询 `GetDispenserStatus()`
6. 异常或超时场景下关闭点胶阀，并在托管供胶时关闭供胶阀

## 5) 现场使用边界

- 排胶前必须确认喷嘴处于安全位置，并准备回收容器。
- 排胶不应在运动执行中的工艺流程里临时插入。
- 手动按键排胶需参考 `docs/library/06_reference.md` 中的 `X2` 排胶键定义。

## 6) 与其他文档的关系

- 参数、IO、按键映射：`docs/library/06_reference.md`
- 现场排障：`docs/library/05_runbook.md`
- 历史方案稿：`docs/_archive/2026-03/plans-triage/2026-01-19-purge-dispenser-usecase-plan.md`
