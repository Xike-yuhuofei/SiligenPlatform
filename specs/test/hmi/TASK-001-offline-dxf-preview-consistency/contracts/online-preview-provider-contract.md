# Contract: 在线权威预览提供者

## 1. 目的

定义 HMI 在 `online` 模式且机台已经准备就绪时，获取“执行对齐的权威预览”必须满足的输入/输出语义。该契约不替代 `shared/contracts/application/commands/dxf.command-set.json` 的正式协议，而是把在线真实预览验收必须满足的调用前提、响应约束和失败边界冻结下来。

## 2. 边界

- HMI 启动模式：`online`
- 仅当 Supervisor 已达到 `online_ready` 且机台准备就绪时，当前预览才允许进入真实验收
- 允许 gateway / TCP / 硬件链路参与
- 输出必须能证明与上位机准备下发到运动控制卡的数据语义一致，而不是仅生成几何可视化

## 3. 前置调用序列

在线权威预览链必须具备以下最小序列：

1. `dxf.artifact.create` 或等效真实 DXF 装载步骤，生成当前 `artifact_id`
2. `dxf.plan.prepare`，生成当前 `plan_id` 与 `plan_fingerprint`
3. `dxf.preview.snapshot`，基于同一 `plan_id` 返回当前预览快照

规则：

1. 输入 DXF 必须是真实 DXF 文件，不允许 synthetic / mock 几何替代。
2. HMI 必须保存 `plan_id` 与 `plan_fingerprint`，用于后续与控制卡下发准备数据做一致性追溯。
3. 若当前在线就绪前提失效，预览链必须失败，而不是回退旧缓存结果。

## 4. 快照请求语义

`dxf.preview.snapshot` 的最小请求字段：

| 字段 | 必填 | 说明 |
|---|---|---|
| `plan_id` | 是 | 来自同一上下文中的 `dxf.plan.prepare` |
| `max_polyline_points` | 否 | HMI 渲染点数上限；若提供，必须 >= 2 |

## 5. 快照响应语义

在线权威预览提供者至少必须返回以下字段：

| 字段 | 必填 | 说明 |
|---|---|---|
| `snapshot_id` | 是 | 预览快照标识 |
| `snapshot_hash` | 是 | 当前快照签名 |
| `plan_id` | 是 | 对应执行准备标识 |
| `preview_state` | 是 | 例如 `snapshot_ready` |
| `preview_source` | 是 | 真实验收必须为 `runtime_snapshot` |
| `confirmed_at` | 是 | 当前快照确认时间 |
| `segment_count` | 是 | 段数 |
| `point_count` | 是 | 原始规划点数 |
| `polyline_point_count` | 是 | 输出点数 |
| `polyline_source_point_count` | 是 | 输出前点数 |
| `trajectory_polyline` | 是 | HMI 用于渲染的点列 |
| `total_length_mm` | 是 | 总长度 |
| `estimated_time_s` | 是 | 预估时间 |
| `generated_at` | 是 | 生成时间 |

## 6. 语义规则

1. `preview_source` 必须显式存在；真实验收只接受 `runtime_snapshot`。
2. 当前快照必须与同一 `plan_id` 关联；HMI 必须保留来自 `dxf.plan.prepare` 的 `plan_fingerprint`，用于证明它与待下发到运动控制卡的数据属于同一执行准备上下文。
3. `trajectory_polyline` 可以为了 UI 展示而采样，但其几何路径、路径顺序和点胶相关运动语义不得与执行准备结果漂移。
4. 当提供者无法证明当前结果对应当前 DXF、当前 plan 或当前在线就绪上下文时，必须返回失败，而不是回退历史结果或其他渲染链。
5. `mock_synthetic`、历史缓存点列、脱离当前在线上下文的 HTML 预览都不能作为本契约的成功输出。

## 7. 与现有正式协议的关系

| 维度 | 正式协议 | 本文件 |
|---|---|---|
| 协议源 | `shared/contracts/application/commands/dxf.command-set.json` | 在线真实预览验收补充约束 |
| 权威快照入口 | `dxf.preview.snapshot` | 同一入口 |
| 可接受来源 | `runtime_snapshot` | `runtime_snapshot` |
| 禁止来源 | `mock_synthetic`、空来源、不可追溯来源 | 同左，并补充“非当前在线就绪上下文” |

规则：

1. 本文件不得改写正式协议字段含义，只能补充在线真实验收所需的约束。
2. 如需补充额外验收字段，只能通过证据包扩展，不得隐式篡改 `dxf.command-set.json` 既有语义。

## 8. 失败条件

出现以下任一情况时，在线权威预览请求必须失败：

1. `launch_mode != online`
2. 当前上下文未达到 `online_ready`
3. `plan_id` 缺失，或无法回链到同一 `dxf.plan.prepare` 结果
4. `preview_source` 为空、为 `unknown`、为 `mock_synthetic` 或不属于当前 DXF / 当前 plan
5. 无法证明结果与准备下发到运动控制卡的数据语义一致
