# Contract: Planned Glue Preview Provider

## 1. 目的

冻结 `dxf.preview.snapshot` 在本特性中的主预览语义，确保 gateway、workflow 与 HMI 都把 `planned_glue_snapshot + glue_points` 当作唯一可接受的执行前权威预览。

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
3. 当前计划已持有非空 `glue_points`
4. 当前预览来源可追溯到与 execution launch 共用的同一份权威结果

## 4. 响应主语义

| 字段 | 约束 |
|---|---|
| `preview_source` | 必须为 `planned_glue_snapshot` |
| `preview_kind` | 必须为 `glue_points` |
| `glue_points` | 必须非空，且是主预览语义 |
| `execution_polyline` | 允许采样，但只作为辅助叠加层 |
| `snapshot_hash` | 必须存在，并与当前计划上下文一致 |
| `plan_id` | 必须与当前请求 plan 一致 |

## 5. 语义规则

1. `glue_points` 是执行前确认的主语义，不得由 HMI 或 gateway 再次重采样决定。
2. `execution_polyline` 仅用于辅助展示走向，不得替代 `glue_points` 成为通过依据。
3. provider 不得在 `glue_points` 缺失时偷偷退回 `motion_trajectory_points`、`runtime_snapshot` 或其他旧链结果。
4. gateway 必须继续拒绝 `preview_source != planned_glue_snapshot` 或 `preview_kind != glue_points` 的响应。
5. HMI 必须继续把 `planned_glue_snapshot` 呈现为规划胶点主预览，并把不合规来源视为非权威结果。

## 6. 成功条件

以下条件全部满足时，本 contract 才视为成功：

1. `glue_points` 非空
2. `preview_source == planned_glue_snapshot`
3. `preview_kind == glue_points`
4. `plan_id` 与当前请求一致
5. `snapshot_hash` 存在且可用于后续 confirm / start job 链路

## 7. 失败边界

以下任一条件出现时，provider 或 transport layer 必须失败，而不是静默兼容：

1. `glue_points` 为空
2. `preview_source` 缺失或不等于 `planned_glue_snapshot`
3. `preview_kind` 不等于 `glue_points`
4. 响应 `plan_id` 与请求 plan 不一致
5. 当前计划不是 latest，或 authority 无法证明与 execution launch 共用同一结果

## 8. 与正式协议的关系

本文件不新增协议字段，也不替代 `dxf.command-set.json`。它只把本特性要求的 provider/consumer 约束冻结下来，用于后续 tasks 与测试设计。
