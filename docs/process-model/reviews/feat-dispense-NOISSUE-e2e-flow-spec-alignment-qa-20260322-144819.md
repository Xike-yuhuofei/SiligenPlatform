# QA Report

## 1. 环境信息

1. 分支：`feat/dispense/NOISSUE-e2e-flow-spec-alignment`
2. 基线提交：`f6e5668d`
3. 执行时间：`2026-03-22`
4. 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-144819`
5. 说明：本次按当前工作树作为基线执行验证，未清理既有无关脏变更；结论仅覆盖 `Wave 2A` 收口与 `Wave 2B` 取证相关行为

## 2. 执行命令与退出码

1. `apps/control-runtime/run.ps1 -DryRun`
   - exit code：`0`
2. `apps/control-tcp-server/run.ps1 -DryRun`
   - exit code：`0`
3. `apps/control-cli/run.ps1 -DryRun`
   - exit code：`0`
4. `.\legacy-exit-check.ps1 -Profile CI -ReportDir integration\reports\verify\wave2a-closeout\legacy-exit`
   - exit code：`0`
5. `.\build.ps1 -Profile CI -Suite apps`
   - exit code：`0`
6. `.\test.ps1 -Profile CI -Suite apps -ReportDir integration\reports\verify\wave2a-closeout\apps -FailOnKnownFailure`
   - exit code：`0`
7. `.\test.ps1 -Profile CI -Suite packages -ReportDir integration\reports\verify\wave2a-closeout\packages -FailOnKnownFailure`
   - exit code：`0`
8. `.\test.ps1 -Profile CI -Suite integration -ReportDir integration\reports\verify\wave2a-closeout\integration -FailOnKnownFailure`
   - exit code：`0`
9. `.\test.ps1 -Profile CI -Suite protocol-compatibility -ReportDir integration\reports\verify\wave2a-closeout\protocol -FailOnKnownFailure`
   - exit code：`0`
10. `.\build.ps1 -Profile CI -Suite simulation`
    - exit code：`0`
11. `.\test.ps1 -Profile CI -Suite simulation -ReportDir integration\reports\verify\wave2a-closeout\simulation -FailOnKnownFailure`
    - exit code：`0`

## 3. 测试范围

1. app canonical dry-run
2. legacy-exit 回流门禁
3. apps / packages / integration / protocol-compatibility / simulation 根级 suites
4. simulation suite 内的 scheme C ctest 子集与 process-runtime-core contract tests

## 4. 失败项与根因

1. 无

## 5. 修复项与验证证据

1. 本轮未为测试失败做额外修复
2. 验证证据位于 `integration/reports/verify/wave2a-closeout/*`
3. 关键结果：
   - `legacy-exit`：`18/18 PASS`
   - `apps`：`16/16 PASS`
   - `packages`：`6/6 PASS`
   - `integration`：`1/1 PASS`
   - `protocol-compatibility`：`1/1 PASS`
   - `simulation`：`5/5 PASS`

## 6. 未修复项与风险接受结论

1. `Wave 2B` 的 root build graph、runtime 默认资产路径和 residual dependency 仍未切换
2. 这些不是本轮回归缺陷，而是已经确认的下阶段阻塞项
3. 风险接受结论：接受 `Wave 2A` 收口完成，但不接受 `Wave 2B` 物理切换启动

## 7. 发布建议

1. `Wave 2A` 收口结论：`Go`
2. `Wave 2B` 物理切换结论：`No-Go`
