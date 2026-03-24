# Release Summary

## 1. 发布上下文

1. 分支：`refactor/arch/NOISSUE-architecture-refactor-spec`
2. base：`origin/main`
3. 工作流上下文：`ticket=NOISSUE`，`timestamp=20260324-213217`
4. 目标版本：`0.1.0-rc.1`
5. 证据：
   - scope：[`NOISSUE-rc1-release-scope-20260324-213217.md`](/D:/Projects/SS-dispense-align/docs/process-model/plans/NOISSUE-rc1-release-scope-20260324-213217.md)
   - premerge：[`refactor-arch-NOISSUE-architecture-refactor-spec-premerge-20260324-213217.md`](/D:/Projects/SS-dispense-align/docs/process-model/reviews/refactor-arch-NOISSUE-architecture-refactor-spec-premerge-20260324-213217.md)
   - qa：[`refactor-arch-NOISSUE-architecture-refactor-spec-qa-20260324-213217.md`](/D:/Projects/SS-dispense-align/docs/process-model/reviews/refactor-arch-NOISSUE-architecture-refactor-spec-qa-20260324-213217.md)
   - changelog：[`CHANGELOG.md`](/D:/Projects/SS-dispense-align/CHANGELOG.md)

## 2. 门禁结果

1. premerge：`Conditional Go`
   - 含义：允许生成仓内 `RC evidence`，不外推为 merge / tag / 正式发布结论
2. local validation gate：`Go`
   - `passed_steps=5/5`
   - evidence：`integration/reports/local-validation-gate/20260324-214144/local-validation-gate-summary.md`
3. QA build/test：`Go`
   - `build.ps1 -Profile CI -Suite all`：`exit 0`
   - `test.ps1 -Profile CI -Suite all -FailOnKnownFailure`：`passed=47 failed=0 known_failure=0 skipped=0`
   - evidence：`integration/reports/qa-0.1.0-rc.1-20260324-213217/build-ci-all.log`，`integration/reports/qa-0.1.0-rc.1-20260324-213217/workspace-validation/workspace-validation.md`
4. CI 串行链：`Go`
   - workspace validation：`passed=47 failed=0 known_failure=0 skipped=0`
   - local validation gate：`passed_steps=5/5`
   - legacy exit：`passed_rules=33 failed_rules=0`
   - evidence：`integration/reports/ci-0.1.0-rc.1-20260324-213217/`
5. release-check：`Go`
   - `.\release-check.ps1 -Version 0.1.0-rc.1`
   - result：`release-check passed`
   - evidence：`integration/reports/releases/0.1.0-rc.1/`

## 3. 发布执行结果

1. `release-manifest.txt` 已生成：
   - `version: 0.1.0-rc.1`
   - `git-head: 9a04f8e1e5b179550582852552db8add180678aa`
   - `allow-cli-blocked: False`
   - `allow-runtime-blocked: False`
2. app dry-run 全部未命中 `BLOCKED`：
   - `hmi-app`：`target = apps/hmi-app/scripts/run.ps1`，`mode = online`，`gateway autostart = disabled`
   - `control-tcp-server`：canonical `siligen_tcp_server.exe`
   - `control-cli`：canonical `siligen_cli.exe`
   - `control-runtime`：canonical `siligen_control_runtime.exe`
3. 本轮未执行以下动作：
   - 创建 tag
   - push 分支
   - 创建或更新 PR

## 4. 风险与回滚

1. 当前 release evidence 绑定的是脏工作树快照；`release-manifest.txt` 只记录 `HEAD`，不能完整表达未提交工作树差异。
2. 仓外交付包、回滚包、现场脚本观察和长期硬件验证，未纳入本轮 `RC` release gate。
3. 最小回滚口径仍按 `CHANGELOG.md` 的四件套执行：提交 hash、release evidence、`config/` 与 `data/recipes/` 快照、canonical control-apps 构建产物快照。

## 5. 结论

1. 当前状态：`DONE_WITH_CONCERNS`
2. `0.1.0-rc.1` 的仓内 `RC evidence` 已收齐，可作为后续白名单收口、merge 审查和是否打 tag 的输入基线。
