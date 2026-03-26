# tests/e2e

正式 e2e 验证承载面。

当前 canonical 子域：`tests/e2e/simulated-line/`、`tests/e2e/hardware-in-loop/`

## Wave 5 仓库级 e2e 回链检查点（US6 / M10-M11）

| Checkpoint | 通过标准 | 证据入口 |
|---|---|---|
| `US6-E2E-CP1-surface-anchored` | e2e 验证入口稳定承载于 `tests/e2e/`，并被 `tests/CMakeLists.txt` 的仓库级验证锚点校验覆盖 | `tests/e2e/README.md`、`tests/CMakeLists.txt` |
| `US6-E2E-CP2-acceptance-scenarios-aligned` | e2e 场景口径可回链到 S09 定义的 success/block/rollback/recovery/archive 五类 acceptance 场景 | `dsp-e2e-spec-s09-test-matrix-acceptance-baseline.md` |
| `US6-E2E-CP3-root-entry-consistent` | 执行与证据输出口径遵循根级入口与验证说明，不引入额外旧根入口 | `.\\test.ps1`、`.\\ci.ps1`、`scripts\\validation\\run-local-validation-gate.ps1`、`docs/validation/README.md` |
