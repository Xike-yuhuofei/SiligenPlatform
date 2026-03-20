# ADR-001 Workspace Baseline

- 状态：`Accepted`
- 日期：`2026-03-18`

## 决策

本仓库从 `2026-03-18` 起采用以下基线判定规则：

1. `canonical` 必须以当前代码事实、构建事实、测试事实为准。
2. 所有路径统一分为五类：`effective canonical`、`transitional canonical`、`legacy`、`compatibility shell`、`removed`。
3. DXF 编辑器能力已退出工作区默认版图：
   - `apps/dxf-editor-app` 已删除
   - `dxf-editor` 已删除
   - `packages/editor-contracts` 已删除
   - 当前改为外部编辑器人工流程

## 当前基线认定

- `apps/hmi-app`、`packages/engineering-data`、`packages/runtime-host`、`packages/transport-gateway`、`packages/application-contracts`、`packages/engineering-contracts`、`packages/test-kit`、`integration/` 为当前 `effective canonical`。
- `apps/control-runtime`、`apps/control-tcp-server`、`apps/control-cli`、`packages/process-runtime-core`、`packages/device-adapters`、`packages/device-contracts`、`config/`、`data/` 为 `transitional canonical`。
- `control-core` 仍是 C++ 主控的主要 legacy 实现承载体。
- `hmi-client` 在本 ADR 建立时不是纯薄壳，而是保留了整套重复源码的 legacy 目录；当前该目录已删除，历史材料已归档到 `docs/_archive/2026-03/hmi-client/`。
