# Workspace Baseline

更新时间：`2026-03-19`

## 1. 当前整仓真实形态

当前仓库已经不是“只有 5 个并列子项目”的状态，而是根级 monorepo 目录与 legacy 承载体并行的混合形态：

1. 根级 `apps/`、`packages/`、`integration/`、`tools/`、`docs/`、`config/`、`data/`、`examples/` 已建立。
2. `control-core` 仍然是当前 C++ 主控核心的大型实现承载体。
3. `apps/hmi-app`、`packages/engineering-data`、`packages/runtime-host`、`packages/transport-gateway`、`packages/application-contracts`、`packages/engineering-contracts`、`packages/test-kit` 与 `integration/` 已具备实际内容。
4. `dxf-editor`、`apps/dxf-editor-app`、`packages/editor-contracts` 已退出工作区默认链路；DXF 编辑改为外部编辑器人工流程。

## 2. 当前能力分布

| 能力域 | 当前首选路径 | 当前真实承载 | 当前状态 |
|---|---|---|---|
| HMI 应用 | `apps/hmi-app` | `apps/hmi-app` | `可作为正式开发入口；legacy hmi-client 已删除并归档` |
| DXF 编辑 | `docs/runtime/external-dxf-editing.md` | 外部编辑器人工流程 | `已退出应用能力版图` |
| 工程数据预处理 | `packages/engineering-data` | `packages/engineering-data` | `可作为正式开发入口` |
| 工程契约 | `packages/engineering-contracts` | `packages/engineering-contracts` | `可作为正式开发入口` |
| 应用契约 | `packages/application-contracts` | `packages/application-contracts` | `可作为正式开发入口` |
| 运行时宿主实现 | `packages/runtime-host` | `packages/runtime-host` | `可作为正式开发入口` |
| TCP/JSON 网关实现 | `packages/transport-gateway` | `packages/transport-gateway` | `可作为正式开发入口` |
| TCP server 应用入口 | `apps/control-tcp-server` | `apps/control-tcp-server` + `packages/transport-gateway` + `packages/runtime-host` | `可作为正式开发入口` |
| CLI 应用入口 | `apps/control-cli` | `apps/control-cli` + 最小 canonical `siligen_cli.exe` 命令面；未迁移命令仅显式 fallback | `过渡态` |
| Runtime 应用入口 | `apps/control-runtime` | `apps/control-runtime` + `packages/runtime-host` | `可作为正式开发入口` |
| 过程运行时核心 | `packages/process-runtime-core` | 边界层 + `control-core/src/*`/`modules/*` 实现 | `结构已建/能力未完成` |
| 设备契约与适配 | `packages/device-contracts` / `packages/device-adapters` | 仍部分依赖 `control-core` | `过渡态` |
| 仿真引擎 | `packages/simulation-engine` | `packages/simulation-engine` | `可作为正式开发入口` |
| 工作区级验证 | `packages/test-kit` + `integration` | `packages/test-kit` + `integration` | `可作为正式开发入口` |

## 3. 当前可验证事实

- `apps/control-runtime/run.ps1 -DryRun` 当前输出 `source: canonical`。
- `apps/control-tcp-server/run.ps1 -DryRun` 当前输出 `source: canonical`。
- `apps/control-cli/run.ps1 -DryRun` 当前输出 `source: canonical`。
- `test.ps1 -Profile Local -Suite apps` 当前会同时验证 dry-run 与真实 exe：`siligen_control_runtime.exe --version`、`siligen_tcp_server.exe --help`、`siligen_cli.exe bootstrap-check`。

结论：

`SiligenSuite` 已有根级 monorepo 基线和一部分真实 canonical 代码，但整仓仍处于“canonical 路径与 legacy 承载体并行”的迁移中期。
