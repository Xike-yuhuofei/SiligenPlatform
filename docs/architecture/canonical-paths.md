# Canonical Paths

更新时间：`2026-03-19`

## 1. 分类定义

本文档使用四类路径标签：

- `effective canonical`
- `transitional canonical`
- `legacy source owner`
- `removed`

## 2. Effective Canonical

| 能力 | 路径 | 备注 |
|---|---|---|
| HMI 应用 | `apps/hmi-app` | 有真实源码、脚本、测试。 |
| Runtime app 入口 | `apps/control-runtime` | 有真实 `main.cpp`、`run.ps1` 与独立可执行文件。 |
| TCP server 入口 | `apps/control-tcp-server` | 有真实 `main.cpp`、`run.ps1`；HIL 默认已切到该入口。 |
| 机器配置 | `config/` | `config/machine/machine_config.ini` 已是默认配置源。 |
| 配方/Schema 数据 | `data/` | `data/recipes/`、`data/schemas/recipes/` 已是默认数据源。 |
| 运行时宿主实现 | `packages/runtime-host` | 真实 C++ 宿主实现。 |
| 传输网关实现 | `packages/transport-gateway` | 真实 TCP/JSON 协议与 facade/wiring 实现；兼容 alias 也从这里导出。 |
| 过程运行时核心公开入口 | `packages/process-runtime-core` | 公开 target、测试入口与核心源码承接目录均在此；`control-core` 的四个 legacy 子树已删除。 |
| shared-kernel | `packages/shared-kernel` | 真实共享基础源码 owner；legacy `control-core/modules/shared-kernel` 已删除。 |
| 工作区验证工具 | `packages/test-kit` | 有统一 runner 与报告模型。 |
| 集成验证目录 | `integration/` | HIL、协议、工程回归、sim 脚本都从这里发起。 |

## 3. Transitional Canonical

| 能力 | 路径 | 当前缺口 |
|---|---|---|
| CLI 入口 | `apps/control-cli` | 默认产物已 canonical，但命令面仍未完全迁入。 |
| 设备适配 | `packages/device-adapters` | 仅接管部分驱动/IO/fake；`device-hal` 仍保留大量混合实现。 |
| 设备契约 | `packages/device-contracts` | 已有头文件和 CMake，但仍缺独立测试闭环。 |

## 4. Legacy Source Owner

| 能力 | 路径 | 当前角色 | 对应 canonical |
|---|---|---|---|
| shared compat include root | `control-core/src/shared` | 仍承接 `shared/*` 兼容头与共享类型桥接 | 后续独立迁移 |
| infrastructure / DXF parsing | `control-core/src/infrastructure` | 仍承接 parsing adapter、日志/持久化装配等基础设施实现 | `packages/runtime-host` / `packages/engineering-data` / 后续拆分 |
| 设备混合实现 | `control-core/modules/device-hal` | 仍承载纯设备外的 recipes/diagnostics 等实现 | `packages/device-adapters` / `packages/traceability-observability` |
| `third_party` / PCH / 顶层库图 | `control-core` | 仍是当前 C++ 顶层构建 owner | 后续独立迁移 |
| `control-core/config/*` | `control-core` | 历史残留，不再是默认配置入口 | `config/` |
| `control-core/data/recipes/*` | `control-core` | 历史残留，不再是默认数据入口 | `data/` |
| `control-core/src/adapters/tcp` / `modules/control-gateway` | `control-core` | 目录仍在，但已退出默认 alias 注册链路 | `packages/transport-gateway` |

## 5. Removed Paths

| 能力 | 路径 | 当前状态 |
|---|---|---|
| 内建 DXF 编辑器应用 | `apps/dxf-editor-app` | 已删除；改为外部编辑器人工流程。 |
| DXF 编辑器旧项目名目录 | `dxf-editor` | 已删除；不再保留兼容壳。 |
| 编辑器协作契约 | `packages/editor-contracts` | 已删除；原 notify / CLI 协议已废弃。 |
| Runtime legacy app 兼容壳 | `control-core/apps/control-runtime` | 已删除；根级 `apps/control-runtime` 成为唯一 app 入口。 |
| TCP server legacy app 兼容壳 | `control-core/apps/control-tcp-server` | 已删除；根级 `apps/control-tcp-server` 成为唯一 app 入口。 |
| Shared-kernel legacy 子树 | `control-core/modules/shared-kernel` | 已删除；canonical owner 为 `packages/shared-kernel`。 |
| process-runtime-core legacy domain 子树 | `control-core/src/domain` | 已删除；canonical owner 为 `packages/process-runtime-core`。 |
| process-runtime-core legacy application 子树 | `control-core/src/application` | 已删除；canonical owner 为 `packages/process-runtime-core` / `packages/runtime-host`。 |
| process-runtime-core legacy process 子树 | `control-core/modules/process-core` | 已删除；canonical owner 为 `packages/process-runtime-core`。 |
| process-runtime-core legacy motion 子树 | `control-core/modules/motion-core` | 已删除；canonical owner 为 `packages/process-runtime-core`。 |
| 顶层 legacy 仿真目录 | `simulation-engine/` | 已删除；默认入口稳定指向 `packages/simulation-engine`。 |
