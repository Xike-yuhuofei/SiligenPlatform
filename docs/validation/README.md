# Validation Docs

本目录承接当前仍有效的正式验证口径、长期验收文档和测试模板。

对 `Codex` 的默认读取顺序：

1. 先看根级正式口径与矩阵
2. 再看长期验收文档和模板
3. 只有在需要追溯阶段性执行记录、incident closeout 或首层评审材料时，才进入 `history/`

## 1. 当前正式口径

验证体系的正式真值优先看：

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

- `layered-test-matrix.md`
- `online-test-matrix-v1.md`
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

## 3. 历史文档区

历史材料统一放在 [history/](./history/README.md)，当前已分为：

- `history/first-layer-review/`：首层评审、S1-S9 记录、复审包
- `history/incidents/`：单次 incident 或 bug 的验证 closeout
- `history/dxf/`：阶段性 DXF 回归/会话总结
- `history/phase-closeouts/`：分阶段 rollout、gap、closeout 材料
- `history/summaries/`：阶段性 release/summary 文档

这些内容保留追溯价值，但不应作为当前默认正式口径。
