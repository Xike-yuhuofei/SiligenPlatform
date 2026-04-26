# Validation Docs

本目录承接当前仍有效的正式验证口径、长期验收文档和测试模板。

对 `Codex` 的默认读取顺序：

1. 先看根级正式口径与矩阵
2. 再看长期验收文档和模板
3. 只有在需要追溯阶段性执行记录、incident closeout 或首层评审材料时，才进入 `history/`

## 1. 当前正式口径

验证体系的正式真值优先看：

- `docs/validation/gate-orchestrator.md`
- `scripts/validation/gates/gates.json`
- `docs/architecture/build-and-test.md`
- `docs/architecture/validation/layered-test-system-v2.md`
- `docs/architecture/validation/test-coverage-matrix-v1.md`
- `docs/architecture/validation/machine-preflight-quality-gate-v1.md`

全仓只允许以下正式层级：

- `L0` 静态与构建门禁
- `L1` 单元测试
- `L2` 模块契约测试
- `L3` 集成测试
- `L4` 模拟全链路 E2E
- `L5` HIL 闭环测试
- `L6` 性能与稳定性测试

## 2. 当前正式文档

以下内容默认属于正式区：

- `gate-orchestrator.md`
- `layered-test-matrix.md`
- `online-test-matrix-v1.md`
- `pr-gate-and-hil-gate-policy.md`
- `self-hosted-build-runner-runbook.md`
- `release-test-checklist.md`
- `dispense-trajectory-preview-real-acceptance-checklist.md`
- `dxf-real-dispense-multi-cycle-template-v1.md`
- `sim-observer-p0-acceptance-v1.md`
- `sim-observer-p1-acceptance-v1.md`
- `sim-observer-unified-acceptance-v1.md`
- `sim-observer-real-recording-validation-v1.md`
- `simulation-controlled-production-test-v1.md`
- `codex-test-templates/`

判定原则：

1. 长期有效的 checklist、matrix、template、acceptance 文档保留在正式区
2. 单次执行记录、incident closeout、阶段性 review/summary/closeout 一律下沉到 `history/`
3. 文件名带日期不自动等于历史，是否承担当前真值职责才是最高判定标准

## 2.1 门禁与自动化测试权威入口

门禁与自动化测试的当前权威入口是：

- 执行入口：`scripts/validation/invoke-gate.ps1`
- Gate 定义：`scripts/validation/gates/gates.json`
- 变更分类：`scripts/validation/classify-change.ps1`
- 分类规则：`scripts/validation/gates/change-classification.json`
- 开发者遵守文档：`docs/validation/gate-orchestrator.md`
- PR / Native / HIL 合并策略：`docs/validation/pr-gate-and-hil-gate-policy.md`

后续新增或调整门禁时，必须同步更新 gate 配置、契约测试和 `gate-orchestrator.md`。不得在 GitHub workflow、本地 hook 或临时脚本中维护独立测试清单。

`pre-push` 当前是风险路由型 quick gate：`invoke-pre-push-gate.ps1` 先确认待 push commit range 和 dirty worktree 策略，再调用 `classify-change.ps1` 输出 `selected_pre_push_steps`，最后由 `invoke-gate.ps1 -Gate pre-push` 只运行被选中的 quick checks。Native/HIL 敏感变更在 pre-push 中只生成 follow-up evidence，不直接跑 full native 或 HIL。

PR / Native / HIL / Nightly / Release 分层以 `docs/validation/gate-orchestrator.md` 和 `scripts/validation/gates/gates.json` 为准。`Strict PR Gate` 是所有 PR 的中等强度 baseline；`Strict Native Gate` 与 `Strict HIL Gate` 对所有 PR 产生稳定 check，敏感 PR 才真实执行重门禁；Nightly 承接 full-offline、performance、长稳/仿真、baseline drift 和 coverage；Release Gate 承接发布 Go/No-Go。PR passed 不等于 release ready。

## 3. 历史文档区

历史材料统一放在 [history/](./history/README.md)，当前已分为：

- `history/first-layer-review/`：首层评审、S1-S9 记录、复审包
- `history/incidents/`：单次 incident 或 bug 的验证 closeout
- `history/dxf/`：阶段性 DXF 回归/会话总结
- `history/phase-closeouts/`：分阶段 rollout、gap、closeout 材料
- `history/summaries/`：阶段性 release/summary 文档

这些内容保留追溯价值，但不应作为当前默认正式口径。
