# QA Report

## 1. 环境信息

1. 分支：`feat/dispense/NOISSUE-e2e-flow-spec-alignment`
2. 基线提交：`f6e5668d`
3. 执行时间：`2026-03-22`
4. 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-202853`
5. 说明：本轮结论覆盖 `Wave 3C` 的 control-cli workspace 入口收口和 engineering-data bridge 默认入口规范化；未清理工作树中既有无关脏变更

## 2. 执行命令与退出码

1. `python packages/engineering-data/tests/test_engineering_data_compatibility.py`
   - exit code：`0`
2. `python packages/engineering-data/tests/test_dxf_pipeline_legacy_shims.py`
   - exit code：`0`
3. `powershell -NoProfile -ExecutionPolicy Bypass -File apps/control-cli/run.ps1 -DryRun`
   - exit code：`0`
4. `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile CI -Suite apps`
   - exit code：`0`
5. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite apps -ReportDir integration\reports\verify\wave3c-closeout\apps -FailOnKnownFailure`
   - exit code：`0`
6. `python integration/scenarios/run_engineering_regression.py`
   - exit code：`0`
7. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite protocol-compatibility -ReportDir integration\reports\verify\wave3c-closeout\protocol -FailOnKnownFailure`
   - exit code：`0`

## 3. 测试范围

1. `engineering-data` canonical/legacy 兼容 gate
2. `control-cli` canonical dry-run 与 `apps` 根级 suite
3. 工程回归 `integration/scenarios/run_engineering_regression.py`
4. `protocol-compatibility` 根级 suite

## 4. 失败项与根因

1. 本轮无失败项。

## 5. 验证证据

1. `integration/reports/verify/wave3c-closeout/apps/workspace-validation.md`
   - `passed=23 failed=0 known_failure=0 skipped=0`
2. 重点用例：
   - `control-cli-bootstrap-check-subdir` 通过
   - `control-cli-bootstrap-check-compat-alias-rejected` 通过
   - `control-cli-dxf-preview` 通过
   - `control-cli-dxf-preview-subdir` 通过
3. `integration/reports/verify/wave3c-closeout/protocol/workspace-validation.md`
   - `passed=1 failed=0 known_failure=0 skipped=0`
4. `python integration/scenarios/run_engineering_regression.py`
   - 输出 `engineering regression passed`
5. `python packages/engineering-data/tests/test_engineering_data_compatibility.py`
   - `Ran 4 tests ... OK`
6. `python packages/engineering-data/tests/test_dxf_pipeline_legacy_shims.py`
   - `Ran 3 tests ... OK`

## 6. 未修复项与风险接受结论

1. 当前未收紧 `dxf-pipeline` 仓外兼容窗口；这是本轮刻意保留的策略，不是遗留失败。
2. `.\build.ps1 -Profile CI -Suite apps` 输出中的 protobuf/abseil 链接 warning 仍存在，但未影响退出码与回归结果，当前记为非阻断观察项。
3. 风险接受结论：接受 `Wave 3C` 收口完成；不接受据此直接开始“仓外兼容窗口退出”实现。

## 7. 发布建议

1. `Wave 3C closeout`：`Go`
2. `仓外兼容窗口退出实现`：`No-Go`
