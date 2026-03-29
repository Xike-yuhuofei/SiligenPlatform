# Phase 1 Data Model: HMI 胶点预览锚定分布一致性

## 1. 模型目标

本数据模型描述的是“同一份权威出胶结果如何同时支撑 preview snapshot 与 execution package，并在几何锚定、共享顶点、短线段例外和执行门禁上保持一致”。它不是数据库设计，而是为 planning owner logic、workflow gate、gateway 协议和 HMI 主预览提供统一语义。

## 2. 实体总览

| 实体 | 作用 | 核心关系 |
|---|---|---|
| `AuthorityPreviewPlan` | 表示一次可进入 preview/execution 门禁的权威计划结果 | 聚合 `GeometricDispenseSegment`、`TriggerAuthorityPoint`、`PreviewSnapshotPayload` |
| `GeometricDispenseSegment` | 表示一条实际参与点胶分布的几何线段 | 生成多个 `TriggerAuthorityPoint`，并对应一个 `SpacingValidationGroup` |
| `TriggerAuthorityPoint` | 表示一个写回到权威轨迹点列中的显式出胶点 | 同时服务于 preview 与 execution |
| `SpacingValidationGroup` | 表示一次按真实几何分组的 spacing 复核单元 | 对应一个或多个 `GeometricDispenseSegment` |
| `PreviewSnapshotPayload` | 表示 `planned_glue_snapshot + glue_points` 主预览负载 | 消费 `TriggerAuthorityPoint`，并受 `ExecutionLaunchGate` 约束 |
| `ExecutionLaunchGate` | 表示当前计划是否允许继续执行的门禁状态 | 依赖 `AuthorityPreviewPlan` 与 `PreviewSnapshotPayload` |

## 3. 实体定义

### 3.1 `AuthorityPreviewPlan`

**说明**: 当前图形与当前计划下，唯一允许进入 preview 与 execution 门禁的权威计划结果。

| 字段 | 类型 | 约束 |
|---|---|---|
| `plan_id` | string | 唯一 |
| `plan_fingerprint` | string | 必填；用于 preview 与 execution 对齐 |
| `artifact_ref` | string | 指向当前图形/工件 |
| `authority_origin` | enum | 固定为 `explicit-triggered-interpolation` |
| `preview_source` | enum | 固定为 `planned_glue_snapshot` |
| `preview_kind` | enum | 固定为 `glue_points` |
| `derived_trigger_distances_mm` | list[number] | 可为空；仅为派生兼容产物，不是 authority source |
| `legacy_conversion_allowed` | boolean | 固定为 `false` |
| `state` | enum | `prepared`, `snapshot_ready`, `confirmed`, `blocked`, `stale` |

**规则**

- preview 与 execution 必须消费同一个 `AuthorityPreviewPlan`。
- 若缺少可用 `TriggerAuthorityPoint`，该计划必须转入 `blocked`。
- `derived_trigger_distances_mm` 只允许作为导出或兼容字段存在，不得反向决定 preview 主结果。

### 3.2 `GeometricDispenseSegment`

**说明**: 每条参与点胶分布的实际几何线段，是首尾双锚定与间距判定的基本单元。

| 字段 | 类型 | 约束 |
|---|---|---|
| `segment_id` | string | 唯一 |
| `plan_ref` | string | 引用 `AuthorityPreviewPlan.plan_id` |
| `contour_id` | string | 必填 |
| `source_order` | integer | 必须 >= 0；保持执行顺序 |
| `start_point` | point2d | 必填 |
| `end_point` | point2d | 必填 |
| `length_mm` | number | 必须 >= 0 |
| `dispense_enabled` | boolean | 只有 `true` 时才参与布点 |
| `shared_start_vertex_key` | string/null | 可空；共享顶点时必填 |
| `shared_end_vertex_key` | string/null | 可空；共享顶点时必填 |
| `short_segment_exception` | boolean | 由锚定后实际间距是否落出窗口决定 |

**规则**

- `dispense_enabled=false` 的线段不得生成 `TriggerAuthorityPoint`。
- `short_segment_exception=true` 并不表示失败，只表示该段必须按例外口径复核。
- `source_order` 不能因重分布而改变。

### 3.3 `TriggerAuthorityPoint`

**说明**: 被写回权威轨迹点列并同时被 preview 与 execution 消费的显式出胶点。

| 字段 | 类型 | 约束 |
|---|---|---|
| `trigger_id` | string | 唯一 |
| `plan_ref` | string | 引用 `AuthorityPreviewPlan.plan_id` |
| `segment_ref` | string | 引用 `GeometricDispenseSegment.segment_id` |
| `sequence_index` | integer | 必须 >= 0 |
| `position` | point2d | 必填 |
| `anchor_role` | enum | `segment_start`, `segment_end`, `interior`, `shared_vertex` |
| `enable_position_trigger` | boolean | 固定为 `true` |
| `interpolation_index` | integer/null | 可空；写回插补点列时使用 |
| `trigger_position_mm` | number/null | 可空；仅作派生参考 |

**规则**

- `TriggerAuthorityPoint` 必须在几何上对应到 `GeometricDispenseSegment` 的合法位置。
- 共享顶点去重后，只允许保留一个公共 `position`。
- preview snapshot 和 execution package 必须消费同一批 `TriggerAuthorityPoint`。

### 3.4 `SpacingValidationGroup`

**说明**: 对权威胶点结果进行 spacing 复核时的真实几何分组。

| 字段 | 类型 | 约束 |
|---|---|---|
| `group_id` | string | 唯一 |
| `plan_ref` | string | 引用 `AuthorityPreviewPlan.plan_id` |
| `segment_refs` | list[string] | 至少一个 `GeometricDispenseSegment` |
| `trigger_refs` | list[string] | 至少两个 `TriggerAuthorityPoint` 才产生 spacing |
| `target_spacing_mm` | number | 固定为 `3.0` |
| `min_spacing_mm` | number | 固定为 `2.7` |
| `max_spacing_mm` | number | 固定为 `3.3` |
| `is_short_segment_exception` | boolean | 必填 |
| `within_window` | boolean | 必填 |

**规则**

- 分组边界必须反映真实几何分布来源，不能跨不相邻轮廓或跳跃路径拼接。
- `is_short_segment_exception=true` 时，可允许 `within_window=false`，但必须显式记录。

### 3.5 `PreviewSnapshotPayload`

**说明**: 发送给 gateway/HMI 的 `planned_glue_snapshot` 主预览负载。

| 字段 | 类型 | 约束 |
|---|---|---|
| `snapshot_id` | string | 唯一 |
| `snapshot_hash` | string | 必填 |
| `plan_ref` | string | 引用 `AuthorityPreviewPlan.plan_id` |
| `preview_state` | enum | `snapshot_ready`, `confirmed`, `failed`, `stale` |
| `preview_source` | enum | 固定为 `planned_glue_snapshot` |
| `preview_kind` | enum | 固定为 `glue_points` |
| `glue_points` | list[point2d] | 必须非空；来源于 `TriggerAuthorityPoint.position` |
| `execution_polyline` | list[point2d] | 辅助层，可采样 |
| `segment_count` | integer | 必须 >= 0 |
| `execution_point_count` | integer | 必须 >= 0 |
| `generated_at` | datetime | 必填 |
| `confirmed_at` | datetime/null | 可空 |

**规则**

- `glue_points` 是主预览语义；`execution_polyline` 只用于核对走向。
- `snapshot_hash` 与 `plan_fingerprint` 必须可关联。
- 当 `glue_points` 为空、来源不合规或 authority 缺失时，payload 不得进入成功路径。

### 3.6 `ExecutionLaunchGate`

**说明**: 当前计划是否允许继续执行的门禁状态。

| 字段 | 类型 | 约束 |
|---|---|---|
| `gate_id` | string | 唯一 |
| `plan_ref` | string | 引用 `AuthorityPreviewPlan.plan_id` |
| `snapshot_ref` | string | 引用 `PreviewSnapshotPayload.snapshot_id` |
| `preview_confirmed` | boolean | 必填 |
| `source_valid` | boolean | 只有 `planned_glue_snapshot` 为 `true` |
| `authority_shared` | boolean | preview 与 execution 共用同一权威结果时为 `true` |
| `start_allowed` | boolean | 必填 |
| `block_reason` | string/null | 阻止执行时必填 |

**规则**

- 只有 `preview_confirmed=true`、`source_valid=true`、`authority_shared=true` 时，`start_allowed` 才允许为 `true`。
- 若 preview 生成失败、来源不合规或未通过当前规则校验，`start_allowed` 必须为 `false`。

## 4. 关系约束

```text
AuthorityPreviewPlan
├── 1..n GeometricDispenseSegment
├── 1..n TriggerAuthorityPoint
├── 1..n SpacingValidationGroup
├── 1..1 PreviewSnapshotPayload
└── 1..1 ExecutionLaunchGate

GeometricDispenseSegment
└── 1..n TriggerAuthorityPoint

SpacingValidationGroup
├── 1..n GeometricDispenseSegment
└── 2..n TriggerAuthorityPoint

PreviewSnapshotPayload
└── 消费全部权威 TriggerAuthorityPoint 的 preview 坐标

ExecutionLaunchGate
└── 依赖 PreviewSnapshotPayload 与 AuthorityPreviewPlan 的一致性
```

## 5. 状态迁移

### 5.1 `AuthorityPreviewPlan`

```text
prepared -> snapshot_ready -> confirmed
prepared -> blocked
snapshot_ready -> stale
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

一次计划胶点预览只有在以下条件全部满足时，才允许成为执行前依据：

1. `AuthorityPreviewPlan.authority_origin = explicit-triggered-interpolation`
2. `PreviewSnapshotPayload.preview_source = planned_glue_snapshot`
3. `PreviewSnapshotPayload.glue_points` 非空，且来自 `TriggerAuthorityPoint`
4. `TriggerAuthorityPoint` 在几何上与应出胶端点、角点、共享顶点保持当前澄清后的重合口径
5. `SpacingValidationGroup` 对非例外线段全部满足 `2.7-3.3 mm` 窗口
6. `ExecutionLaunchGate.start_allowed = true`
