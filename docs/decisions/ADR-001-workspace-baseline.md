# ADR-001 Workspace Baseline

- 状态：`Accepted`
- 日期：`2026-03-24`

## 决策

本仓库采用模板化目标拓扑作为正式基线：

1. target canonical roots：`apps/`、`modules/`、`shared/`、`docs/`、`samples/`、`tests/`、`scripts/`、`config/`、`data/`、`deploy/`
2. migration-source roots：`packages/`、`integration/`、`tools/`、`examples/`
3. `.specify/` 与 `specs/` 不纳入当前工作区正式基线
4. `cmake/`、`third_party/`、`build/`、`logs/`、`uploads/` 分别归类为 `support`、`vendor`、`generated`

## 当前基线认定

- `modules/` 是 `M0-M11` 的唯一正式 owner 根。
- `packages/`、`integration/`、`tools/`、`examples/` 允许继续承载迁移中的当前事实，但不再被视为终态 canonical roots。
- `docs/architecture/dsp-e2e-spec/` 是唯一正式冻结文档集入口。
- 根级验证入口固定为 `build.ps1`、`test.ps1`、`ci.ps1` 与 `scripts/validation/run-local-validation-gate.ps1`。
