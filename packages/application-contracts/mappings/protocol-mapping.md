# 当前协议映射清单

以下清单只记录当前代码里已经存在的协议事实，不推测未来命名。

## `status`

| 方法 | 类型 | HMI 调用点 | TCP 处理器 | CLI 对应语义 | 结果字段 | 兼容说明 |
|---|---|---|---|---|---|---|
| `status` | 查询 | `CommandProtocol.get_status()` | `HandleStatus` | `HandleStatus` | `connected` / `machine_state` / `axes` / `position` / `io` / `dispenser` / `alarms` | HMI 当前主要消费 `machine_state`、`axes`、`io`、`dispenser.*` |

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
| `dxf.execute` | 命令 | `CommandProtocol.dxf_execute()` | `HandleDxfExecute` | `DXF_DISPENSE` | `executing` / `task_id` | 旧速度字段 `speed` / `dry_run_speed` / `dryRunSpeed` 被明确拒绝；当 `require_active_recipe=true` 且无激活配方时返回 `3004` |
| `dxf.stop` | 命令 | `CommandProtocol.dxf_stop()` | `HandleDxfStop` | 无独立 CLI facade | `stopped` | 会取消现有 task 并清空缓存 task id |
| `dxf.info` | 查询 | `CommandProtocol.dxf_get_info()` | `HandleDxfInfo` | `DXF_PLAN` 结果侧等价 | `total_length` / `total_segments` / `bounds` | 与 HMI UI 段数读取字段已对齐 |
| `dxf.progress` | 查询 | `CommandProtocol.dxf_get_progress()` | `HandleDxfProgress` | 无 | `running` / `progress` / `current_segment` / `total_segments` / `state` / `error_message` | HMI UI 与 TCP 对该结构目前一致 |

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
- `dxf.*` 当前命令和查询都已经跨 HMI / CLI / TCP 共用，`dxf.info` 与 HMI 段数字段已对齐。
- `status` 与 `alarms.*` 结构已被 HMI UI 直接依赖，字段改名风险高。
