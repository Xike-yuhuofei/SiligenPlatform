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
| `data/` | 配方、schema、运行资产 | 上传缓存 |
| `deploy/` | 部署材料、交付约束 | 业务实现源码 |

补充规则：

- `apps/` 的终态集合以 `planner-cli`、`runtime-service`、`runtime-gateway`、`hmi-app`、`trace-viewer` 一类进程入口或装配面为准。
- `shared/` 允许承载 `ids`、`artifacts`、`contracts`、`commands`、`events`、`failures`、`messaging`、`logging`、`config`、`testing`，但不得承载一级业务 owner。
- `tests/`、`samples/`、`scripts/`、`deploy/` 在 `Wave 1` 之后必须成为正式承载面，而不是长期占位目录。

## 2. migration-source roots 与 bridge 约束

| 根目录 | 当前角色 | canonical destination | 允许 bridge 形态 | 退出信号 |
|---|---|---|---|---|
| `packages/` | 当前实现与契约迁移来源 | `modules/`、`shared/`、目标应用面 | README、wrapper、forwarding include/CMake、thin bridge | 对应 owner 已迁入 target root，旧包只剩兼容壳。 |
| `integration/` | 当前验证与报告迁移来源 | `tests/` | README redirect、test harness forwarder | `tests/` 成为唯一仓库级验证承载面。 |
| `tools/` | 当前自动化迁移来源 | `scripts/` | root wrapper、入口转发脚本 | 根级稳定入口切向 `scripts/`，`tools/` 只剩兼容入口。 |
| `examples/` | 当前样本迁移来源 | `samples/` | README redirect、tombstone | `samples/` 成为唯一稳定样本承载面。 |

补充规则：

- `migration-source` 根中的实现只表示“当前承载”，不表示终态 owner，也不允许新增业务逻辑。
- bridge 必须显式、可审计、可退出；禁止依赖未声明的 implicit fallback。
- bridge 退出判定统一回链到 `wave-mapping.md` 的退出波次和 `validation-gates.md` 的根级门禁，而不是依赖口头约定。

## 3. 模块 owner 落位

| 模块 | target owner path | 当前 fact cluster |
|---|---|---|
| `M0` | `modules/workflow/` | `packages/process-runtime-core/src/application` |
| `M1` | `modules/job-ingest/` | `apps/hmi-app`, `apps/control-cli`, `packages/application-contracts` |
| `M2` | `modules/dxf-geometry/` | `packages/engineering-data`, `packages/engineering-contracts` |
| `M3` | `modules/topology-feature/` | `packages/engineering-data` |
| `M4` | `modules/process-planning/` | `packages/process-runtime-core/planning`, `packages/process-runtime-core/src/domain` |
| `M5` | `modules/coordinate-alignment/` | `packages/process-runtime-core/planning`, `packages/process-runtime-core/machine` |
| `M6` | `modules/process-path/` | `packages/process-runtime-core/planning` |
| `M7` | `modules/motion-planning/` | `packages/process-runtime-core/src/domain/motion` |
| `M8` | `modules/dispense-packaging/` | `packages/process-runtime-core/src/domain/dispensing`, `packages/process-runtime-core/src/application/usecases/dispensing` |
| `M9` | `modules/runtime-execution/` | `packages/runtime-host`, `packages/device-contracts`, `packages/device-adapters`, `apps/control-runtime`, `apps/control-tcp-server` |
| `M10` | `modules/trace-diagnostics/` | `packages/traceability-observability`, `docs/validation/`, `integration/` |
| `M11` | `modules/hmi-application/` | `apps/hmi-app`, `apps/control-cli`, `packages/application-contracts` |

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
- `modules/` 是唯一正式 owner 面；迁移来源根中的实现只表示当前承载，不表示终态 owner。
- `tests/` 与 `docs/validation/` 负责 evidence，不回写业务事实。
- `shared/`、`cmake/`、`third_party/`、`build/`、`logs/`、`uploads/` 不能升级为 formal owner 根。
- 任何从 `migration-source`、`support`、`vendor`、`generated` 回流到正式 owner 面的调整，都必须先更新冻结文档与门禁定义，再通过根级验证。
