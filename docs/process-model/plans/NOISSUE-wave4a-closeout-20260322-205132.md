# NOISSUE Wave 4A Closeout

- 状态：Done
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-205132`
- 上游收口：`docs/process-model/plans/NOISSUE-wave3c-closeout-20260322-202853.md`
- 关联计划：
  - `docs/process-model/plans/NOISSUE-wave4a-scope-20260322-204444.md`
  - `docs/process-model/plans/NOISSUE-wave4a-arch-plan-20260322-204444.md`
  - `docs/process-model/plans/NOISSUE-wave4a-test-plan-20260322-204444.md`
- 关联验证目录：`integration/reports/verify/wave4a-closeout/`

## 1. 总体裁决

1. `Wave 4A = Done`
2. `Wave 4A closeout = Done`
3. `next-wave planning = Go`
4. `next-wave implementation = No-Go`

## 2. 本阶段实际完成内容

### 2.1 外部 DXF launcher 合同切到 canonical

1. `packages/process-runtime-core/src/application/services/dxf/DxfPbPreparationService.cpp`
   - 外部 DXF 仓库模式向 `dxf_core_launcher.py` 传递的子命令从 `dxf-to-pb` 切到 `engineering-data-dxf-to-pb`
2. `packages/process-runtime-core/tests/unit/dispensing/UploadFileUseCaseTest.cpp`
   - 单测断言同步到 canonical 子命令
3. `packages/process-runtime-core/src/application/usecases/dispensing/README.md`
   - current-fact 说明同步到 canonical launcher 合同

### 2.2 `engineering-data` 兼容壳退出

1. `packages/engineering-data/pyproject.toml`
   - 删除 legacy CLI alias：
     - `dxf-to-pb`
     - `path-to-trajectory`
     - `export-simulation-input`
     - `generate-dxf-preview`
2. 删除：
   - `packages/engineering-data/src/dxf_pipeline/**`
   - `packages/engineering-data/src/proto/dxf_primitives_pb2.py`
   - `packages/engineering-data/tests/test_dxf_pipeline_legacy_shims.py`
3. 新增：
   - `packages/engineering-data/tests/test_dxf_pipeline_legacy_exit.py`
   - 负向验证 legacy import/proto 必须失败，且 `pyproject.toml` 只保留 canonical CLI

### 2.3 门禁与 current-fact 文档升级

1. `tools/scripts/legacy_exit_checks.py`
   - 新增 `dxf-pipeline-compat-shells-absent` 缺失规则
   - 把 `dxf-pipeline` 规则从防回流升级到兼容壳归零
   - 为历史审计脚本 `tools/migration/validate_wave2b_residual_payoff.py` 补登记白名单
2. `docs/architecture/legacy-exit-checks.md`
3. `docs/architecture/dxf-pipeline-strangler-progress.md`
4. `docs/architecture/dxf-pipeline-final-cutover.md`
5. `docs/architecture/removed-legacy-items-final.md`
6. `docs/architecture/redundancy-candidate-audit.md`
7. `packages/engineering-data/README.md`
8. `packages/engineering-data/docs/compatibility.md`

统一结论：

1. `dxf-pipeline` 外部兼容窗口已结束
2. 工作区与 canonical 包都不再暴露 legacy CLI / import / proto 兼容壳
3. 后续只保留仓外环境人工确认与防回流门禁

## 3. 实际执行命令

### 3.1 Python / compatibility gate

1. `python packages/engineering-data/tests/test_engineering_data_compatibility.py`
2. `python packages/engineering-data/tests/test_dxf_pipeline_legacy_exit.py`
3. `python integration/scenarios/run_engineering_regression.py`

### 3.2 构建与根级回归

1. `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile CI -Suite packages`
2. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite packages -ReportDir integration\reports\verify\wave4a-closeout\packages -FailOnKnownFailure`
3. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite protocol-compatibility -ReportDir integration\reports\verify\wave4a-closeout\protocol -FailOnKnownFailure`
4. `powershell -NoProfile -ExecutionPolicy Bypass -File .\legacy-exit-check.ps1 -Profile Local -ReportDir integration\reports\verify\wave4a-closeout\legacy-exit`

## 4. 关键证据

1. `packages/engineering-data/tests/test_dxf_pipeline_legacy_exit.py`
   - `Ran 2 tests ... OK`
2. `integration/reports/verify/wave4a-closeout/packages/workspace-validation.md`
   - `passed=10 failed=0 known_failure=0 skipped=0`
3. `integration/reports/verify/wave4a-closeout/protocol/workspace-validation.md`
   - `passed=1 failed=0 known_failure=0 skipped=0`
   - `protocol compatibility suite passed`
4. `integration/reports/verify/wave4a-closeout/legacy-exit/legacy-exit-checks.md`
   - `passed_rules=19 failed_rules=0`
   - `dxf-pipeline-compat-shells-absent=PASS`
   - `dxf-pipeline-shell-blocked-in-code=PASS`
5. `python integration/scenarios/run_engineering_regression.py`
   - 输出 `engineering regression passed`

## 5. 阶段结论

1. `Wave 4A` 已完成 `engineering-data` 的外部兼容窗口退出。
2. `process-runtime-core` 外部 DXF launcher 已切到 canonical 命令名，没有因为 legacy alias 退出而断裂。
3. `legacy-exit` 已能同时约束目录删除、compat shell 缺失和 legacy 名称回流。

## 6. 非阻断记录

1. 当前工作树仍明显不干净，但 `Wave 4A` 相关写集、验证与 closeout 证据已闭环。
2. `packages/engineering-data/src/dxf_pipeline/` 目录在本地可能残留空目录或缓存目录；门禁已明确以“不得再有可执行源码文件”为准，不影响本次结论。
3. 下一阶段若要继续推进，只能处理仓外环境观察期、发布/回滚文档与现场迁移确认，不能再把 legacy 兼容壳恢复为过渡手段。
