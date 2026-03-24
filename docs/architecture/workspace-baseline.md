# Workspace Baseline

更新时间：`2026-03-24`

## 1. 当前正式基线

当前工作区的唯一正式冻结基线入口为：

- `docs/architecture/workspace-e2e-freeze/`

该目录承载 `S01-S09` 正式轴文档和 `S10` 索引，不再并行维护第二套默认架构冻结入口。

## 2. 当前整仓形态

| 类别 | 当前结论 |
|---|---|
| target canonical roots | `apps/`, `modules/`, `shared/`, `docs/`, `samples/`, `tests/`, `scripts/`, `config/`, `data/`, `deploy/` |
| migration-source roots | `packages/`, `integration/`, `tools/`, `examples/` |
| feature artifact roots | `specs/` (`freeze/input artifacts` only) |
| support/vendor/generated | `cmake/`, `third_party/`, `build/`, `logs/`, `uploads/` |
| 正式冻结文档集 | `docs/architecture/workspace-e2e-freeze/` |

## 2.1 canonical roots 与 migration-source roots 正式口径

- `target canonical roots` 是后续架构评审、owner 判定、目录归位和根级构建图收敛的唯一正式目标根集合。
- `migration-source roots` 仅用于承载迁移过程中的当前事实，不具备默认 owner 资格，也不构成终态承载面。
- 新增或迁移后的稳定资产必须优先写入 `target canonical roots`；禁止以“临时落位”为由回流到 `migration-source roots`。
- 若确需临时桥接保留在 `migration-source roots`，必须在对应 `Wave` 文档中声明退出条件、责任人和清理时点。

## 3. 当前阶段说明

- 本工作区已完成治理基线切换与根级 cutover：模板化目标根已经成为正式目标结构，同时也是默认构建与验证入口。
- `packages/`、`integration/`、`tools/`、`examples/` 当前仅允许作为迁移来源、wrapper、tombstone 或历史审计面存在，不再是终态 owner 根。
- `modules/`、`samples/`、`scripts/`、`deploy/` 已从“预留目标根”转为正式承载根；新增稳定资产必须直接归位到这些 canonical roots。

## 4. 统一验证事实

- 根级 build/test/CI 入口是 `build.ps1`、`test.ps1`、`ci.ps1`。
- 本地冻结门禁入口是 `tools/scripts/run-local-validation-gate.ps1`。
- 冻结文档集校验入口是 `tools/migration/validate_workspace_freeze_docset.py`。
- workspace 布局校验入口是 `tools/migration/validate_workspace_layout.py`。

## 5. 当前结论

本页用于声明正式冻结基线，不在 `Wave 1` 阶段宣告整项模板化工作区重构最终 closeout。正式审阅、owner 判定和 acceptance 回链统一以 `workspace-e2e-freeze`、根级 gate 证据以及 `system-acceptance-report.md` 为准；后续任何目录调整都必须继续保持向本页声明的 target canonical roots 收敛。

## 6. 本次特性 closeout 判定

- 历史约束：在 `Wave 1` 基线阶段，当前仅 `US1 / Wave 0` 已完成 closeout；后续波次必须逐步通过 `Phase End Check` 回写，不得在本页预宣告“整项模板化工作区重构最终 closeout”。
- 当前已完成的 closeout 波次，以 `docs/architecture/system-acceptance-report.md` 中已回写并通过 `Phase End Check` 的条目为准；截至 `2026-03-24`，该报告已确认 `US1 / Wave 0`、`US2 / Wave 1`、`US3 / Wave 2`、`US4 / Wave 3`、`US5 / Wave 4`、`US6 / Wave 5`、`US7 / Wave 6`。
- `build.ps1`、`test.ps1`、`ci.ps1`、`tools/scripts/run-local-validation-gate.ps1`、`scripts/migration/legacy-exit-checks.py` 的有效证据，统一由 `system-acceptance-report.md` 与对应 `integration/reports/**` 路径承载。
- accepted exceptions：`none`
- `packages/`、`integration/`、`tools/`、`examples/` 继续保留当前迁移来源事实，但不构成默认 owner；其是否影响某一波次 closeout，以对应 Phase End Check 结论为准。
