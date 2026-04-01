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

- `dxf.load`: `filepath` / `file_path`
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

- `status.machine_state` / `machine_state_reason` 仍保留为兼容导出面；当前由 `IRuntimeStatusPort` snapshot 内的 `supervision` 单向派生，HMI 主显示与门禁已优先消费 `status.supervision` 与 `effective_interlocks`，不再把 `machine_state` 视为主真相。
- `status` 的 owner 数据面当前来自 `IRuntimeStatusPort` snapshot；其中 `supervision` / `effective_interlocks` / `io` 继续由内嵌 `IRuntimeSupervisionPort` snapshot 提供，`runtime-gateway` 只承担协议序列化，不再本地派生 `machine_state` compat 字段。
- HMI 仅在 `status.supervision` 整体缺失时回退到 `machine_state` compat 面，避免对 `supervision` 单字段回填形成双向真相。
- 当前事件 envelope 已存在，但未发现已稳定发布的具体事件名。
