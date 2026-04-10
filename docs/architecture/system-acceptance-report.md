# System Acceptance Report

更新时间：`2026-04-07`

## 1. 结论

- 当前工作区主根仍维持 `apps/`、`modules/`、`shared/`、`docs/`、`samples/`、`tests/`、`scripts/`、`config/`、`data/`、`deploy/` 单轨布局。
- 旧根 `packages/`、`integration/`、`tools/`、`examples/` 已物理删除，并由 legacy-exit 门禁防回流。
- `specs/` 与 `.specify/` 不纳入当前工作区正式基线；它们只允许作为本地忽略缓存存在，不再作为活动 feature / support roots。
- 根级执行链统一为 `build.ps1`、`test.ps1`、`ci.ps1`、`scripts/validation/run-local-validation-gate.ps1`。
- `2026-04-07` 已从 live code 与 `modules/runtime-execution/contracts/runtime` 的 canonical required surface 中移除 `IConfigurationPort` / `ITaskSchedulerPort` / `IEventPublisherPort` alias shell；剩余 gap 已转为 broader owner 收口与文档同步问题。
- 截至 `2026-04-07`，本次 legacy/bridge 收口不得宣称已完成；`runtime-execution` / `runtime-service` 一侧仍存在进行中的 owner 收口与文档同步工作，因此 closeout 口径必须保持 `NOT PASS`。

## 2. 当前正式证据根

- `tests/reports/workspace-validation.md`
- `tests/reports/legacy-exit-current/legacy-exit-checks.md`
- `tests/reports/local-validation-gate/<timestamp>/`
- `tests/reports/dsp-e2e-spec-docset/`
- `tests/reports/verify/`

历史批次与按日期展开的 acceptance 复核细节见：

- `docs/architecture/history/closeouts/system-acceptance-report-2026-04-07.md`

## 3. 对外口径

- 任何“当前来源/迁移来源”表述不得再把已删除旧根写成现势 owner。
- 文档、脚本、排障、部署说明统一指向单轨根与根级入口。
- 历史迁移过程材料仅允许保留在归档目录，不再作为执行依据。
- 历史过程文档若保留 `specs/` / `.specify/` 文案，必须被理解为历史快照，不得覆盖当前 `canonical-paths`、`workspace-baseline` 与 `system-acceptance-report` 的正式口径。

## 4. 历史材料归档

- 历史波次与迁移过程：`docs/_archive/`
- 冻结文档与验收索引：`docs/architecture/dsp-e2e-spec/`
- 历史验证报告：`tests/reports/verify/`（仅审计用途）

## 5. 后续基线

- 新增或改动必须满足：`known_failure=0`、`legacy-exit blocking_count=0`，且任何残留 finding 都必须是已登记的 `controlled-exception`。
- e2e/performance 回归报告在单轨目录下产出并固化到 `tests/reports/` 与 `docs/architecture/`。
- mock / 无机台回归通过后，仍需单独补真实机台或高保真仿真放行证据，覆盖运动精度、停止距离、真实 IO 与安全链。
- 若重新引入旧根、bridge metadata 或 bridge 路径文案，`validate_workspace_layout.py` 与 `legacy-exit-checks.py` 必须直接失败。
