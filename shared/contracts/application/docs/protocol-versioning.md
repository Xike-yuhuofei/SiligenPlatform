# 协议版本策略

## 当前事实

- TCP 响应与事件当前固定写入 `version: "1.0"`。
- HMI 请求 envelope 当前不显式携带版本号，只发送 `id`、`method`、`params`。
- 旧仓 `Backend_CPP/src/adapters/tcp/JsonProtocol.cpp` 与当前 `apps/runtime-gateway/transport-gateway/src/protocol/json_protocol.cpp` 的 envelope 行为一致。

## 兼容规则

1. 不删除已公开的 `method` 名。
2. 已存在的别名字段必须显式记录并在迁移期间保留。
3. 允许新增可选字段，不允许无记录地改变已有字段含义。
4. 如果 HMI 与 TCP 对同一字段理解不一致，先保留兼容层并记录，再决定收敛方案。

## 已记录的兼容层

- `recipe.*`: `recipeId` / `recipe_id`
- `recipe.*version*`: `versionId` / `version_id`
- `recipe.draft.create`: `templateId` / `template_id`
- `recipe.*`: `baseVersionId` / `base_version_id`
- `recipe.*`: `versionLabel` / `version_label`
- `recipe.*`: `changeNote` / `change_note`
- `recipe.export`: `outputPath` / `output_path`
- `recipe.import`: `bundleJson` / `bundle_json`
- `recipe.import`: `bundlePath` / `bundle_path`
- `recipe.import`: `dryRun` / `dry_run`

## 当前未对齐但已记录

- `status` 的 owner 数据面当前来自 `IRuntimeStatusExportPort` snapshot；其中 `connected` / `connection_state` / `device_mode` / `interlock_latched` / `job_execution` / `supervision` / `effective_interlocks` / `io` 由 export snapshot 统一导出，底层 supervision 语义仍来自 `IRuntimeSupervisionPort` 输入，`runtime-gateway` 只承担协议序列化。
- `status` 当前继续保留 `machine_state` / `machine_state_reason` compat 面，语义镜像 `status.supervision.current_state` / `status.supervision.state_reason`；HMI 主显示与门禁统一依赖 `status.supervision` 与 `effective_interlocks`。
- `status.device_mode` 当前是 V1 派生字段：`job_execution.dry_run=true` 时导出 `test`，其余导出 `production`；在独立 device mode owner 落地前，不表达 HMI 启动模式或离线模式。
- `status.safety_boundary` 当前是 V1 软件动作准入快照：基于 `effective_interlocks` / `io` / `interlock_latched` / `device_mode` / `job_execution.dry_run` 单向派生，只表达运动与真实工艺输出是否准入，不等同功能安全认证状态；gateway 直接透传，HMI 在字段缺失时按同一规则兼容派生。
- `status.action_capabilities` 当前是 V1 后端粗粒度动作能力摘要：基于 `connected` / `connection_state` / `safety_boundary` / `job_execution` 单向派生，只表达网关侧是否适合继续向后端发起常规运动命令、手动输出命令，以及后端是否存在活动 job；不包含 HMI 本地 pending、worker、session 门禁。
- 当前事件 envelope 已存在，但未发现已稳定发布的具体事件名。
