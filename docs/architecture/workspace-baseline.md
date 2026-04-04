# Workspace Baseline

更新时间：`2026-03-25`

## 1. 当前正式基线

当前工作区的唯一正式冻结基线入口为：

- `docs/architecture/dsp-e2e-spec/`

该目录承载 `S01-S09` 正式轴文档和 `S10` 索引，不再并行维护第二套默认架构冻结入口。

## 2. 当前整仓形态

| 类别 | 当前结论 |
|---|---|
| canonical roots | `apps/`, `modules/`, `shared/`, `docs/`, `samples/`, `tests/`, `scripts/`, `config/`, `data/`, `deploy/` |
| removed legacy roots | `packages/`, `integration/`, `tools/`, `examples/` 已物理删除 |
| removed speckit roots | `.specify/`, `specs/` 已卸载，不再参与当前工作区基线 |
| support/vendor/generated | `cmake/`, `third_party/`, `build/`, `logs/`, `uploads/` |
| 正式冻结文档集 | `docs/architecture/dsp-e2e-spec/` |

## 2.1 单轨根级口径

- `canonical roots` 是后续架构评审、owner 判定、目录归位和根级构建图收敛的唯一正式根集合。
- 已删除 legacy roots 不得以 wrapper、旁路目录或文档默认入口的形式回流；一旦回流，`legacy-exit-checks.py` 必须失败。
- 新增稳定资产必须直接落在 canonical roots；禁止以“临时落位”为由恢复 bridge root 或默认 fallback。

## 3. 当前阶段说明

- 本工作区已完成治理基线切换、根级 cutover 与 bridge exit closeout。
- `modules/`、`shared/`、`tests/`、`scripts/`、`samples/`、`deploy/` 均已转为正式承载根。
- `apps/` 只承担宿主与装配职责，`shared/` 只承担跨模块稳定能力，不再通过 legacy 根承载 live owner 事实。

## 4. 统一验证事实

- 根级 build/test/CI 入口是 `build.ps1`、`test.ps1`、`ci.ps1`。
- 本地冻结门禁入口是 `scripts/validation/run-local-validation-gate.ps1`。
- 冻结文档集校验入口是 `scripts/migration/validate_dsp_e2e_spec_docset.py`。
- workspace 布局校验入口是 `scripts/migration/validate_workspace_layout.py`。
- legacy/bridge 退出校验入口是 `scripts/migration/legacy-exit-checks.py`。

## 5. 当前结论

- `2026-03-25` 实测结果表明，工作区已经完成单轨收口。
- `validate_workspace_layout.py`：`missing=0`、`legacy_root_present=0`、`bridge_root_failure=0`。
- `legacy-exit-checks.py`：`finding_count=0`。
- `test.ps1 -Profile CI -Suite contracts -FailOnKnownFailure`：`passed=15`、`failed=0`、`known_failure=0`、`skipped=0`。
- `run-local-validation-gate.ps1`：`overall_status=passed`、`passed_steps=6/6`。

## 6. Closeout Evidence

- `tests/reports/workspace-validation.md`
- `tests/reports/legacy-exit-current/legacy-exit-checks.md`
- `tests/reports/dsp-e2e-spec-docset/dsp-e2e-spec-docset.md`
- `tests/reports/local-validation-gate/20260325-212745/local-validation-gate-summary.md`
- `docs/architecture/system-acceptance-report.md`
