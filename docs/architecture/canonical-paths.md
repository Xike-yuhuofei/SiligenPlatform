# Canonical Paths

更新时间：`2026-03-24`

## 1. 分类定义

| classification | 含义 | 治理约束 |
|---|---|---|
| `effective-canonical` | 正式 owner 根、正式承载面或正式入口面 | 必须先成为真实承载面，legacy root 才允许降级。 |
| `migration-source` | 迁移期间暂存当前事实的历史根 | 只允许显式 bridge，不得继续新增业务 owner。 |
| `feature-artifact` | 规格、计划、任务与 freeze/input 材料 | 只保留设计与治理事实，不承载运行时 owner。 |
| `support` | 构建、装配或辅助基础设施 | 只能支撑正式根，不能升级为默认 owner 根。 |
| `vendor` | 第三方依赖引入面 | 不承载项目业务事实。 |
| `generated` | 构建、验证或运行生成物 | 不是 source of truth，不允许回写正式事实。 |
| `archived` | 历史归档材料 | 不参与默认审阅链，不向当前实现回流。 |
| `removed` | 已退出或已删除的历史根 | 只保留退出结论，不重新开放实现入口。 |

## 2. 根级结构分类与处置

| 路径 | classification | main-chain relation | disposition | 说明 |
|---|---|---|---|---|
| `apps/` | `effective-canonical` | `adjacent` | `keep` | 应用入口、装配与发布面；不承载一级业务 owner。 |
| `modules/` | `effective-canonical` | `main-chain` | `migrate` | `M0-M11` 的唯一业务 owner 根。 |
| `shared/` | `effective-canonical` | `adjacent` | `keep` | 稳定公共契约、ID、envelope、failure payload 与低业务含义基础设施。 |
| `docs/` | `effective-canonical` | `adjacent` | `keep` | 架构冻结、验证索引、运行文档。 |
| `samples/` | `effective-canonical` | `adjacent` | `migrate` | golden cases、fixtures、稳定样本的目标根。 |
| `tests/` | `effective-canonical` | `adjacent` | `keep` | integration/e2e/performance/contract-lint 的正式承载面。 |
| `scripts/` | `effective-canonical` | `adjacent` | `migrate` | 根级自动化、迁移脚本与构建辅助的正式目标根。 |
| `config/` | `effective-canonical` | `adjacent` | `keep` | 机台与环境配置源。 |
| `data/` | `effective-canonical` | `adjacent` | `keep` | 配方、schema、运行资产。 |
| `deploy/` | `effective-canonical` | `adjacent` | `migrate` | 部署与交付材料的目标根。 |
| `packages/` | `migration-source` | `main-chain` | `migrate` | 历史业务、契约、宿主承载面；必须被排空或降级。 |
| `integration/` | `migration-source` | `adjacent` | `migrate` | 历史集成、回归、证据根；目标收敛到 `tests/`。 |
| `tools/` | `migration-source` | `adjacent` | `migrate` | 历史脚本、迁移、构建辅助根；目标收敛到 `scripts/`。 |
| `examples/` | `migration-source` | `adjacent` | `migrate` | 历史样本根；目标收敛到 `samples/`。 |
| `specs/` | `feature-artifact` | `adjacent` | `freeze` | 规格、计划、任务材料；只保留为 freeze/input artifacts。 |
| `cmake/` | `support` | `non-main-chain` | `keep` | 构建基础设施与布局清单。 |
| `third_party/` | `vendor` | `non-main-chain` | `keep` | 第三方依赖；不得承载业务事实。 |
| `build/` | `generated` | `non-main-chain` | `keep` | 构建产物输出。 |
| `logs/` | `generated` | `non-main-chain` | `keep` | 运行与验证日志。 |
| `uploads/` | `generated` | `adjacent` | `keep` | staging 输入面，不是 source of truth。 |
| `docs/_archive/` | `archived` | `non-main-chain` | `archive` | 历史归档，不参与默认审阅链。 |
| 已退出历史根 | `removed` | `non-main-chain` | `remove` | 只保留迁移结论，不作为正式入口。 |

## 3. bridge、脚本入口与门禁约束

- `effective-canonical` 必须先变成真实 owner 面、正式承载面或正式入口面，legacy root 才允许降级为 wrapper、redirect、forwarding include/CMake、README+tombstone 等显式 bridge。
- `migration-source` 只表达“当前仍有内容，但必须迁出到目标根”；它不构成终态 canonical 许可，也不允许继续承载新业务逻辑。
- 根级稳定入口固定为 `build.ps1`、`test.ps1`、`ci.ps1` 与 `scripts/validation/run-local-validation-gate.ps1`；正式脚本实现目标根为 `scripts/`，`tools/` 仅保留兼容入口或 wrapper 角色。
- `Wave 1` 对根治理的正式门禁以 `validation-gates.md` 为准，至少要求 canonical root 文档、workspace layout 解析、root script 入口和 freeze/layout validator 能形成同一套证据链。
- 任何想把 `migration-source`、`support`、`vendor`、`generated`、`archived` 重新解释为默认 owner 根的行为，都视为违规；必须先更新冻结文档与门禁定义并重新通过根级验证。

## 4. 与冻结文档集的关系

- `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s06-repo-structure-guide.md` 是路径分类、处置值和 bridge 退出口径的冻结事实源。
- `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s09-test-matrix-acceptance-baseline.md` 定义根级验证入口和证据路径。
- `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s10-frozen-directory-index.md` 定义审阅顺序与回链索引。
- `specs/refactor/arch/NOISSUE-workspace-template-refactor/wave-mapping.md` 负责 root 到 wave 的正式映射与退出波次。
- `specs/refactor/arch/NOISSUE-workspace-template-refactor/validation-gates.md` 负责 `Wave 0-6` 的入口门禁、退出门禁、验证命令和证据路径。
- `docs/architecture/system-acceptance-report.md` 是根级 closeout 与 acceptance 证据的最终汇总面。
