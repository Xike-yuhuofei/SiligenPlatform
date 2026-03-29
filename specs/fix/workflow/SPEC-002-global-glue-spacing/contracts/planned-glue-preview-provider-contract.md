# Contract: Planned Glue Preview Provider

## 1. 目的

冻结本特性下 `dxf.preview.snapshot` 的成功主语义，确保 gateway、workflow 与 HMI 都把 `planned_glue_snapshot + glue_points` 当作唯一可接受的执行前权威预览。

## 2. 适用边界

- provider: `D:\Projects\SiligenSuite\modules\workflow\application\usecases\dispensing\DispensingWorkflowUseCase.cpp`
- snapshot assembler: `D:\Projects\SiligenSuite\modules\workflow\application\services\dispensing\WorkflowPreviewSnapshotService.cpp`
- transport guard: `D:\Projects\SiligenSuite\apps\runtime-gateway\transport-gateway\src\tcp\TcpCommandDispatcher.cpp`
- consumer: `D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\ui\main_window.py`
- formal schema source: `D:\Projects\SiligenSuite\shared\contracts\application\commands\dxf.command-set.json`

## 3. 请求前提

在允许生成 preview snapshot 之前，以下条件必须同时满足：

1. 已存在当前计划的 `plan_id`
2. 当前计划仍是 latest plan，不是 stale 记录
3. 当前计划持有一份有效的 `AuthorityTriggerLayout`
4. 当前计划的 execution gate 与 preview gate 共享同一份 authority layout

## 4. 响应主语义

| 字段 | 约束 |
|---|---|
| `preview_source` | 必须为 `planned_glue_snapshot` |
| `preview_kind` | 必须为 `glue_points` |
| `glue_points` | 必须非空，且按 authority 全局顺序直接来自 `LayoutTriggerPoint.position` |
| `execution_polyline` | 允许采样，但只作为辅助叠加层 |
| `snapshot_hash` | 必须存在，并与当前 `plan_fingerprint` 对齐 |
| `plan_id` | 必须与当前请求 plan 一致 |

## 5. 语义规则

1. `glue_points` 是执行前确认的主语义，不得由 gateway 或 HMI 重新采样决定。
2. `execution_polyline` 只用于辅助展示路径走向，不得替代 `glue_points` 成为通过依据。
3. provider 不得在 authority 缺失时退回 `runtime_snapshot`、`motion_trajectory_points`、`trajectory_polyline` 或其他显示专用结果。
4. gateway 必须继续拒绝 `preview_source != planned_glue_snapshot` 或 `preview_kind != glue_points` 的响应。
5. HMI 必须继续把 `planned_glue_snapshot` 呈现为规划胶点主预览，并把非该来源的结果视为非权威结果。

## 6. 成功条件

以下条件全部满足时，本 contract 才视为成功：

1. `glue_points` 非空
2. `preview_source == planned_glue_snapshot`
3. `preview_kind == glue_points`
4. `plan_id` 与当前请求一致
5. `snapshot_hash` 存在且可用于后续 confirm / start job 链路
6. preview 对应的 authority layout 没有阻塞性校验失败

## 7. 失败边界

以下任一条件出现时，provider 或 transport layer 必须失败，而不是静默兼容：

1. `glue_points` 为空
2. `preview_source` 缺失或不等于 `planned_glue_snapshot`
3. `preview_kind` 不等于 `glue_points`
4. 响应 `plan_id` 与请求 plan 不一致
5. 当前计划不是 latest，或 authority 与 execution binding 不一致
6. preview 对应 authority layout 存在阻塞性校验失败

## 8. 兼容边界

1. 现有“缺少 `plan_id` 时先 prepare 一次”的 transport 兼容分支不是新的 truth source，不得扩展为绕过 authority 校验的捷径。
2. `runtime_snapshot` 可继续存在于旧链或诊断链中，但不得进入本 contract 的成功路径。
3. 本文件不新增对外 schema 字段，也不替代 `dxf.command-set.json`；它只冻结 provider / consumer 约束。
