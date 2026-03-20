# SiligenSuite WORKSPACE

## 1. 目的

本文件定义 `D:\Projects\SiligenSuite` 根级 monorepo 工作区骨架、治理边界和当前兼容口径。

当前阶段目标是：

- 建立根级 canonical 目录
- 固定后续任务的稳定落位
- 在不做大规模业务迁移的前提下统一治理入口

当前阶段不做：

- 在未完成真实迁移前静默删除现有子项目
- 一步到位重写所有构建文件
- 把 `control-core`、`dxf-pipeline` 整体搬入新目录

## 2. 当前状态

当前根目录同时存在两类路径：

1. 现有实现承载路径

- `control-core`
- `dxf-pipeline`

2. 新增 canonical 工作区路径

- `apps/`
- `packages/`
- `integration/`
- `tools/`
- `docs/`
- `config/`
- `data/`
- `examples/`

## 3. 根级目录职责

| 目录 | 职责 | 当前阶段使用方式 |
|---|---|---|
| `apps/` | 可运行入口、壳层装配、启动封装 | 当前承接 HMI 与控制侧入口 |
| `packages/` | 可复用业务能力、契约、宿主支撑、公共基础 | 当前承接工程数据、应用契约、工程契约、运行时与测试工具 |
| `integration/` | 跨包场景回归、协议兼容、联调验证 | 当前已有工程回归、协议兼容、HIL/SIL 脚本 |
| `tools/` | 构建、脚本、codegen、迁移工具 | 当前承接根级 build/test/install 治理入口 |
| `docs/` | 根级架构与治理文档 | 当前承接工作区规范、部署、排障与外部 DXF 编辑说明 |
| `config/` | 构建/CI/环境/机器配置资产 | 当前作为默认说明路径 |
| `data/` | 配方、schema、基线等版本化资产 | 当前作为默认说明路径 |
| `examples/` | 示例输入、样例输出、演示场景 | 当前作为工作区样例目录 |

## 4. 当前兼容口径

- `packages/runtime-host` 已是当前运行时宿主的真实实现路径；`control-core/apps/control-runtime` 仅保留旧 target 名称和文档兼容壳。
- `packages/transport-gateway` 已是当前 TCP/JSON 传输实现的真实路径；`control-core/apps/control-tcp-server` 仅保留薄启动入口，`control-core/modules/control-gateway` 与 `control-core/src/adapters/tcp` 仅保留兼容壳。
- `apps/hmi-app` 已是当前 HMI 的真实源码根；历史 `hmi-client` 目录已删除并归档到 `docs/_archive/2026-03/hmi-client/`。
- `packages/simulation-engine` 已完成对原顶层 `simulation-engine` 的源码收口，当前由 package 承接真实实现与构建入口。
- `control-cli` 当前没有新式源码目录，后续以旧仓 `Backend_CPP/src/adapters/cli` 为迁移源，对齐到 `apps/control-cli`。
- `dxf-editor`、`apps/dxf-editor-app`、`packages/editor-contracts` 已退出工作区默认能力版图；DXF 编辑改为外部编辑器人工流程。

## 5. 依赖与迁移入口

工作区级治理规则见以下文档：

- `docs/architecture/directory-responsibilities.md`
- `docs/architecture/dependency-rules.md`
- `docs/architecture/migration-principles.md`
- `docs/runtime/external-dxf-editing.md`

## 6. 后续任务执行原则

1. 新增治理/契约/入口骨架时，优先放根级 canonical 路径。
2. 现有业务实现的局部修复，仍可以留在旧子项目，但不能借机扩张旧目录边界。
3. DXF 编辑相关需求不再创建新的 app 或 contracts 包，统一按外部编辑器人工流程说明处理。
4. Git 分支命名必须使用统一格式：`<type>/<scope>/<ticket>-<short-desc>`；无任务号时可临时使用 `NOISSUE` 占位。

## 7. 协作命名约定（强制）

- 分支命名格式：`<type>/<scope>/<ticket>-<short-desc>`
- `type` 允许值：`feat`、`fix`、`chore`、`refactor`、`docs`、`test`、`hotfix`、`spike`、`release`
- `scope` 使用模块短名，例如 `hmi`、`runtime`、`cli`、`gateway`
- `ticket` 优先使用任务系统编号（如 `SS-142`），确无编号时使用 `NOISSUE`
- `short-desc` 使用英文小写 kebab-case
