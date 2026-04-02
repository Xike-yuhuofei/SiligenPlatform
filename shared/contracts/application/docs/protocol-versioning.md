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

- `status.machine_state` 在 TCP 真服务端目前偏静态（`Ready`），而 HMI mock 中还出现过 `Idle` / `Running`。
- 当前事件 envelope 已存在，但未发现已稳定发布的具体事件名。
