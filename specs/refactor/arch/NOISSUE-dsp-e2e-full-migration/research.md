# Phase 0 Research: DSP E2E 全量迁移与 Legacy 清除

## Decision 1: 以单轨 canonical roots 作为唯一终态 owner 面

- Decision: 实施与验收仍以 `docs/architecture/workspace-baseline.md` 中定义的 `target canonical roots` 作为正式目标根集合，即 `apps/`、`modules/`、`shared/`、`docs/`、`samples/`、`tests/`、`scripts/`、`config/`、`data/`、`deploy/`；但对旧根当前是否仍存在、是否已经退出 closeout，则以仓库现实和 `docs/architecture/system-acceptance-report.md` 为准。`workspace-baseline.md` 中残留的 `migration-source roots` 表述只被解释为历史迁移语境，不得被解释为本特性允许保留 live owner 面。
- Rationale: 当前上游文档对旧根口径并不完全一致：`workspace-baseline.md` 仍保留历史迁移语汇，而 `system-acceptance-report.md` 与仓库现实都表明 `packages/`、`integration/`、`tools/`、`examples/` 已物理删除。将“目标根定义”和“当前存在性结论”拆开解释，才能避免把历史描述误读为终态许可。
- Alternatives considered:
  - 把 `workspace-baseline.md` 中的 `migration-source roots` 继续解释为本特性允许保留的 live owner 面: 拒绝。与规格、宪章和当前仓库现实冲突。
  - 完全忽略 `workspace-baseline.md`: 拒绝。它仍然是 `target canonical roots` 定义的正式来源。

## Decision 2: 模块内部 bridge/source 子树属于 legacy 资产，必须进入清除范围

- Decision: `module.yaml` 中仍标记为 `bridge_mode: active` 的模块桥接面，统一视为本特性的 legacy 资产，包括但不限于 `modules/*/src`、`modules/workflow/process-runtime-core`、`modules/dxf-geometry/engineering-data`、`modules/runtime-execution/runtime-host`、`modules/runtime-execution/device-adapters`、`modules/runtime-execution/device-contracts`、`modules/workflow/application/usecases-bridge`。
- Rationale: 这些目录虽然位于 canonical roots 内部，但仍以 bridge/source/forwarder 语义存在，并被 `validate_workspace_layout.py` 与模块 `CMakeLists.txt` 明确建模为桥接或迁移来源。按照规格要求，它们不能继续作为终态组成部分。
- Alternatives considered:
  - 继续保留 shell-only bridges 作为长期终态: 拒绝。规格明确要求物理清除 legacy 承载层。
  - 只把旧顶层目录视为 legacy，忽略 canonical roots 内部桥接子树: 拒绝。会把“目录已迁移、真实承载未收敛”的问题永久化。

## Decision 3: 用迁移归位与清除矩阵作为唯一实施账本

- Decision: 本特性的实施账本采用“迁移归位与清除矩阵”模型，逐项覆盖 `M0-M11`、模块内 canonical surface，以及分布在 `apps/`、`shared/`、`docs/`、`samples/`、`tests/`、`scripts/`、`config/`、`data/`、`deploy/` 中与 module owner 直接相关的跨根资产。每条记录必须同时保存目标 owner 面、当前 live surface、现存 bridge/legacy 路径、构建入口、验证入口、阻断项和清除状态。矩阵的契约在 `contracts/migration-alignment-clearance-contract.md` 中定义，后续正式执行账本落在 canonical docs 中。
- Rationale: 规格要求的是整仓级现实收敛，而不是只把 `modules/` 子树收干净。若不把 `apps/` 应用入口、`shared/` 共享契约/支持层和其他 canonical roots 中的直接 owner 资产纳入同一账本，就无法证明不存在“模块迁了，但对外 owner 面仍旧靠旧语义组织”的残留状态。
- Alternatives considered:
  - 仅依赖模块 `README.md` 和 `module.yaml`: 拒绝。它们能表达单模块状态，但无法形成整仓级 closeout 视图。
  - 仅依赖 `system-acceptance-report.md`: 拒绝。它适合承载最终结论，不适合承载逐资产迁移执行账本。

## Decision 4: 现有 workspace/legacy gate 需要先补齐责任分工，再执行 bridge 终态清除

- Decision: `scripts/migration/validate_workspace_layout.py`、`scripts/migration/legacy-exit-checks.py`、`scripts/migration/validate_dsp_e2e_spec_docset.py`、根级 `build.ps1`/`test.ps1`/`ci.ps1` 与 `scripts/validation/run-local-validation-gate.ps1` 继续组成正式执行链，但必须先明确各自责任边界，再执行 bridge 终态清除。`validate_workspace_layout.py` 负责 active bridge、live owner graph 和 bridge metadata 违规；`legacy-exit-checks.py` 只负责旧顶层根和旧路径文本回流；`validate_dsp_e2e_spec_docset.py` 负责唯一冻结文档集完整性。门禁责任未收紧前，不得把 bridge 子树直接删空并期待现有 gate 自行得出正确结论。
- Rationale: 当前 `validate_workspace_layout.py` 仍显式列出 `MODULE_BRIDGES`，而 `legacy-exit-checks.py` 只检查旧根存在性和旧路径文本引用，不检查 `bridge_mode: active`、`SILIGEN_BRIDGE_ROOTS` 或 `SILIGEN_MIGRATION_SOURCE_ROOTS`。如果不先补齐门禁语义，就会把“删桥后被旧 gate 误判失败”与“bridge 真正退出失败”混为一谈。
- Alternatives considered:
  - 继续把 `legacy-exit-checks.py` 当成 bridge/fallback 全面退出的唯一证据: 拒绝。其当前能力不覆盖该语义。
  - 先删除 bridge，再等回归失败后修 gate: 拒绝。会让执行顺序变得不可控，并增加回滚成本。

## Decision 5: 兼容行为只能作为有期限的迁移中间态，不得再出现在正式运行路径

- Decision: 本特性允许在实施过程中短暂保留 bridge/fallback 行为，但这些行为只能用于迁移窗口，必须由矩阵记录退出条件，并且不能继续作为根级 build/test/CI/local gate 的默认运行路径。最终 closeout 前，所有运行说明、排障说明、发布说明和模块说明都必须只描述 canonical 入口。
- Rationale: 宪章要求兼容行为显式且有时间边界。规格进一步要求最终状态达到零 fallback、零 compatibility shell。
- Alternatives considered:
  - 允许 bridge 脚本或 bridge CMake 永久保留但不再默认调用: 拒绝。只要仍在 live repo 中保留 owner 语义，就会成为未来回流点。
  - 用文档警告替代物理清除: 拒绝。与本特性的完成定义冲突。

## Decision 6: 验证策略采用“职责拆分 + 快速闭环 + 全量 closeout”双层执行

- Decision: 日常实施阶段使用最小必要验证组合：`build.ps1 -Profile Local -Suite contracts`、`test.ps1 -Profile CI -Suite contracts`、`python scripts/migration/validate_workspace_layout.py`、`python scripts/migration/validate_dsp_e2e_spec_docset.py`、`python scripts/migration/legacy-exit-checks.py --profile local`、`scripts/validation/run-local-validation-gate.ps1`。Closeout 阶段追加 `test.ps1 -Profile CI -Suite e2e,protocol-compatibility,performance -FailOnKnownFailure` 与 `ci.ps1 -Suite all`。所有报告按职责解释：layout 证明 active bridge/live owner graph 退出，docset validator 证明唯一冻结事实源完整，legacy-exit 证明旧根和旧路径引用退出，local gate/CI 证明根级聚合入口通过。
- Rationale: 迁移过程中会频繁触碰模块清单、CMake、文档冻结口径和门禁脚本。需要先有快速验证闭环保证目录、证据和事实源同步收敛，再用全量根级入口证明没有功能回归和 fallback 回流。把不同报告各自证明什么写清楚，才能避免把单一报告误用为“所有退出维度都已覆盖”的假证据。
- Alternatives considered:
  - 每次只跑 `ci.ps1 -Suite all`: 拒绝。反馈过慢，不利于逐模块切桥。
  - 只跑 contracts/lint，不跑 docset/layout/legacy-exit/local gate: 拒绝。无法证明执行链、冻结事实源和退出维度保持单轨。
