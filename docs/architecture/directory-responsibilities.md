# 目录职责

## 1. target canonical roots

| 根目录 | 正式职责 | 不应承载 |
|---|---|---|
| `apps/` | 进程入口、装配、发布壳 | 一级业务 owner |
| `modules/` | `M0-M11` 的正式 owner 根 | 发布壳和验证报告 |
| `shared/` | 公共契约、kernel、testing 与基础设施 | 业务编排和业务事实 owner |
| `docs/` | 冻结文档、运行说明、审阅索引 | 运行态事实源 |
| `samples/` | golden cases、稳定 fixtures | 临时报告和派生缓存 |
| `tests/` | integration/e2e/performance/contract-lint 承载面 | 一级业务实现 |
| `scripts/` | 根级自动化、迁移脚本、构建辅助 | 业务 owner |
| `config/` | 版本化配置 | 运行时临时文件 |
| `data/` | 工程 schema、运行资产 | 上传缓存 |
| `deploy/` | 部署材料、交付约束 | 业务实现源码 |

补充规则：

- `apps/` 的终态集合以 `planner-cli`、`runtime-service`、`runtime-gateway`、`hmi-app`、`trace-viewer` 一类进程入口或装配面为准。
- `shared/` 允许承载 `ids`、`artifacts`、`contracts`、`commands`、`events`、`failures`、`messaging`、`logging`、`config`、`testing`，但不得承载一级业务 owner。
- `tests/`、`samples/`、`scripts/`、`deploy/` 在 `Wave 1` 之后必须成为正式承载面，而不是长期占位目录。

## 2. 已退出 legacy roots 与 bridge 约束

| 根目录 | 当前状态 | 当前正式去向 | 允许保留的描述方式 |
|---|---|---|---|
| `packages/` | 已退出 current root set | `modules/`、`shared/`、目标应用面 | 只允许作为历史迁移来源、兼容背景或审计对象被显式说明 |
| `integration/` | 已退出 current root set | `tests/`、`tests/reports/` | 只允许作为历史证据根被提及 |
| `tools/` | 已退出 current root set | `scripts/` | 只允许作为历史脚本根或兼容 wrapper 背景被提及 |
| `examples/` | 已退出 current root set | `samples/` | 只允许作为历史样本根被提及 |

补充规则：

- 已退出根不得再被写成“当前承载”或“默认入口”。
- bridge 必须显式、可审计、可退出；禁止依赖未声明的 implicit fallback。
- bridge 退出判定统一回链到 `wave-mapping.md` 的退出波次和 `validation-gates.md` 的根级门禁，而不是依赖口头约定。

## 3. 模块 owner 落位

| 模块 | target owner path | 当前正式关注点 |
|---|---|---|
| `M0` | `modules/workflow/` | orchestration 边界仍需继续收紧，避免重新吸入已退役管理面 / runtime / engineering concrete |
| `M1` | `modules/job-ingest/` | 负责上传语义与输入归一，不应被宿主或 UI 重新承接 owner 事实 |
| `M2` | `modules/dxf-geometry/` | DXF preprocess 与 PB 准备链当前以模块实现 + `shared/contracts/engineering/` 为准 |
| `M3` | `modules/topology-feature/` | 目录语义与 owner 面仍需继续收口 |
| `M4` | `modules/process-planning/` | owner 边界仍偏弱，不能被写成完整 `ProcessPlan` 唯一事实源 |
| `M5` | `modules/coordinate-alignment/` | 负责坐标/对位专属语义，不承接长期稳定工程契约 |
| `M6` | `modules/process-path/` | `ProcessPath` contract 与相关路径语义以模块 public surface 为准 |
| `M7` | `modules/motion-planning/` | `MotionTrajectory` contract 与插补程序规划语义以模块 public surface 为准 |
| `M8` | `modules/dispense-packaging/` | 负责点胶组包与执行程序装配，避免与 `workflow` 形成双 owner |
| `M9` | `modules/runtime-execution/` | 当前 owner 已收紧为执行域、runtime contracts 与 host core，不再概括为旧 runtime-host 聚合面 |
| `M10` | `modules/trace-diagnostics/` | evidence 与诊断契约边界仍需继续收口，不得把 `docs/validation/` 或历史证据根写成 owner |
| `M11` | `modules/hmi-application/` | HMI owner 面应在模块 public surface，不应退回 app 宿主侧承接业务语义 |

## 4. 正式脚本入口与门禁回链

| 入口 | 对外 contract | 治理角色 | 关键证据 |
|---|---|---|---|
| `build.ps1` | 根级构建口径保持稳定 | 对外入口保持不变，内部实现允许切向 `scripts/` 正式根 | `build/` 产物、`system-acceptance-report.md` |
| `test.ps1` | 根级测试口径保持稳定 | 仓库级验证入口，不因 owner 迁移改变调用方式 | `tests/reports/workspace-validation.*` |
| `ci.ps1` | CI 口径保持稳定 | 统一收口根级验证、布局与冻结文档校验 | `tests/reports/ci/` |
| `scripts/validation/run-local-validation-gate.ps1` | 本地冻结门禁 | 验证根治理、冻结文档与根级入口是否仍可闭环 | `tests/reports/local-validation-gate/<timestamp>/` |
| `scripts/migration/validate_dsp_e2e_spec_docset.py` | 冻结文档集契约校验 | 验证 `dsp-e2e-spec` 正式轴文档的完整性、术语与索引一致性 | `dsp-e2e-spec-docset/` 子目录报告 |

补充规则：

- 根级入口名称必须保持稳定；迁移允许改变内部实现归属，不允许改变对外 contract。
- 根治理是否通过，不看单个目录是否存在，而看 canonical root 文档、layout 解析、script bridge 和 freeze/layout validator 是否形成统一证据链。

## 5. 边界规则

- `apps/` 可以发命令、展示状态和承接装配，但不能直接成为核心运行时对象事实源。
- `modules/` 是唯一正式 owner 面；已退出根不得再承担“当前承载”或“当前 owner”语义。
- `tests/` 与 `docs/validation/` 负责 evidence，不回写业务事实。
- `shared/`、`cmake/`、`third_party/`、`build/`、`logs/`、`uploads/` 不能升级为 formal owner 根。
- 任何从 `migration-source`、`support`、`vendor`、`generated` 回流到正式 owner 面的调整，都必须先更新冻结文档与门禁定义，再通过根级验证。
