# QA Report

## 1. 环境信息

1. 分支：`refactor/arch/NOISSUE-architecture-refactor-spec`
2. 执行日期：`2026-03-24`
3. 工作流上下文：`ticket=NOISSUE`，`timestamp=20260324-213217`
4. 目标版本：`0.1.0-rc.1`
5. 审查前提：
   - scope：[`NOISSUE-rc1-release-scope-20260324-213217.md`](/D:/Projects/SS-dispense-align/docs/process-model/plans/NOISSUE-rc1-release-scope-20260324-213217.md)
   - premerge：[`refactor-arch-NOISSUE-architecture-refactor-spec-premerge-20260324-213217.md`](/D:/Projects/SS-dispense-align/docs/process-model/reviews/refactor-arch-NOISSUE-architecture-refactor-spec-premerge-20260324-213217.md)
6. 说明：本轮 QA 目标是形成仓内 `RC` 候选版证据，不包含 tag / push / PR。

## 2. 执行命令与退出码

1. `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\scripts\run-local-validation-gate.ps1`
   - 首次结果：`exit 1`
   - 阻塞原因：`workspace-baseline.md` 的 closeout 表述与 `validate_workspace_layout.py` 的 freeze 断言不一致
   - 同会话修复后复跑：`exit 0`
2. `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile CI -Suite all`
   - exit code：`0`
3. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite all -ReportDir integration\reports\qa-0.1.0-rc.1-20260324-213217\workspace-validation -FailOnKnownFailure`
   - exit code：`0`
4. `powershell -NoProfile -ExecutionPolicy Bypass -File .\ci.ps1 -Suite all -ReportDir D:\Projects\SS-dispense-align\integration\reports\ci-0.1.0-rc.1-20260324-213217`
   - 首次结果：`exit 1`
   - 阻塞原因：`ci.ps1` 传入绝对 `ReportDir` 时重复拼接仓根，产生非法路径
   - 同会话修复后复跑：`exit 0`

## 3. 结果摘要

1. 本地 validation gate：
   - `passed_steps=5/5`
   - evidence：`integration/reports/local-validation-gate/20260324-214144/local-validation-gate-summary.md`
2. 全量 CI profile 测试：
   - `passed=47 failed=0 known_failure=0 skipped=0`
   - evidence：`integration/reports/qa-0.1.0-rc.1-20260324-213217/workspace-validation/workspace-validation.md`
3. CI 串行链：
   - workspace validation：`passed=47 failed=0 known_failure=0 skipped=0`
   - local validation gate：`passed_steps=5/5`
   - legacy exit：`passed_rules=33 failed_rules=0`
   - evidence：
     - `integration/reports/ci-0.1.0-rc.1-20260324-213217/workspace-validation.md`
     - `integration/reports/ci-0.1.0-rc.1-20260324-213217/local-validation-gate/20260324-214955/local-validation-gate-summary.md`
     - `integration/reports/ci-0.1.0-rc.1-20260324-213217/legacy-exit/legacy-exit-checks.md`
4. 构建结果：
   - `build.ps1 -Profile CI -Suite all` 通过
   - 观察到第三方库配置 warning（Abseil future default / protobuf DLL export warning），未形成阻断
   - evidence：`integration/reports/qa-0.1.0-rc.1-20260324-213217/build-ci-all.log`

## 4. 同会话修复项

1. [`workspace-baseline.md`](/D:/Projects/SS-dispense-align/docs/architecture/workspace-baseline.md)
   - 调整为“冻结基线声明页”口径，保留 `Wave 1` 历史约束并把全量 closeout 状态回链到 `system-acceptance-report.md`
   - 目的：恢复 `validate_workspace_layout.py` 的 freeze 断言一致性
2. [`ci.ps1`](/D:/Projects/SS-dispense-align/ci.ps1)
   - 新增绝对/相对 `ReportDir` 统一解析
   - 目的：修复 CI 证据目录在绝对路径场景下的非法拼接

## 5. 残余风险

1. 当前 QA 证据覆盖的是 `2026-03-24` 的工作树快照，不是已经收口到单一提交的最终发布基线。
2. 仓外交付包、回滚包、现场脚本观察与长期硬件验证，未纳入本轮 QA 必过门禁。
3. 即使当前 QA 全绿，仍需在 release 报告中重复声明“禁止将本轮结论直接解释为 merge / tag 就绪”。

## 6. 结论

1. 当前仓内 QA 结论：`Go`
2. 当前已满足继续执行 `release-check.ps1 -Version 0.1.0-rc.1` 的条件。
