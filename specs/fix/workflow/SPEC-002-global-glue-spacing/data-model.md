# Phase 1 Data Model: 胶点全局均布与真值链一致性

## 1. 模型目标

本数据模型描述的是“连续点胶路径的全局胶点布局如何同时支撑预览、校验与执行准备”。它不是数据库设计，而是用于约束 `modules/dispense-packaging` 的 authority layout、`modules/workflow` 的 preview/execution gate，以及 `dxf.preview.snapshot` 的主预览语义。

## 2. 实体总览

| 实体 | 作用 | 核心关系 |
|---|---|---|
| `AuthorityTriggerLayout` | 当前计划唯一被认可的权威胶点布局 | 聚合 `DispenseSpan`、`LayoutTriggerPoint`、`SpacingValidationOutcome`、`InterpolationTriggerBinding` |
| `DispenseSpan` | 一条连续点胶路径的布局单元 | 生成多个 `LayoutTriggerPoint`，对应一个或多个 `SpacingValidationOutcome` |
| `StrongAnchor` | 业务上必须保留的边界点或关键点 | 附属于 `DispenseSpan`，影响布局求解 |
| `LayoutTriggerPoint` | 权威胶点布局中的单个胶点 | 同时服务于预览、校验与执行绑定 |
| `InterpolationTriggerBinding` | authority point 到执行插补点的单调绑定 | 将 `LayoutTriggerPoint` 映射到 execution carrier |
| `SpacingValidationOutcome` | 对 span/group 的间距复核结论 | 决定门禁是通过、例外通过还是失败 |
| `PreviewSnapshotPayload` | 对外 `planned_glue_snapshot + glue_points` 预览负载 | 直接消费 `LayoutTriggerPoint` |
| `ExecutionLaunchGate` | 当前计划是否允许继续执行的门禁状态 | 依赖 `AuthorityTriggerLayout`、`SpacingValidationOutcome` 与 `PreviewSnapshotPayload` |

## 3. 实体定义

### 3.1 `AuthorityTriggerLayout`

**说明**: 当前图形与当前计划下，唯一允许进入 preview 和 execution gate 的胶点布局真值。

| 字段 | 类型 | 约束 |
|---|---|---|
| `layout_id` | string | 唯一 |
| `plan_id` | string | 唯一；绑定当前计划 |
| `plan_fingerprint` | string | 必填；用于 preview confirm / start job 对齐 |
| `origin` | enum | 固定为 `continuous-span-layout` |
| `preview_source` | enum | 固定为 `planned_glue_snapshot` |
| `preview_kind` | enum | 固定为 `glue_points` |
| `target_spacing_mm` | number | 默认 `3.0` |
| `min_spacing_mm` | number | 默认 `2.7` |
| `max_spacing_mm` | number | 默认 `3.3` |
| `state` | enum | `layout_ready`, `binding_ready`, `snapshot_ready`, `confirmed`, `blocked`, `stale` |

**规则**

- 同一计划只允许存在一份当前有效的 `AuthorityTriggerLayout`。
- preview 与 execution 必须消费同一个 `layout_id`。
- 当任一 span 产生阻塞性布局失败或绑定失败时，`state` 必须进入 `blocked`。

### 3.2 `DispenseSpan`

**说明**: 一条连续点胶路径的布局单元，可能是开放路径，也可能是闭合路径。

| 字段 | 类型 | 约束 |
|---|---|---|
| `span_id` | string | 唯一 |
| `layout_ref` | string | 引用 `AuthorityTriggerLayout.layout_id` |
| `contour_id` | string | 必填 |
| `source_segment_refs` | list[string] | 至少一个源 segment |
| `order_index` | integer | 必须 >= 0；决定跨 span 顺序 |
| `closed` | boolean | 必填 |
| `total_length_mm` | number | 必须 > 0 |
| `actual_spacing_mm` | number | 必须 > 0 |
| `interval_count` | integer | 必须 >= 1 |
| `phase_mm` | number | 开放路径固定为 `0`；闭合路径可 > 0 |
| `curve_mode` | enum | `line`, `arc`, `mixed`, `flattened-curve` |
| `validation_state` | enum | `pass`, `pass_with_exception`, `fail` |

**规则**

- 几何上等价但内部切分不同的输入，必须生成等价的 `DispenseSpan` 布局结果。
- `closed=true` 的 span 在无业务强锚点时不得依赖固定几何起点。
- `validation_state=pass_with_exception` 表示允许例外，不等同于失败。

### 3.3 `StrongAnchor`

**说明**: 在布局求解中必须保留的业务关键点。

| 字段 | 类型 | 约束 |
|---|---|---|
| `anchor_id` | string | 唯一 |
| `span_ref` | string | 引用 `DispenseSpan.span_id` |
| `role` | enum | `open_start`, `open_end`, `process_boundary`, `explicit_feature` |
| `position` | point2d | 必填 |
| `required` | boolean | 固定为 `true` |

**规则**

- 普通内部切分顶点不得自动升格为 `StrongAnchor`。
- 闭合路径在没有明确业务约束时允许 `StrongAnchor` 为空。

### 3.4 `LayoutTriggerPoint`

**说明**: 权威胶点布局中的单个胶点，也是 preview 主语义与 execution 绑定源。

| 字段 | 类型 | 约束 |
|---|---|---|
| `trigger_id` | string | 唯一 |
| `layout_ref` | string | 引用 `AuthorityTriggerLayout.layout_id` |
| `span_ref` | string | 引用 `DispenseSpan.span_id` |
| `sequence_index_global` | integer | 必须 >= 0 |
| `sequence_index_span` | integer | 必须 >= 0 |
| `distance_mm_global` | number | 必须 >= 0 |
| `distance_mm_span` | number | 必须 >= 0 |
| `position` | point2d | 必填 |
| `source_kind` | enum | `anchor`, `generated`, `shared_vertex` |
| `preview_visible` | boolean | 固定为 `true` |

**规则**

- `PreviewSnapshotPayload.glue_points` 必须按 `sequence_index_global` 顺序直接投影自 `LayoutTriggerPoint.position`。
- 若多个源 segment 共享同一物理顶点，只允许存在一个 `source_kind=shared_vertex` 的公共点。
- 相同输入重复布局时，`sequence_index_global` 与 `position` 必须稳定。

### 3.5 `InterpolationTriggerBinding`

**说明**: authority points 到 execution 插补点的绑定结果。

| 字段 | 类型 | 约束 |
|---|---|---|
| `binding_id` | string | 唯一 |
| `layout_ref` | string | 引用 `AuthorityTriggerLayout.layout_id` |
| `trigger_ref` | string | 引用 `LayoutTriggerPoint.trigger_id` |
| `interpolation_index` | integer | 必须 >= 0 |
| `execution_position` | point2d | 必填 |
| `match_error_mm` | number | 必须 >= 0 |
| `monotonic` | boolean | 必填 |
| `bound` | boolean | 必填 |

**规则**

- 所有 `LayoutTriggerPoint` 必须形成单调绑定；不得一对多或多对一混绑。
- 若任一 `LayoutTriggerPoint` 无法绑定到 execution carrier，布局状态必须进入 `blocked`。

### 3.6 `SpacingValidationOutcome`

**说明**: 真实几何分组上的间距复核结论。

| 字段 | 类型 | 约束 |
|---|---|---|
| `outcome_id` | string | 唯一 |
| `layout_ref` | string | 引用 `AuthorityTriggerLayout.layout_id` |
| `span_ref` | string | 引用 `DispenseSpan.span_id` |
| `trigger_refs` | list[string] | 至少两个点才产生间距复核 |
| `classification` | enum | `pass`, `pass_with_exception`, `fail` |
| `min_observed_spacing_mm` | number | 必填 |
| `max_observed_spacing_mm` | number | 必填 |
| `exception_reason` | string/null | `pass_with_exception` 时必填 |
| `blocking_reason` | string/null | `fail` 时必填 |

**规则**

- 分组边界必须反映真实连续路径，不能跨不相连轮廓拼接。
- `classification=pass_with_exception` 时允许超出常规窗口，但必须说明原因。
- `classification=fail` 才能直接阻塞 `ExecutionLaunchGate`。

### 3.7 `PreviewSnapshotPayload`

**说明**: 发送给 gateway/HMI 的主预览负载。

| 字段 | 类型 | 约束 |
|---|---|---|
| `snapshot_id` | string | 唯一 |
| `snapshot_hash` | string | 必填 |
| `plan_ref` | string | 引用 `AuthorityTriggerLayout.plan_id` |
| `layout_ref` | string | 引用 `AuthorityTriggerLayout.layout_id` |
| `preview_state` | enum | `snapshot_ready`, `confirmed`, `failed`, `stale` |
| `preview_source` | enum | 固定为 `planned_glue_snapshot` |
| `preview_kind` | enum | 固定为 `glue_points` |
| `glue_points` | list[point2d] | 必须非空；直接来自 `LayoutTriggerPoint.position` |
| `execution_polyline` | list[point2d] | 可采样；仅作辅助层 |
| `generated_at` | datetime | 必填 |
| `confirmed_at` | datetime/null | 可空 |

**规则**

- `glue_points` 是主语义，`execution_polyline` 只用于辅助展示。
- 只要 `glue_points` 为空、来源不合规或 `layout_ref` 缺失，payload 就不得进入成功路径。

### 3.8 `ExecutionLaunchGate`

**说明**: 当前计划是否允许继续执行的门禁状态。

| 字段 | 类型 | 约束 |
|---|---|---|
| `gate_id` | string | 唯一 |
| `plan_ref` | string | 引用 `AuthorityTriggerLayout.plan_id` |
| `layout_ref` | string | 引用 `AuthorityTriggerLayout.layout_id` |
| `snapshot_ref` | string | 引用 `PreviewSnapshotPayload.snapshot_id` |
| `authority_ready` | boolean | 必填 |
| `binding_ready` | boolean | 必填 |
| `validation_state` | enum | `pass`, `pass_with_exception`, `fail` |
| `preview_confirmed` | boolean | 必填 |
| `start_allowed` | boolean | 必填 |
| `block_reason` | string/null | `start_allowed=false` 时必填 |

**规则**

- `authority_ready=true`、`binding_ready=true`、`preview_confirmed=true`，且 `validation_state != fail` 时，`start_allowed` 才允许为 `true`。
- `pass_with_exception` 允许继续执行，但必须保留例外说明。

## 4. 关系约束

```text
AuthorityTriggerLayout
├── 1..n DispenseSpan
├── 0..n StrongAnchor
├── 1..n LayoutTriggerPoint
├── 1..n SpacingValidationOutcome
├── 1..n InterpolationTriggerBinding
├── 1..1 PreviewSnapshotPayload
└── 1..1 ExecutionLaunchGate

DispenseSpan
├── 0..n StrongAnchor
├── 1..n LayoutTriggerPoint
└── 1..n SpacingValidationOutcome

PreviewSnapshotPayload
└── 直接消费按全局顺序排序后的 LayoutTriggerPoint

ExecutionLaunchGate
└── 依赖 AuthorityTriggerLayout、SpacingValidationOutcome 与 PreviewSnapshotPayload 一致性
```

## 5. 状态迁移

### 5.1 `AuthorityTriggerLayout`

```text
layout_ready -> binding_ready -> snapshot_ready -> confirmed
layout_ready -> blocked
binding_ready -> blocked
snapshot_ready -> blocked
confirmed -> stale
```

### 5.2 `ExecutionLaunchGate`

```text
blocked -> ready
ready -> consumed
ready -> blocked
```

## 6. 完成判定闭环

一次胶点预览只有在以下条件全部满足时，才允许成为执行前依据：

1. `AuthorityTriggerLayout.origin = continuous-span-layout`
2. `PreviewSnapshotPayload.preview_source = planned_glue_snapshot`
3. `PreviewSnapshotPayload.preview_kind = glue_points`
4. `PreviewSnapshotPayload.glue_points` 非空，且逐点来自 `LayoutTriggerPoint.position`
5. 所有 `InterpolationTriggerBinding` 都已成功并保持单调
6. `SpacingValidationOutcome` 不存在 `fail`
7. `ExecutionLaunchGate.start_allowed = true`
