# Canonical Paths

更新时间：`2026-03-24`

## 1. 分类定义

| classification | 含义 | 治理约束 |
|---|---|---|
| `effective-canonical` | 正式 owner 根、正式承载面或正式入口面 | 必须先成为真实承载面，legacy root 才允许降级。 |
| `migration-source` | 迁移期间暂存当前事实的历史根 | 只允许显式 bridge，不得继续新增业务 owner。 |
| `support` | 构建、装配或辅助基础设施 | 只能支撑正式根，不能升级为默认 owner 根。 |
| `vendor` | 第三方依赖引入面 | 不承载项目业务事实。 |
| `generated` | 构建、验证或运行生成物 | 不是 source of truth，不允许回写正式事实。 |
| `archived` | 历史归档材料 | 不参与默认审阅链，不向当前实现回流。 |
| `removed` | 已退出或已删除的历史根 | 只保留退出结论，不重新开放实现入口。 |
| `local-cache` | 允许存在于本地工作区、但不纳入正式基线的缓存根 | 必须被版本管理忽略，不得作为正式事实源或默认入口。 |

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
| `.specify/` | `local-cache` | `non-main-chain` | `ignore-local` | 本地工具缓存；允许存在，但不纳入正式基线、不得作为仓库正式资产。 |
| `specs/` | `local-cache` | `non-main-chain` | `ignore-local` | 本地工具缓存；允许存在，但不作为 `feature artifact` 正式根。 |
| `.claude/` | `local-cache` | `non-main-chain` | `ignore-local` | 本地工具状态目录；允许存在，但不纳入正式基线。 |
| `cmake/` | `support` | `non-main-chain` | `keep` | 构建基础设施与布局清单。 |
| `third_party/` | `vendor` | `non-main-chain` | `keep` | 第三方依赖；不得承载业务事实。 |
| `build/` | `generated` | `non-main-chain` | `keep` | 构建产物输出。 |
| `build-*/` | `generated` | `non-main-chain` | `ignore-local` | 根级临时专项构建目录；允许本地存在，但不作为稳定输出根。 |
| `logs/` | `generated` | `non-main-chain` | `keep` | 运行与验证日志。 |
| `uploads/` | `generated` | `adjacent` | `keep` | staging 输入面，不是 source of truth。 |
| `docs/_archive/` | `archived` | `non-main-chain` | `archive` | 历史归档，不参与默认审阅链。 |
| 已退出历史根 | `removed` | `non-main-chain` | `remove` | 只保留迁移结论，不作为正式入口。 |

### 2.1 已退出 legacy roots

下列根目录不再属于当前工作区结构分类，只允许按“已退出历史根”处理：

| 路径 | 退出后角色 | 当前规则 |
|---|---|---|
| `packages/` | 已退出的历史业务 / 契约 / 宿主根 | 不再作为 current root classification；只允许在历史、迁移或兼容说明中被显式提及。 |
| `integration/` | 已退出的历史验证与证据根 | 当前正式证据统一落到 `tests/` 与 `tests/reports/`。 |
| `tools/` | 已退出的历史脚本根 | 当前正式脚本实现根为 `scripts/`。 |
| `examples/` | 已退出的历史样本根 | 当前稳定样本根为 `samples/`。 |

## 3. bridge、脚本入口与门禁约束

- `effective-canonical` 必须先变成真实 owner 面、正式承载面或正式入口面，legacy root 才允许降级为 wrapper、redirect、forwarding include/CMake、README+tombstone 等显式 bridge。
- `migration-source` 只用于描述仍在历史资料或兼容说明中的迁移来源；它不构成 current root classification，也不允许继续承载新业务逻辑。
- `local-cache` 只表达“允许本地存在但不具备架构地位”；它不是 source of truth，不允许成为默认入口、正式脚本输入或冻结文档锚点。
- 根级稳定入口固定为 `build.ps1`、`test.ps1`、`ci.ps1` 与 `scripts/validation/run-local-validation-gate.ps1`；正式脚本实现目标根为 `scripts/`。
- `Wave 1` 对根治理的正式门禁以 `validation-gates.md` 为准，至少要求 canonical root 文档、workspace layout 解析、root script 入口和 freeze/layout validator 能形成同一套证据链。
- 任何想把 `migration-source`、`support`、`vendor`、`generated`、`archived` 重新解释为默认 owner 根的行为，都视为违规；必须先更新冻结文档与门禁定义并重新通过根级验证。

## 4. 与冻结文档集的关系

- `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s06-repo-structure-guide.md` 是路径分类、处置值和 bridge 退出口径的冻结事实源。
- `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s09-test-matrix-acceptance-baseline.md` 定义根级验证入口和证据路径。
- `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s10-frozen-directory-index.md` 定义审阅顺序与回链索引。
- `docs/architecture/system-acceptance-report.md` 是根级 acceptance 汇总面，`docs/architecture/legacy-cutover-status.md` 是当前 legacy/cutover 状态的正式收束页。
