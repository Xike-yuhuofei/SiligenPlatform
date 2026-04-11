# Architecture

本目录是 `SiligenSuite` 工作区级架构文档的正式入口。

对 `Codex` 的默认读取顺序：

1. 工作区基线与目录/依赖规则
2. 系统主链路、契约与模块入口
3. 迁移/冻结/收口规则
4. 只有在需要追溯专项审计、阶段性评审或收口过程时，才进入 `history/`

## 1. 当前正式主题

### 工作区规则

- `workspace-baseline.md`
- `canonical-paths.md`
- `directory-responsibilities.md`
- `dependency-rules.md`
- `config-layout.md`
- `data-asset-layout.md`

### 系统主链路与契约

- `dsp-e2e-spec/`
- `dxf-authority-layout-global-strategy-v1.md`
- `dxf-closed-contour-direction-semantics-v1.md`
- `dxf-motion-execution-contract-v1.md`
- `dxf-motion-execution-code-map-v1.md`
- `runtime-supervision-state-contract-boundary-v1.md`
- `modules-owner-boundary.md`
- `legacy-cutover-status.md`

### 运行基线与 Acceptance

- `system-acceptance-report.md`
- `performance-baseline.md`

### 测试体系与运行门禁

- `build-and-test.md`
- `layered-test-system-v2.md`
- `test-coverage-matrix-v1.md`
- `machine-preflight-quality-gate-v1.md`

### 迁移与收口规则

- `migration-principles.md`
- `legacy-freeze-policy.md`
- `legacy-exit-checks.md`
- `refactor-guard-rules.md`
- `refactor-guard-runbook.md`

### 模块入口

- `modules/README.md`

## 2. 历史文档区

历史材料统一放在 [history/](./history/README.md)，当前已分为：

- `history/module-boundary/`：模块边界评审、置信度审计、阶段性 owner 审查
- `history/audits/`：专项审计与盘点报告
- `history/baselines/`：历史基线与旧阶段指标快照
- `history/progress/`：strangler/progress 过程记录
- `history/closeouts/`：阶段性 closeout 记录

这些内容保留追溯价值，但不应作为当前正式架构规则的第一入口。

## 3. 使用规则

1. 当 `Codex` 需要理解当前系统结构、边界和依赖时，优先读本目录正式区
2. review、audit、progress、closeout 只用于解释“如何收敛到当前状态”，不直接承担当前真值
3. 若历史材料中的结论仍然有效，应在正式区形成稳定文档，而不是继续依赖历史稿
