# Canonical Paths

更新时间：`2026-03-18`

## 1. 分类定义

本文档使用五类路径标签：

- `effective canonical`
- `transitional canonical`
- `legacy`
- `compatibility shell`
- `removed`

## 2. Effective Canonical

| 能力 | 路径 | 状态 | 备注 |
|---|---|---|---|
| HMI 应用 | `apps/hmi-app` | `effective canonical` | 有真实源码、脚本、测试。 |
| 工程数据预处理 | `packages/engineering-data` | `effective canonical` | 有 Python 包、脚本、兼容测试。 |
| 运动仿真 | `packages/simulation-engine` | `effective canonical` | 已收口原顶层仿真源码、示例、测试与根级 build 入口。 |
| 应用契约 | `packages/application-contracts` | `effective canonical` | 有 commands/queries/models/fixtures/tests。 |
| 工程契约 | `packages/engineering-contracts` | `effective canonical` | 有 proto/schema/fixtures/tests。 |
| 运行时宿主实现 | `packages/runtime-host` | `effective canonical` | 有真实 C++ 源码并参与 `control-core` 构建。 |
| 传输网关实现 | `packages/transport-gateway` | `effective canonical` | 有真实 C++ 源码并参与 `control-core` 构建。 |
| 工作区验证工具 | `packages/test-kit` | `effective canonical` | 有统一 runner 与报告模型。 |
| 集成验证目录 | `integration/` | `effective canonical` | 已有协议、工程回归、sim/HIL 脚本。 |
| 外部 DXF 编辑说明 | `docs/runtime/external-dxf-editing.md` | `effective canonical` | 取代内建编辑器与 editor contracts。 |

## 3. Transitional Canonical

| 能力 | 路径 | 状态 | 当前缺口 |
|---|---|---|---|
| Runtime app 入口 | `apps/control-runtime` | `transitional canonical` | 只有 wrapper；当前无独立可执行文件。 |
| TCP server 入口 | `apps/control-tcp-server` | `transitional canonical` | 根级入口存在，但真实 C++ `main.cpp` 仍在 `control-core/apps/control-tcp-server`。 |
| CLI 入口 | `apps/control-cli` | `transitional canonical` | 只有 wrapper；源码迁移仍缺失。 |
| 过程运行时核心 | `packages/process-runtime-core` | `transitional canonical` | 当前以边界说明、INTERFACE target、测试入口为主，主体实现仍在 `control-core`。 |
| 设备适配 | `packages/device-adapters` | `transitional canonical` | 仅接管部分驱动/IO/fake；`device-hal` 仍保留大量混合实现。 |
| 设备契约 | `packages/device-contracts` | `transitional canonical` | 已有头文件和 CMake，但仍缺独立测试闭环。 |
| 配置目录 | `config/` | `transitional canonical` | 目录已建；真实配置文件仍主要在 `control-core`。 |
| 数据目录 | `data/` | `transitional canonical` | 目录已建；真实 recipes 资产仍主要在 `control-core/data/recipes`。 |

## 4. Legacy Paths

| 能力 | 路径 | 当前角色 | 对应 canonical |
|---|---|---|---|
| C++ 主控主体实现 | `control-core/src/domain` | 仍是 process/runtime 主要实现承载 | `packages/process-runtime-core` |
| C++ 应用层主体实现 | `control-core/src/application` | 仍是 process/runtime 主要实现承载 | `packages/process-runtime-core` / `packages/runtime-host` |
| 过程核心切片 | `control-core/modules/process-core` | 仍被构建并提供 recipe/process 相关 target | `packages/process-runtime-core` |
| 运动/安全切片 | `control-core/modules/motion-core` | 仍被构建并提供 motion/safety 相关 target | `packages/process-runtime-core` |
| 设备混合实现 | `control-core/modules/device-hal` | 仍承载纯设备外的 recipes/diagnostics 等实现 | `packages/device-adapters` / `packages/traceability-observability` |
| 共享基础实现 | `control-core/modules/shared-kernel` | 当前真实共享内核代码所在 | `packages/shared-kernel` |
| 机器配置 | `control-core/config/machine_config.ini` | 当前真实机器配置文件 | `config/machine/` |
| 配方资产 | `control-core/data/recipes` | 当前真实 recipes 资产目录 | `data/recipes/` |
| HMI 旧项目名目录 | `hmi-client` | 仍保留整套重复源码，不是纯薄壳 | `apps/hmi-app` |

## 5. Compatibility Shell Paths

| 路径 | 当前职责 | 不应再做的事 | 对应 canonical |
|---|---|---|---|
| `control-core/apps/control-runtime` | 保留旧 target 名称并转发到 `siligen_runtime_host` | 不应继续沉淀宿主源码 | `apps/control-runtime` + `packages/runtime-host` |
| `control-core/modules/control-gateway` | 保留旧 target 名称并转发到 `siligen_transport_gateway_protocol` | 不应继续放 TCP 会话、协议实现或 facade 主逻辑 | `packages/transport-gateway` |
| `control-core/src/adapters/tcp` | 保留 adapter alias target | 不应继续扩展 TCP adapter 实现 | `packages/transport-gateway` + `apps/control-tcp-server` |
| `dxf-pipeline` | 保留旧 CLI 名称与旧 Python 导入路径；内部转发到 `engineering-data` | 不应继续把新算法落回旧包 | `packages/engineering-data` + `packages/engineering-contracts` |
| `hmi-client/scripts/run.ps1` / `hmi-client/scripts/test.ps1` | 转发到根级 HMI app | 不应重新成为官方入口 | `apps/hmi-app` |

## 6. Removed Paths

| 能力 | 路径 | 当前状态 |
|---|---|---|
| 内建 DXF 编辑器应用 | `apps/dxf-editor-app` | 已删除；改为外部编辑器人工流程。 |
| DXF 编辑器旧项目名目录 | `dxf-editor` | 已删除；不再保留兼容壳。 |
| 编辑器协作契约 | `packages/editor-contracts` | 已删除；原 notify / CLI 协议已废弃。 |

## 7. 当前路径使用准则

1. 新的根级文档、自动化和治理规则，应优先引用 `effective canonical` 路径。
2. 对 `transitional canonical` 路径，允许继续使用，但必须同时注明真实实现仍在何处。
3. `legacy` 路径不得静默删除；只有在替代路径完成验证后才能进入后续收口阶段。
4. 已删除路径不得再出现在默认命令、默认文档、测试入口和发布门禁中。
