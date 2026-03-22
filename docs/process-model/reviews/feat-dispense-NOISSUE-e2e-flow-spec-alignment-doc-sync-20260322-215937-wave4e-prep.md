# Doc Sync Review

- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 时间戳：20260322-215937
- 范围：Wave 4E external intake preparation
- 上游收口：`docs/process-model/plans/NOISSUE-wave4d-closeout-20260322-212031.md`

## 1. 本次同步的文件

1. `docs/runtime/external-migration-observation.md`
2. `integration/reports/verify/wave4d-closeout/intake/README.md`
3. `integration/reports/verify/wave4d-closeout/intake/field-scripts-template.md`
4. `integration/reports/verify/wave4d-closeout/intake/release-package-template.md`
5. `integration/reports/verify/wave4d-closeout/intake/rollback-package-template.md`
6. `integration/reports/verify/wave4e-rerun/README.md`
7. `integration/reports/verify/wave4e-rerun/intake/README.md`
8. `integration/reports/verify/wave4e-rerun/observation/README.md`
9. `docs/process-model/plans/NOISSUE-wave4e-scope-20260322-215937.md`
10. `docs/process-model/plans/NOISSUE-wave4e-arch-plan-20260322-215937.md`
11. `docs/process-model/plans/NOISSUE-wave4e-test-plan-20260322-215937.md`

## 2. 关键差异摘要

1. 观察期文档不再把新一轮证据目录硬编码为 `wave4c-closeout/observation/`。
2. `Wave 4D` intake 模板补充了实际采集命令、`pending-input` 状态语义和路径约束。
3. 新增 `wave4e-rerun` 目录骨架，作为后续真实外部输入的落盘位置。
4. 新增 `Wave 4E` 准备阶段的 scope / arch / test 计划，但没有伪造 closeout 或观察结果。
