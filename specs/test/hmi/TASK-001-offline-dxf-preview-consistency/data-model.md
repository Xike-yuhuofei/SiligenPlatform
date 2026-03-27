# Phase 1 Data Model: HMI 在线点胶轨迹预览一致性验证

## 1. 模型目标

本数据模型描述的是“在线就绪前提下的预览请求、runtime 权威快照、与控制卡下发准备语义的对齐记录，以及验收证据”。它不是数据库设计，而是为 HMI 在线预览功能、协议契约、自动化回归和人工验收追溯提供统一语义。

## 2. 实体总览

| 实体 | 作用 | 核心关系 |
|---|---|---|
| `OnlinePreviewRequest` | 表示一次在线预览请求的输入边界 | 生成 `RuntimePreviewSnapshot` |
| `OnlineReadyCapabilityState` | 描述 HMI / Supervisor 是否到达在线就绪状态 | 约束 `OnlinePreviewRequest` 是否可发起 |
| `RealDxfArtifact` | 表示被验收的真实 DXF 输入 | 被 `OnlinePreviewRequest` 和 `PreviewEvidenceBundle` 引用 |
| `RuntimePreviewSnapshot` | 表示 runtime/gateway 返回的权威在线预览快照 | 从 `OnlinePreviewRequest` 生成，并关联 `ExecutionDispatchAlignmentRecord` |
| `ExecutionDispatchAlignmentRecord` | 记录快照与上位机准备下发到运动控制卡的数据之间的语义对齐关系 | 约束 `RuntimePreviewSnapshot` 和 `PreviewEvidenceBundle` |
| `PreviewSourceClassification` | 冻结预览来源的验收语义 | 约束 `RuntimePreviewSnapshot.preview_source` |
| `PreviewEvidenceBundle` | 聚合在线预览验收所需的全部证据文件与结论 | 引用 `RealDxfArtifact`、`RuntimePreviewSnapshot`、`ExecutionDispatchAlignmentRecord` |

## 3. 实体定义

### 3.1 `OnlinePreviewRequest`

**说明**: HMI 在机台已准备好的在线上下文中发起的一次预览请求。

| 字段 | 类型 | 约束 |
|---|---|---|
| `request_id` | string | 唯一 |
| `launch_mode` | enum | 固定为 `online` |
| `ready_state_ref` | string | 引用 `OnlineReadyCapabilityState.state_id` |
| `dxf_artifact_ref` | string | 引用 `RealDxfArtifact.artifact_id` |
| `dispensing_speed_mm_s` | number | 必须大于 0 |
| `max_polyline_points` | integer | 必须 >= 2 |
| `requested_at` | datetime | 必填 |
| `requested_by` | string | 必填 |

**校验规则**

- `launch_mode` 不是 `online` 时，不属于本特性范围。
- `ready_state_ref` 必须指向 `online_ready=true` 的状态快照。
- `dxf_artifact_ref` 必须指向真实 DXF，不允许 synthetic / mock 点集。

### 3.2 `OnlineReadyCapabilityState`

**说明**: HMI 当前在线就绪能力矩阵。

| 字段 | 类型 | 约束 |
|---|---|---|
| `state_id` | string | 唯一 |
| `launch_mode` | enum | 固定为 `online` |
| `backend_ready` | boolean | 必填 |
| `tcp_connected` | boolean | 必填 |
| `hardware_probed` | boolean | 必填 |
| `online_ready` | boolean | 真实验收前必须为 `true` |
| `stage_sequence` | list[string] | 应包含最小阶段序列 |
| `captured_at` | datetime | 必填 |

**判定规则**

- 只有 `backend_ready=true`、`tcp_connected=true`、`hardware_probed=true` 且 `online_ready=true` 时，才允许进入真实在线预览验收。
- 若当前状态无法证明属于同一在线就绪上下文，预览结论不得判定为通过。

### 3.3 `RealDxfArtifact`

**说明**: 被本次在线验收使用的 DXF 资产。

| 字段 | 类型 | 约束 |
|---|---|---|
| `artifact_id` | string | 唯一 |
| `file_path` | path | 指向真实 DXF 文件 |
| `display_name` | string | 必填 |
| `artifact_origin` | enum | `canonical-sample`, `business-dxf`, `operator-import` |
| `sha256` | string | 必填 |
| `size_bytes` | integer | 必须 > 0 |
| `validated_as_real_dxf` | boolean | 必须为 `true` 才可进入验收 |

**当前强制基线**

- `D:\Projects\SiligenSuite\samples\dxf\rect_diag.dxf`

### 3.4 `RuntimePreviewSnapshot`

**说明**: 在线模式下由 runtime/gateway 返回、可用于人工判断和自动回归的权威快照。

| 字段 | 类型 | 约束 |
|---|---|---|
| `snapshot_id` | string | 唯一 |
| `snapshot_hash` | string | 必填 |
| `plan_id` | string | 必填 |
| `preview_state` | enum | `snapshot_ready`, `confirmed`, `expired`, `failed` |
| `preview_source` | enum | 引用 `PreviewSourceClassification.source_kind` |
| `segment_count` | integer | 必须 >= 0 |
| `point_count` | integer | 必须 >= 0 |
| `polyline_point_count` | integer | 必须 >= 0 |
| `polyline_source_point_count` | integer | 必须 >= `polyline_point_count` |
| `trajectory_polyline` | list[point] | 至少 2 点才可进入真实预览判断 |
| `total_length_mm` | number | 必须 >= 0 |
| `estimated_time_s` | number | 必须 >= 0 |
| `generated_at` | datetime | 必填 |
| `confirmed_at` | datetime | 必填 |

**校验规则**

- `preview_source=runtime_snapshot` 是进入真实在线验收通过结论的必要条件。
- `preview_source=mock_synthetic`、`unknown` 或空值时，快照不得进入真实验收通过结论。
- `trajectory_polyline` 为空或不足 2 点时，视为预览失败。

### 3.5 `ExecutionDispatchAlignmentRecord`

**说明**: 记录当前预览快照与上位机准备下发到运动控制卡的数据之间的映射关系。

| 字段 | 类型 | 约束 |
|---|---|---|
| `alignment_id` | string | 唯一 |
| `plan_id` | string | 必填 |
| `plan_fingerprint` | string | 必填 |
| `dispatch_program_kind` | enum | 固定为 `interpolation_segments-derived` |
| `interpolation_segment_count` | integer | 必须 >= 0 |
| `geometry_semantics_match` | boolean | 必填 |
| `order_semantics_match` | boolean | 必填 |
| `dispense_motion_semantics_match` | boolean | 必填 |
| `comparison_summary` | string | 必填 |

**判定规则**

- 三个 `*_match` 必须全部为 `true`，该快照才允许进入通过结论。
- 允许序列化形式不同，但不允许出现无法解释的几何、顺序或点胶运动语义偏差。

### 3.6 `PreviewSourceClassification`

**说明**: 预览来源与验收语义的冻结分类。

| `source_kind` | 含义 | 可否作为真实验收来源 |
|---|---|---|
| `runtime_snapshot` | 在线 runtime/gateway 链生成的权威快照 | 是 |
| `mock_synthetic` | mock 或占位生成的模拟轨迹 | 否 |
| `unknown` | 来源不明或字段缺失 | 否 |

### 3.7 `PreviewEvidenceBundle`

**说明**: 一次在线预览验收输出的证据集合。

| 字段 | 类型 | 约束 |
|---|---|---|
| `bundle_id` | string | 唯一 |
| `bundle_root` | path | 指向 `tests/reports/` 下的具体目录 |
| `ready_state_ref` | string | 引用 `OnlineReadyCapabilityState.state_id` |
| `dxf_artifact_ref` | string | 引用 `RealDxfArtifact.artifact_id` |
| `snapshot_ref` | string | 引用 `RuntimePreviewSnapshot.snapshot_id` |
| `alignment_ref` | string | 引用 `ExecutionDispatchAlignmentRecord.alignment_id` |
| `baseline_ref` | path | 可为空；使用 canonical 基线时必填 |
| `ui_screenshot_path` | path/null | 可空 |
| `verdict` | enum | `passed`, `failed`, `invalid-source`, `mismatch`, `not-ready`, `incomplete` |
| `generated_at` | datetime | 必填 |

**必须包含的证据文件**

- `plan-prepare.json`
- `snapshot.json`
- `trajectory_polyline.json`
- `preview-verdict.json`
- `preview-evidence.md`

## 4. 关系约束

```text
OnlineReadyCapabilityState
└── 影响 HMI UI 是否允许发起 OnlinePreviewRequest

RealDxfArtifact
└── n x OnlinePreviewRequest

OnlinePreviewRequest
└── 1 x RuntimePreviewSnapshot

RuntimePreviewSnapshot
├── 1 x PreviewSourceClassification
└── 1 x ExecutionDispatchAlignmentRecord

PreviewEvidenceBundle
├── 1 x OnlineReadyCapabilityState
├── 1 x RealDxfArtifact
├── 1 x RuntimePreviewSnapshot
└── 1 x ExecutionDispatchAlignmentRecord
```

## 5. 状态迁移

### 5.1 `RuntimePreviewSnapshot`

```text
requested -> snapshot_ready -> confirmed -> passed
requested -> snapshot_ready -> mismatch
requested -> failed
requested -> not-ready
snapshot_ready -> expired
snapshot_ready -> invalid-source
```

### 5.2 `PreviewEvidenceBundle`

```text
pending -> captured -> compared -> passed
pending -> captured -> invalid-source
pending -> captured -> mismatch
pending -> not-ready
pending -> incomplete
```

## 6. 完成判定闭环

一次在线预览验收仅在以下条件同时满足时才可判定为 `passed`：

1. `OnlineReadyCapabilityState.online_ready = true`，且能证明属于当前验收上下文。
2. `RealDxfArtifact.validated_as_real_dxf = true`。
3. `RuntimePreviewSnapshot.preview_source = runtime_snapshot`。
4. `ExecutionDispatchAlignmentRecord` 中三项语义匹配均为 `true`。
5. `PreviewEvidenceBundle` 必需文件齐全，并可回链到同一 DXF、同一 plan、同一在线就绪上下文。
