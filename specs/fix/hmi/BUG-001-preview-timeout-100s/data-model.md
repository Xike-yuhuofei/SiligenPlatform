# Phase 1 Data Model: HMI 胶点预览超时窗口调整

## 1. 模型目标

本数据模型描述的是“打开 DXF 自动触发的胶点预览链路如何在不改变 wire payload 的前提下，将本地等待预算从 15 秒收敛到 300 秒，并把真实超时值传播到用户可见错误”。它不是数据库设计，而是为 `apps/hmi-app` 与 `modules/hmi-application` 中的 transport、worker、UI 失败传播提供统一语义。

## 2. 实体总览

| 实体 | 作用 | 核心关系 |
|---|---|---|
| `AutoPreviewGenerationFlow` | 表示一次“打开 DXF 后自动生成预览”的完整本地流程 | 聚合 `PreviewPrepareRequest`、`PreviewSnapshotRequest`、`PreviewTimeoutPolicy`、`UserVisiblePreviewFailure` |
| `PreviewPrepareRequest` | 表示自动预览链路中的 `dxf.plan.prepare` 本地请求 | 成功后产出 `plan_id` 给 `PreviewSnapshotRequest` |
| `PreviewSnapshotRequest` | 表示自动预览链路中的 `dxf.preview.snapshot` 本地请求 | 消费 `plan_id`，成功后返回权威预览 payload |
| `PreviewTimeoutPolicy` | 表示本地调用链上的等待预算与适用范围 | 同时约束 `PreviewPrepareRequest` 与 `PreviewSnapshotRequest` |
| `UserVisiblePreviewFailure` | 表示最终呈现给 HMI 用户的失败信息 | 由 flow 内任一失败阶段生成并消费真实 timeout 文本 |

## 3. 实体定义

### 3.1 `AutoPreviewGenerationFlow`

**说明**: 用户打开 DXF 且工件上传成功后，HMI 自动进入的一次预览生成流程。

| 字段 | 类型 | 约束 |
|---|---|---|
| `flow_id` | string | 唯一 |
| `artifact_ref` | string | 必填；指向当前 DXF 工件 |
| `initiated_from` | enum | 固定为 `dxf_open_auto_preview` |
| `state` | enum | `idle`, `preparing_plan`, `requesting_snapshot`, `ready`, `failed` |
| `timeout_policy_ref` | string | 引用 `PreviewTimeoutPolicy.policy_id` |
| `current_plan_id` | string/null | `PreviewPrepareRequest` 成功后必填 |
| `last_error_ref` | string/null | 失败时引用 `UserVisiblePreviewFailure.failure_id` |

**规则**

- 只有在 `dxf.artifact.create` 成功后，flow 才允许从 `idle` 进入 `preparing_plan`。
- `PreviewPrepareRequest` 或 `PreviewSnapshotRequest` 任一阶段失败，flow 必须进入 `failed`。
- 只有在 prepare 与 snapshot 都成功后，flow 才允许进入 `ready`。

### 3.2 `PreviewPrepareRequest`

**说明**: 自动预览链路中对 `dxf.plan.prepare` 的本地请求封装。

| 字段 | 类型 | 约束 |
|---|---|---|
| `request_id` | string | 唯一 |
| `flow_ref` | string | 引用 `AutoPreviewGenerationFlow.flow_id` |
| `artifact_ref` | string | 必填 |
| `speed_mm_s` | number | 必须 > 0 |
| `dry_run` | boolean | 必填 |
| `timeout_s` | number | 在本特性范围内固定为 `300.0` |
| `result_plan_id` | string/null | 成功时必填 |
| `error_message` | string/null | 失败时必填 |

**规则**

- 对于 `initiated_from=dxf_open_auto_preview` 的 flow，`timeout_s` 必须为 `300.0`。
- `result_plan_id` 为空时，不允许继续发起 `PreviewSnapshotRequest`。
- 非超时失败必须保留原始错误文本，不得被重写为 timeout。

### 3.3 `PreviewSnapshotRequest`

**说明**: 自动预览链路中对 `dxf.preview.snapshot` 的本地请求封装。

| 字段 | 类型 | 约束 |
|---|---|---|
| `request_id` | string | 唯一 |
| `flow_ref` | string | 引用 `AutoPreviewGenerationFlow.flow_id` |
| `plan_ref` | string | 引用 `PreviewPrepareRequest.result_plan_id` |
| `max_polyline_points` | integer | 必须 >= 2 |
| `timeout_s` | number | 在本特性范围内固定为 `300.0` |
| `preview_source` | string/null | 成功时应为 `planned_glue_snapshot` |
| `preview_kind` | string/null | 成功时应为 `glue_points` |
| `error_message` | string/null | 失败时必填 |

**规则**

- `plan_ref` 缺失时，snapshot 请求不得启动。
- 对于 `initiated_from=dxf_open_auto_preview` 的 flow，`timeout_s` 必须为 `300.0`。
- 成功 payload 仍必须满足既有 preview authority 规则；本特性不改变 `preview_source` / `preview_kind` 的合法值。

### 3.4 `PreviewTimeoutPolicy`

**说明**: 定义本地等待预算的适用边界。

| 字段 | 类型 | 约束 |
|---|---|---|
| `policy_id` | string | 唯一 |
| `scope` | enum | `dxf_open_auto_preview` |
| `prepare_timeout_s` | number | 固定为 `300.0` |
| `snapshot_timeout_s` | number | 固定为 `300.0` |
| `apply_to_resync` | boolean | 固定为 `false` |
| `apply_to_preview_confirm` | boolean | 固定为 `false` |
| `apply_to_job_start` | boolean | 固定为 `false` |
| `apply_to_connect` | boolean | 固定为 `false` |

**规则**

- `PreviewTimeoutPolicy` 只绑定“打开 DXF 自动预览”这一个 scope。
- 任何未显式声明在 scope 内的请求，都不得自动继承 `300.0s`。
- 同一 policy 下，prepare 与 snapshot 的 timeout 必须保持一致，以避免链路内局部 15 秒残留。

### 3.5 `UserVisiblePreviewFailure`

**说明**: 最终展示在 HMI 上的预览失败信息。

| 字段 | 类型 | 约束 |
|---|---|---|
| `failure_id` | string | 唯一 |
| `flow_ref` | string | 引用 `AutoPreviewGenerationFlow.flow_id` |
| `title` | string | 典型值为 `胶点预览生成失败` |
| `detail` | string | 必填；来自底层错误传播 |
| `failure_category` | enum | `timeout`, `backend_error`, `local_error` |
| `source_stage` | enum | `plan_prepare`, `preview_snapshot`, `worker_startup`, `unknown` |
| `displayed_timeout_s` | number/null | 仅 `failure_category=timeout` 时必填 |

**规则**

- 当 `failure_category=timeout` 时，`detail` 必须反映底层真实 `timeout_s`，不得继续显示旧的 `15.0s`。
- 当 `failure_category!=timeout` 时，`detail` 必须保留原始失败语义，不能统一覆写为 timeout。
- `title` 可以稳定为“胶点预览生成失败”，但 `detail` 必须区分 timeout 与非 timeout。

## 4. 关系约束

```text
AutoPreviewGenerationFlow
├── 1..1 PreviewTimeoutPolicy
├── 1..1 PreviewPrepareRequest
├── 0..1 PreviewSnapshotRequest
└── 0..1 UserVisiblePreviewFailure

PreviewPrepareRequest
└── 成功后为 PreviewSnapshotRequest 提供 plan_ref

UserVisiblePreviewFailure
└── 必须能够追溯到 flow 内的失败阶段与真实 timeout 文本
```

## 5. 状态迁移

### 5.1 `AutoPreviewGenerationFlow`

```text
idle -> preparing_plan -> requesting_snapshot -> ready
preparing_plan -> failed
requesting_snapshot -> failed
```

### 5.2 `UserVisiblePreviewFailure`

```text
pending -> surfaced
pending -> suppressed
```

说明：本特性只允许在“请求实际失败”时进入 `surfaced`；不允许把仍在 300 秒预算内执行的请求过早归类为失败。

## 6. 完成判定闭环

一次“打开 DXF 自动预览”只有在以下条件全部满足时，才允许视为设计完成：

1. `PreviewTimeoutPolicy.scope = dxf_open_auto_preview`
2. `PreviewPrepareRequest.timeout_s = 300.0`
3. `PreviewSnapshotRequest.timeout_s = 300.0`
4. `UserVisiblePreviewFailure.detail` 在 timeout 场景下明确反映 `300.0s` 或等价 300 秒新阈值
5. runtime resync、`dxf.preview.confirm`、`dxf.job.start`、`connect` 等其他入口不被被动扩展到 `300s`
