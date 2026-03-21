# Release Summary

- branch: `feat/hmi/NOISSUE-dispense-trajectory-preview-v2`
- base: `main`
- head: `2d2f745c`

## Scope
- HMI 在线状态初始化与预览刷新门禁修复（避免 `_connected` 未初始化导致在线 smoke 失败）。
- 发布证据链补齐：premerge + qa 报告。

## Commit Range (origin/main..HEAD)
- `2d2f745c` fix(hmi): initialize online state before preview refresh gate
- `04538035` chore(repo): ignore local build and temp workdirs
- `03442a09` docs(process): add release summary for dxf preview contract branch
- `7276ad75` fix(runtime): align dxf preview contract semantics and workflow evidence
- `6081d15e` feat(hmi): add dispense trajectory preview workflow and interlock plumbing

## Risks
1. 变更集中在 HMI UI 初始化与状态同步，风险面主要是在线模式状态门禁判定。
2. 本次未改动协议命令面，仅修复初始化顺序和状态回填；回归风险可控。

## Verification
1. `./test.ps1 -Profile CI -Suite apps -FailOnKnownFailure` -> `passed=16 failed=0`。
2. 在线 smoke 三联测（`online-smoke` / `verify-online-ready-timeout` / `verify-online-recovery-loop`）均通过。
3. QA 报告：`D:\Projects\SiligenSuite\docs\process-model\reviews\feat-hmi-NOISSUE-dispense-trajectory-preview-v2-qa-20260322-014949.md`
4. Premerge 报告：`D:\Projects\SiligenSuite\docs\process-model\reviews\feat-hmi-NOISSUE-dispense-trajectory-preview-v2-premerge-20260321-232650.md`

## Rollback Plan
1. 若线上出现在线模式回归，优先回退提交 `fix(hmi): initialize online state before preview refresh gate`。
2. 回退后重跑 `apps` 套件门禁与三联 smoke，确认恢复基线。

## Release Recommendation
- `Go`
