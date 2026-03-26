# QA Report

## 1. 环境信息

1. 分支：`feat/dispense/NOISSUE-e2e-flow-spec-alignment`
2. 基线提交：`f6e5668d`
3. 执行时间：`2026-03-22`
4. 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-205132`
5. 说明：本轮结论覆盖 `Wave 4A` 的 DXF legacy shell exit；未清理工作树中既有无关脏变更

## 2. 执行命令与退出码

1. `python packages/engineering-data/tests/test_engineering_data_compatibility.py`
   - exit code：`0`
2. `python packages/engineering-data/tests/test_dxf_pipeline_legacy_exit.py`
   - exit code：`0`
3. `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile CI -Suite packages`
   - exit code：`0`
4. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite packages -ReportDir integration\reports\verify\wave4a-closeout\packages -FailOnKnownFailure`
   - exit code：`0`
5. `python integration/scenarios/run_engineering_regression.py`
   - exit code：`0`
6. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite protocol-compatibility -ReportDir integration\reports\verify\wave4a-closeout\protocol -FailOnKnownFailure`
   - exit code：`0`
7. `powershell -NoProfile -ExecutionPolicy Bypass -File .\legacy-exit-check.ps1 -Profile Local -ReportDir integration\reports\verify\wave4a-closeout\legacy-exit`
   - exit code：`0`

## 3. 测试范围

1. `engineering-data` canonical artifact 语义回归
2. `dxf-pipeline` legacy import/proto/CLI alias 退出负向验证
3. `packages` 根级 suite
4. 工程回归 `integration/scenarios/run_engineering_regression.py`
5. `protocol-compatibility` 根级 suite
6. `legacy-exit` 归零报告

## 4. 失败项与根因

1. 首次 `test_dxf_pipeline_legacy_exit.py` 失败
   - 根因：本地残留 `__pycache__` 与可能存在的 site-packages 安装让旧 shim 在当前解释器下仍可被导入
2. 首次 `legacy-exit-check.ps1` 失败
   - 根因 A：`packages/engineering-data/src/dxf_pipeline/` 只剩空目录/缓存目录，缺失规则按“目录存在即失败”过于严格
   - 根因 B：`tools/migration/validate_wave2b_residual_payoff.py` 作为历史审计脚本，保留 legacy needle，但未登记白名单

## 5. 修复项与验证证据

1. `packages/engineering-data/tests/test_dxf_pipeline_legacy_exit.py`
   - 改为 `python -S` 子进程隔离验证，只看临时复制的 `src` 树，并排除 `__pycache__`
2. `tools/scripts/legacy_exit_checks.py`
   - 为 `dxf-pipeline` compat shell 增加缺失规则
   - 缺失规则对“只有缓存、没有源码文件”的空目录视为通过
   - 为 `tools/migration/validate_wave2b_residual_payoff.py` 补历史审计白名单
3. 复测结果：
   - `test_dxf_pipeline_legacy_exit.py`：`Ran 2 tests ... OK`
   - `packages` suite：`passed=10 failed=0 known_failure=0 skipped=0`
   - `protocol-compatibility`：`passed=1 failed=0 known_failure=0 skipped=0`
   - `legacy-exit-checks.md`：`passed_rules=19 failed_rules=0`

## 6. 未修复项与风险接受结论

1. 当前未验证仓外 sibling repo、现场脚本或已部署环境是否仍缓存 legacy 入口；这是仓内无法自动化证明的人工确认项。
2. 工作树仍明显不干净，但本轮变更没有回退无关文件。
3. 风险接受结论：接受 `Wave 4A` 收口完成；不接受据此直接宣称仓外所有环境都已迁移完成。

## 7. 发布建议

1. `Wave 4A closeout`：`Go`
2. `仓外环境迁移完成宣告`：`No-Go`
