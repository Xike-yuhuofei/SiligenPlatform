# SiligenSuite WORKSPACE

更新时间：`2026-03-25`

## 1. 目的

本文件冻结仓库根目录的工作区结构、目录职责和单轨约束，防止后续实现把历史承载面重新引回主线。

## 2. target canonical roots

- `apps/`
- `modules/`
- `shared/`
- `docs/`
- `samples/`
- `tests/`
- `scripts/`
- `config/`
- `data/`
- `deploy/`

## 3. legacy roots（已退出）

- `packages/`（已物理删除）
- `integration/`（已物理删除）
- `tools/`（已物理删除；`no-loose-mock` 已迁到 `scripts/testing/`）
- `examples/`（已物理删除）

本地缓存（允许存在但不纳入正式基线）：

- `.specify/`（本地工具缓存，默认忽略）
- `specs/`（本地工具缓存，默认忽略）
- `.claude/`（本地工具状态目录，默认忽略）
- `build-*/`（根级临时构建目录，默认忽略）

约束：

- 任何新变更不得重新创建 legacy roots。
- 旧根路径不得作为默认入口、默认脚本路径或正式文档锚点。

## 4. 根级职责

| 根目录 | 正式职责 | 说明 |
|---|---|---|
| `apps/` | 进程入口、装配、发布壳 | 当前 canonical 入口为 `planner-cli`、`runtime-service`、`runtime-gateway`、`hmi-app`、`trace-viewer` |
| `modules/` | `M0-M11` 的唯一业务 owner 根 | 当前先以锚点和接口目标冻结 owner 落位 |
| `shared/` | 公共契约、ID、消息、failure payload、基础设施 | 不承载一级业务 owner |
| `docs/` | 架构冻结、运行说明、验收索引 | 以 `dsp-e2e-spec/` 为唯一正式冻结入口 |
| `samples/` | golden cases、fixtures、稳定样本 | 唯一样本承载根 |
| `tests/` | contracts/e2e/performance/protocol-compatibility 验证根 | 唯一测试与验证承载根 |
| `scripts/` | 自动化、迁移脚本、构建辅助 | 唯一脚本与迁移承载根 |
| `config/` | 版本化配置 | 默认机器与环境配置源 |
| `data/` | 配方、schema、运行资产 | 默认数据源 |
| `deploy/` | 部署与交付材料 | 发布清单、交付约束和环境说明 |

## 5. 当前 live 入口

| live 入口 | 角色 | 说明 |
|---|---|---|
| `apps/planner-cli/` | 规划侧 CLI 入口 | 生产口径 |
| `apps/runtime-service/` | 运行时宿主入口 | 生产口径 |
| `apps/runtime-gateway/` | 网关与 TCP 入口 | 生产口径 |
| `apps/hmi-app/` | HMI 进程入口 | 与 `modules/hmi-application/` 协同 |
| `apps/trace-viewer/` | 轨迹/追踪观测入口 | 观测与诊断 |

## 6. 执行规则

1. 根级命令只认 `build.ps1`、`test.ps1`、`ci.ps1` 与 `scripts/validation/run-local-validation-gate.ps1`。
2. `docs/architecture/dsp-e2e-spec/` 是阶段、对象、模块、控制语义和 acceptance baseline 的唯一正式入口。
3. 任何目录调整必须先更新 `S05`、`S06`、`S09`、`S10` 和对应验证脚本。
4. 分支命名必须符合 `<type>/<scope>/<ticket>-<short-desc>`，并统一以项目级 skill `.agents/skills/git-create/SKILL.md` 为准。
5. `.specify/` 与 `specs/` 如存在，只能作为本地缓存；不得作为正式文档锚点、正式脚本输入或仓库事实源。
6. 根级 `build-*` 与 `.claude/` 如存在，只能作为本地生成物或工具状态；不得进入版本管理，也不得被正式脚本声明为固定事实路径。
7. control-apps build root 默认只允许显式 `SILIGEN_CONTROL_APPS_BUILD_ROOT` 或当前工作区 `build/ca`；`build/`、`build/control-apps/` 和本地发布缓存不得作为默认 build root 候选。
