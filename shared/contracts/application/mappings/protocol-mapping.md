# 当前协议映射清单

以下清单只记录当前代码里已经存在的协议事实，不推测未来命名。

## `status`

| 方法 | 类型 | HMI 调用点 | TCP 处理器 | CLI 对应语义 | 结果字段 | 兼容说明 |
|---|---|---|---|---|---|---|
| `status` | 查询 | `CommandProtocol.get_status()` | `HandleStatus` | `HandleStatus` | `connected` / `machine_state` / `supervision` / `effective_interlocks` / `axes` / `position` / `io` / `dispenser` / `alarms` | `supervision` 是当前监督态 owner 面；`machine_state` 仅保留 compat 单向导出，且连同 `connected` / `connection_state` / `active_job_*` / `io` / `effective_interlocks` 一并并入 `IRuntimeStatusExportPort` snapshot；HMI 主消费已转向 `supervision`、`effective_interlocks`、`axes`、`io`、`dispenser.*` |

## `alarms.*`

| 方法 | 类型 | HMI 调用点 | TCP 处理器 | CLI 对应语义 | 结果字段 | 兼容说明 |
|---|---|---|---|---|---|---|
| `alarms.list` | 查询 | `CommandProtocol.get_alarms()` | `HandleAlarmsList` | 无独立 CLI facade | `alarms[]` | `alarm.level` 当前观测到 `CRIT` / `WARN` |
| `alarms.clear` | 状态清理 | `CommandProtocol.clear_alarms()` | `HandleAlarmsClear` | 无 | `ok` | 语义是清除已确认集合，不是底层硬件复位 |
| `alarms.acknowledge` | 状态清理 | `CommandProtocol.acknowledge_alarm()` | `HandleAlarmsAcknowledge` | 无 | `ok` | 必须传 `id` |

## `dxf.*`

| 方法 | 类型 | HMI 调用点 | TCP 处理器 | CLI 对应语义 | 结果字段 | 兼容说明 |
|---|---|---|---|---|---|---|
| `dxf.load` | 命令 | `CommandProtocol.dxf_load()` | `HandleDxfLoad` | `DXF_PLAN` / `DXF_DISPENSE` 前置 | `loaded` / `segment_count` / `filepath` | 同时接受 `filepath` / `file_path` |
| `dxf.artifact.create` | 命令 | `CommandProtocol.dxf_create_artifact()` | `HandleDxfArtifactCreate` | 无独立 CLI facade | `artifact_id` / `filepath` / `size` | 生成 canonical artifact，作为后续 prepare 的唯一输入 |
| `dxf.plan.prepare` | 命令 | `CommandProtocol.dxf_prepare_plan()` | `HandleDxfPlanPrepare` | `DXF_PLAN` | `plan_id` / `plan_fingerprint` / `estimated_time_s` | 允许省略 `artifact_id` 回退最近一次已加载 artifact，但新客户端应显式传入 |
| `dxf.preview.snapshot` | 命令 | `CommandProtocol.dxf_preview_snapshot()` | `HandleDxfPreviewSnapshot` | 无独立 CLI facade | `snapshot_hash` / `plan_id` / `preview_kind` / `glue_points` / `motion_preview` | 唯一 shared authority success shape 是 `preview_source=planned_glue_snapshot` + `preview_kind=glue_points` + 非空 `glue_points`；`motion_preview` 是唯一保留的点胶头运动轨迹预览字段 |
| `dxf.preview.confirm` | 命令 | `CommandProtocol.dxf_preview_confirm()` | `HandleDxfPreviewConfirm` | 无独立 CLI facade | `confirmed` / `plan_id` / `snapshot_hash` | 预览确认必须绑定当前 `plan_id + snapshot_hash`，确认后才能进入 `dxf.job.start` |
| `dxf.job.start` | 命令 | `CommandProtocol.dxf_start_job()` | `HandleDxfJobStart` | `DXF_DISPENSE` | `started` / `job_id` / `plan_id` / `plan_fingerprint` | canonical 启动入口；旧 `dxf.execute` 已退役；仅在 preview confirmed、source valid、authority shared 时允许启动 |
| `dxf.job.status` | 查询 | `CommandProtocol.dxf_get_job_status()` | `HandleDxfJobStatus` | 无 | `state` / `overall_progress_percent` / `completed_count` | 作为运行态查询主入口，替代旧 `dxf.progress` |
| `dxf.job.pause` | 命令 | `CommandProtocol.dxf_job_pause()` | `HandleDxfJobPause` | 无 | `paused` / `job_id` | 支持显式 `job_id`，缺省时回退当前活动 job |
| `dxf.job.resume` | 命令 | `CommandProtocol.dxf_job_resume()` | `HandleDxfJobResume` | 无 | `resumed` / `job_id` | 支持显式 `job_id`，缺省时回退当前活动 job |
| `dxf.job.stop` | 命令 | `CommandProtocol.dxf_job_stop()` | `HandleDxfJobStop` | 无 | `stopped` / `job_id` / `transition_state` | 停止 canonical job，并同步清理当前活动 job 缓存 |
| `dxf.info` | 查询 | `CommandProtocol.dxf_get_info()` | `HandleDxfInfo` | `DXF_PLAN` 结果侧等价 | `total_length` / `total_segments` / `bounds` | 与 HMI UI 段数读取字段已对齐 |

### `dxf.preview` authority gate

- `dxf.preview.snapshot` 的唯一成功主预览语义是 shared authority 下的 `planned_glue_snapshot + glue_points`；`motion_preview` 仅用于头部运动轨迹预览，不参与 authority 判定。
- 以下场景必须按失败边界处理，不得进入 HMI 成功渲染或执行前通过路径：non-`planned_glue_snapshot`、non-`glue_points`、空 `glue_points`、缺少或无效 `snapshot_hash`、`plan_id` / authority mismatch、legacy `runtime_snapshot` / `trajectory_polyline`。
- `dxf.preview.confirm` 负责把当前 `plan_id + snapshot_hash` 绑定到已确认预览；`dxf.job.start` 继续依赖该确认结果与 `plan_fingerprint` 一致，避免 preview 与 execution 消费不同 shared authority 结果。

## `recipe.*`

| 方法 | 类型 | HMI 调用点 | TCP 处理器 | CLI 对应语义 | 结果字段 | 兼容说明 |
|---|---|---|---|---|---|---|
| `recipe.list` | 查询 | `recipe_list()` | `HandleRecipeList` | `RECIPE_LIST` | `recipes[]` | `status/query/tag` 直接透传 |
| `recipe.get` | 查询 | `recipe_get()` | `HandleRecipeGet` | `RECIPE_GET` | `recipe` | 接受 `recipeId` / `recipe_id` |
| `recipe.templates` | 查询 | `recipe_templates()` | `HandleRecipeTemplates` | 无 | `templates[]` | 模板实体由 `TemplateToJson` 序列化 |
| `recipe.schema.default` | 查询 | `recipe_schema_default()` | `HandleRecipeSchemaDefault` | 无 | `schema` | 返回参数 schema 默认模板 |
| `recipe.create` | 命令 | `recipe_create()` | `HandleRecipeCreate` | `RECIPE_CREATE` | `recipe` | `tags` 支持数组或逗号分隔字符串 |
| `recipe.update` | 命令 | `recipe_update()` | `HandleRecipeUpdate` | `RECIPE_UPDATE` | `recipe` | 接受 `recipeId` / `recipe_id` |
| `recipe.archive` | 命令 | `recipe_archive()` | `HandleRecipeArchive` | `RECIPE_ARCHIVE` | `archived` | 接受 `recipeId` / `recipe_id` |
| `recipe.draft.create` | 命令 | `recipe_draft_create()` | `HandleRecipeDraftCreate` | `RECIPE_DRAFT_CREATE` | `version` | 多组 camel/snake 别名共存 |
| `recipe.draft.update` | 命令 | `recipe_draft_update()` | `HandleRecipeDraftUpdate` | `RECIPE_DRAFT_UPDATE` | `version` | `parameters[]` 为显式数组，不兼容 CLI token 文本格式 |
| `recipe.publish` | 命令 | `recipe_publish()` | `HandleRecipePublish` | `RECIPE_PUBLISH` | `version` | 接受 `versionId` / `version_id` |
| `recipe.versions` | 查询 | `recipe_versions()` | `HandleRecipeVersions` | `RECIPE_LIST_VERSIONS` | `versions[]` | 接受 `recipeId` / `recipe_id` |
| `recipe.version.create` | 命令 | `recipe_version_create()` | `HandleRecipeVersionCreate` | `RECIPE_VERSION_CREATE` | `version` | 从已发布版本派生草稿 |
| `recipe.version.compare` | 查询 | `recipe_compare()` | `HandleRecipeCompare` | `RECIPE_VERSION_COMPARE` | `changes[]` | `baseVersionId` / `base_version_id` 均可 |
| `recipe.version.activate` | 命令 | `recipe_activate()` | `HandleRecipeActivate` | `RECIPE_VERSION_ACTIVATE` | `activated` | HMI 将其作为激活/回滚入口 |
| `recipe.audit` | 查询 | `recipe_audit()` | `HandleRecipeAudit` | `RECIPE_AUDIT` | `records[]` | `versionId` 可选 |
| `recipe.export` | 命令 | `recipe_export()` | `HandleRecipeExport` | `RECIPE_EXPORT` | `outputPath` / `recipeCount` / `versionCount` / `auditCount` / `bundleJson?` | 指定输出路径时不返回 `bundleJson` |
| `recipe.import` | 命令 | `recipe_import()` | `HandleRecipeImport` | `RECIPE_IMPORT` | `status` / `importedCount` / `conflicts[]` | `bundleJson` / `bundlePath` 与 `dryRun` 均保留 snake_case 兼容 |

## 额外观察

- `recipe.*` 是当前别名兼容最密集的一组协议。
- `dxf.*` 当前正式执行链已收敛到 `artifact.create -> plan.prepare -> preview.snapshot -> preview.confirm -> job.start -> job.status`。
- runtime-execution 对跨模块公开面已收敛为 `DispensingExecutionRequest + DispensingExecutionResult + job API`；`task` 只允许留在 runtime-execution 内部实现或内部测试语境。
- `status.machine_state` / `machine_state_reason` 仍需保留，但语义上已经降级为由 `supervision.current_state` / `state_reason` 单向派生的 compat 面，并已收敛到 `IRuntimeStatusExportPort` snapshot。
- `status` 当前整体由 `IRuntimeStatusExportPort` snapshot 提供；其中 `connected` / `connection_state` / `interlock_latched` / `active_job_*` / `supervision` / `effective_interlocks` / `io` 由 export snapshot 统一导出，底层 supervision 语义来自 `IRuntimeSupervisionPort` 输入，`runtime-gateway` 只负责 transport 序列化。
- HMI 仅在 `status.supervision` 整体缺失时回退读取 `machine_state` compat 字段；不会再对 `supervision` 单个字段做 compat 回填。
- `status` 与 `alarms.*` 结构已被 HMI UI 直接依赖，字段改名风险高。
