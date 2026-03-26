# Validation

## 1. 根级入口

- `build.ps1`
- `test.ps1`
- `ci.ps1`
- `scripts/validation/run-local-validation-gate.ps1`

## 2. 正式承载面

- 冻结规范：`docs/architecture/dsp-e2e-spec/`
- 正式发版前测试清单：`docs/validation/release-test-checklist.md`
- trace owner：`modules/trace-diagnostics/`
- HMI owner：`modules/hmi-application/`
- HMI 宿主壳：`apps/hmi-app/`
- 正式验证根：`tests/`
- 稳定样本根：`samples/`
- 系统收敛汇总：`docs/architecture/system-acceptance-report.md`

## 3. 必要回链字段

- `stage_id`
- `artifact_id`
- `module_id`
- `workflow_state`
- `execution_state`
- `event_name`
- `failure_code`
- `evidence_path`

## 4. 默认报告位置

- `tests/reports/workspace-validation.*`
- `tests/reports/ci/`
- `tests/reports/local-validation-gate/<timestamp>/`
- `tests/reports/.../dsp-e2e-spec-docset/`

## 5. Wave 5 引用检查

- 波次：`Wave 5`
- 布局门禁：`python .\scripts\migration\validate_workspace_layout.py --wave "Wave 5"`
- 仓库级验证入口：`python -m test_kit.workspace_validation --suite packages --profile ci`
- 测试矩阵基线：`docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s09-test-matrix-acceptance-baseline.md`
- 冻结目录索引：`docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s10-frozen-directory-index.md`
