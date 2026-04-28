# 当前协议映射清单

以下清单只记录当前代码里已经存在的协议事实，不推测未来命名。

## `status`

| 方法 | 类型 | HMI 调用点 | TCP 处理器 | CLI 对应语义 | 结果字段 | 兼容说明 |
|---|---|---|---|---|---|---|
| `status` | 查询 | `CommandProtocol.get_status()` | `HandleStatus` | `HandleStatus` | `connected` / `connection_state` / `device_mode` / `machine_state` / `machine_state_reason` / `supervision` / `runtime_identity` / `safety_boundary` / `action_capabilities` / `effective_interlocks` / `job_execution` / `axes` / `position` / `io` / `dispenser` / `alarms` | `supervision` 是当前监督态 owner 面；`machine_state` / `machine_state_reason` 作为兼容镜像继续导出，值分别镜像 `supervision.current_state` / `supervision.state_reason`。`runtime_identity` 是当前 runtime launch contract authority 面，固定导出 `executable_path` / `working_directory` / `protocol_version` / `preview_snapshot_contract`；HMI online startup 必须用它核对本地 launch contract，缺失或不匹配必须 fail-closed，不允许 silent fallback。`device_mode` 当前是 V1 派生字段：`job_execution.dry_run=true` 时为 `test`，其余为 `production`。`safety_boundary` 当前是 V1 软件动作准入快照，基于 `effective_interlocks` / `io` / `interlock_latched` / `device_mode` / `job_execution.dry_run` 派生，只表达运动与真实工艺输出是否准入；缺字段时 HMI 按同一规则兼容派生。`action_capabilities` 当前是 V1 后端粗粒度摘要，基于 `connected` / `connection_state` / `safety_boundary` / `job_execution` 派生，只表达网关侧是否适合继续发起常规运动命令、手动输出命令以及是否存在活动 job，不等同 UI 最终 enable。`connected` / `connection_state` / `interlock_latched` / `supervision` / `runtime_identity` / `effective_interlocks` / `job_execution` / `io` 统一来自 `IRuntimeStatusExportPort` snapshot，HMI 主消费依赖 `supervision`、`runtime_identity`、`effective_interlocks`、`safety_boundary`、`axes`、`io`、`dispenser.*` |

## `alarms.*`

| 方法 | 类型 | HMI 调用点 | TCP 处理器 | CLI 对应语义 | 结果字段 | 兼容说明 |
|---|---|---|---|---|---|---|
| `alarms.list` | 查询 | `CommandProtocol.get_alarms()` | `HandleAlarmsList` | 无独立 CLI facade | `alarms[]` | `alarm.level` 当前观测到 `CRIT` / `WARN` |
| `alarms.clear` | 状态清理 | `CommandProtocol.clear_alarms()` | `HandleAlarmsClear` | 无 | `ok` | 语义是清除已确认集合，不是底层硬件复位 |
| `alarms.acknowledge` | 状态清理 | `CommandProtocol.acknowledge_alarm()` | `HandleAlarmsAcknowledge` | 无 | `ok` | 必须传 `id` |

## `dxf.*`

| 方法 | 类型 | HMI 调用点 | TCP 处理器 | CLI 对应语义 | 结果字段 | 兼容说明 |
|---|---|---|---|---|---|---|
| `dxf.artifact.create` | 命令 | `CommandProtocol.dxf_create_artifact()` | `HandleDxfArtifactCreate` | 无独立 CLI facade | `artifact_id` / `source_drawing_ref` / `filepath` / `source_hash` / `size` / `validation_report` | 生成 canonical source drawing artifact，作为后续 prepare 的唯一输入；`artifact_id == source_drawing_ref`，当前固定为 source-hash authority；S1 只返回文件级 validation report，不导出 `prepared_filepath`，也不再导出 `input_quality` 双轨真值；旧 `dxf.load` 已退役，不再保留兼容入口 |
| `dxf.plan.prepare` | 命令 | `CommandProtocol.dxf_prepare_plan()` | `HandleDxfPlanPrepare` | `DXF_PLAN` | `plan_id` / `plan_fingerprint` / `prepared_filepath` / `production_baseline` / `execution_nominal_time_s` / `execution_plan_summary` / `path_quality` / `input_quality` / `formal_compare_gate` / `preview_validation_classification` / `preview_exception_reason` / `preview_failure_reason` / `preview_diagnostic_code` | 必须显式传入 `artifact_id`；当前阶段由 runtime owner 解析 `current production baseline`，返回中的 `production_baseline` 是 DXF 主链固定参数与追溯真值。可选 `requested_execution_strategy` 未传时默认按 `flying_shot` 解析。`curve_flatten_max_step_mm` / `curve_flatten_max_error_mm` 是规划阶段通用曲线离散化控制，禁止继续沿用任何 `spline_*` 命名暴露为 DXF 导入兼容开关。`preview.snapshot.estimated_time_s` 继续保留 preview 真值；`prepare` 不再导出 execution 语义的 `estimated_time_s`，execution 消费者只能读取 `execution_nominal_time_s` 与 `execution_plan_summary`。`path_quality` 是 prepare/snapshot/start 共用的生产路径质量真值，当前 blocking `reason_codes` 至少包括 `process_path_fragmentation` / `path_discontinuity` / `micro_segment_burst` / `abnormal_short_segment` / `small_backtrack` / `unclassified_path_exception` / `spacing_invalid` / `formal_compare_blocked`。`input_quality` 是 artifact sidecar validation report 的唯一导入真值根，禁止再回落到平铺 `import_*` 字段，也禁止从 `summary` / `errors[]` / `warnings[]` / `file.unit` 派生缺失字段。`preview_*` 仍可作为 preview metadata 消费，但不得再替代 `path_quality` 的 production gate authority。`formal_compare_gate` 继续保留 execution 细节，但 production 是否阻断必须通过 `path_quality` 体现；当前 `Demo-1` 一类下降/回返 compare 几何必须固化为 preview-only / production-blocked，而不是由 runtime 静默兜底。`POINT` 若无 formal carrier trajectory，仅允许基于 baseline-owned `point_flying_carrier_policy` 并按 `approach_direction = normalize(point - planning_start_position)` 合成 formal carrier trajectory；禁止 `artifact_id` 或其他全局活动态回退 |
| `dxf.preview.snapshot` | 命令 | `CommandProtocol.dxf_preview_snapshot()` | `HandleDxfPreviewSnapshot` | 无独立 CLI facade | `snapshot_hash` / `plan_id` / `preview_kind` / `glue_points` / `glue_reveal_lengths_mm` / `preview_binding` / `motion_preview` / `path_quality` / `input_quality` / `preview_validation_classification` / `preview_exception_reason` / `preview_failure_reason` / `preview_diagnostic_code` | 唯一 shared authority success shape 是 `preview_source=planned_glue_snapshot` + `preview_kind=glue_points` + 非空 `glue_points`；`glue_reveal_lengths_mm` 与 `glue_points` 等长同序，作为 execution reveal 真值；`preview_binding` 是 owner 导出的 display binding 真值；`motion_preview` 只接受 `execution_trajectory_snapshot + polyline + 非空 polyline` 单轨语义。`path_quality` 在 snapshot 中返回 prepare 阶段持久化的同一评估结果，不在 snapshot 链重新发明第二套 gate。`input_quality` 与 artifact/prepare/start/traceability 共享同一输入治理真值。 |
| `dxf.preview.confirm` | 命令 | `CommandProtocol.dxf_preview_confirm()` | `HandleDxfPreviewConfirm` | 无独立 CLI facade | `confirmed` / `plan_id` / `snapshot_hash` | 预览确认必须绑定当前 `plan_id + snapshot_hash`，确认后才能进入 `dxf.job.start` |
| `dxf.job.start` | 命令 | `CommandProtocol.dxf_start_job()` | `HandleDxfJobStart` | `DXF_DISPENSE` | `started` / `job_id` / `plan_id` / `plan_fingerprint` / `production_baseline` / `execution_budget_s` / `execution_budget_breakdown` / `input_quality` / `formal_compare_gate` | canonical 启动入口；旧 `dxf.execute` 已退役；仅在 preview confirmed、source valid、authority shared 且导入结果 production-ready 时允许启动。当前 `dxf.job.start` 必须对 `path_quality.blocking=true` fail-closed，blocking `reason_codes` 一律阻断生产，不允许 warning-only 放行，也不允许 runtime start port 被调用。返回中的 `production_baseline` 与 prepare 保持同一 owner 真值。省略 `auto_continue` 时保持既有自动推进语义，显式 `auto_continue=false` 时单周期完成后进入 `awaiting_continue`。所有 execution timeout/budget 消费者必须以 runtime 导出的 `execution_budget_s` / `execution_budget_breakdown` 为准 |
| `dxf.job.status` | 查询 | `CommandProtocol.dxf_get_job_status()` | `HandleDxfJobStatus` | 无 | `state` / `overall_progress_percent` / `completed_count` / `execution_budget_s` / `execution_budget_breakdown` | 作为运行态查询主入口，替代旧 `dxf.progress`；与 `dxf.job.start` 共享同一 execution budget 契约，不允许再回读 `prepare` 字段 |
| `dxf.job.pause` | 命令 | `CommandProtocol.dxf_job_pause()` | `HandleDxfJobPause` | 无 | `paused` / `job_id` | 支持显式 `job_id`，缺省时回退当前活动 job |
| `dxf.job.resume` | 命令 | `CommandProtocol.dxf_job_resume()` | `HandleDxfJobResume` | 无 | `resumed` / `job_id` | 支持显式 `job_id`，缺省时回退当前活动 job |
| `dxf.job.continue` | 命令 | `CommandProtocol.dxf_job_continue()` | `HandleDxfJobContinue` | 无 | `continued` / `job_id` | 仅在 `awaiting_continue` 状态下允许推进下一生产周期；不复用 `resume` 语义 |
| `dxf.job.stop` | 命令 | `CommandProtocol.dxf_job_stop()` | `HandleDxfJobStop` | 无 | `stopped` / `job_id` / `transition_state` / `message` | 停止 canonical job；结果来自 owner transition surface，即使已完成/未接受也返回当前过渡态与说明 |
| `dxf.job.cancel` | 命令 | `CommandProtocol.dxf_job_cancel()` | `HandleDxfJobCancel` | 无 | `cancelled` / `job_id` / `transition_state` / `message` | 取消 canonical job；结果来自 owner transition surface，即使已完成/未接受也返回当前过渡态与说明 |
| `dxf.info` | 查询 | `CommandProtocol.dxf_get_info()` | `HandleDxfInfo` | `DXF_PLAN` 结果侧等价 | `total_length` / `total_segments` / `bounds` | 与 HMI UI 段数读取字段已对齐 |

### `dxf.preview` authority gate

- `dxf.preview.snapshot` 的唯一成功主预览语义是 shared authority 下的 `planned_glue_snapshot + glue_points + glue_reveal_lengths_mm + preview_binding`；`glue_reveal_lengths_mm` 是 execution reveal 真值，`preview_binding` 是返回给 HMI 的 display binding 真值；`motion_preview` 仅接受 `execution_trajectory_snapshot + polyline + 非空 polyline`，用于头部运动轨迹预览。
- `preview_validation_classification` / `preview_exception_reason` / `preview_failure_reason` / `preview_diagnostic_code` 属于预览诊断元数据；允许 HMI/CLI 消费，但不得替代 `preview_source` / `preview_kind` / `glue_points` 的 authority 判定，也不得再直接充当 production gate authority。
- `path_quality` 是当前生产路径质量门禁真值；`process_path_fragmentation` / `path_discontinuity` / `micro_segment_burst` / `abnormal_short_segment` / `small_backtrack` / `unclassified_path_exception` / `spacing_invalid` / `formal_compare_blocked` 都属于 `path_quality.reason_codes`，其中 blocking reason 必须阻断 `dxf.job.start`。
- 以下场景必须按失败边界处理，不得进入 HMI 成功渲染或执行前通过路径：non-`planned_glue_snapshot`、non-`glue_points`、空 `glue_points`、缺少或无效 `snapshot_hash`、`plan_id` / authority mismatch、legacy `runtime_snapshot` / `trajectory_polyline`、缺少或无效 `glue_reveal_lengths_mm`、缺少或无效 `preview_binding`、non-`execution_trajectory_snapshot`、non-`polyline`、空 `motion_preview.polyline`。
- `dxf.preview.confirm` 负责把当前 `plan_id + snapshot_hash` 绑定到已确认预览；`dxf.job.start` 继续依赖该确认结果与 `plan_fingerprint` 一致，避免 preview 与 execution 消费不同 shared authority 结果。

## 额外观察

- `dxf.*` 当前正式执行链已收敛到 `artifact.create -> plan.prepare -> preview.snapshot -> preview.confirm -> job.start -> job.status`。
- runtime-execution 对跨模块公开面已收敛为 `DispensingExecutionRequest + DispensingExecutionResult + job API`；`task` 只允许留在 runtime-execution 内部实现或内部测试语境。
- `status` 当前整体由 `IRuntimeStatusExportPort` snapshot 提供；其中 `connected` / `connection_state` / `device_mode` / `interlock_latched` / `job_execution` / `supervision` / `runtime_identity` / `effective_interlocks` / `io` 由 export snapshot 统一导出，底层 supervision 语义来自 `IRuntimeSupervisionPort` 输入，`runtime-gateway` 只负责 transport 序列化。
- HMI 主消费依赖 `status.supervision`、`effective_interlocks`、`safety_boundary`、`axes`、`io`；`machine_state` compat 字段当前仅为未迁移调用方保留，不作为新 owner 面扩展。
- `status` 与 `alarms.*` 结构已被 HMI UI 直接依赖，字段改名风险高。
