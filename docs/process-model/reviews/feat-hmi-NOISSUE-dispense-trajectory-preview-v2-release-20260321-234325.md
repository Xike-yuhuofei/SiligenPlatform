# Release Summary

- Branch: $(@{Branch=feat/hmi/NOISSUE-dispense-trajectory-preview-v2; BranchSafe=feat-hmi-NOISSUE-dispense-trajectory-preview-v2; Ticket=NOISSUE; BranchCompliant=True; Type=feat; Scope=hmi; ShortDesc=dispense-trajectory-preview-v2; Date=2026-03-21; Timestamp=20260321-234325}.Branch)
- Ticket: $(@{Branch=feat/hmi/NOISSUE-dispense-trajectory-preview-v2; BranchSafe=feat-hmi-NOISSUE-dispense-trajectory-preview-v2; Ticket=NOISSUE; BranchCompliant=True; Type=feat; Scope=hmi; ShortDesc=dispense-trajectory-preview-v2; Date=2026-03-21; Timestamp=20260321-234325}.Ticket)
- Timestamp: $(@{Branch=feat/hmi/NOISSUE-dispense-trajectory-preview-v2; BranchSafe=feat-hmi-NOISSUE-dispense-trajectory-preview-v2; Ticket=NOISSUE; BranchCompliant=True; Type=feat; Scope=hmi; ShortDesc=dispense-trajectory-preview-v2; Date=2026-03-21; Timestamp=20260321-234325}.Timestamp)
- Base: main
- Status: READY_TO_RELEASE

## Scope

1. 对齐 DXF 预览链路契约，补齐 dxf.preview.confirm 请求/响应夹具与协议兼容断言。
2. 统一 HMI / mock / CLI / runtime / gateway 的预览阶段状态与互锁语义，减少调用方与被调用方语义漂移。
3. 补充契约治理相关单测与回归证据（含中断续跑记录），确保仿真链路与真实链路契约一致。
4. 引入流程化技能与工作流脚本，规范后续范围锁定、审查、回归与发布门禁执行。

## Evidence

- Premerge: $(D:\Projects\SiligenSuite\docs\process-model\reviews\feat-hmi-NOISSUE-dispense-trajectory-preview-v2-premerge-20260321-232907.md.Name)
- QA: $(D:\Projects\SiligenSuite\docs\process-model\reviews\feat-hmi-NOISSUE-dispense-trajectory-preview-v2-qa-20260321-231515.md.Name)
- Validation Report: integration/reports/workspace-validation.md
- Validation JSON: integration/reports/workspace-validation.json
- Recent Commits:
  - 2e8ed81c fix(runtime): align dxf preview contract semantics and workflow evidence
  - 7a52478e feat(hmi): add dispense trajectory preview workflow and interlock plumbing

## Verification

1. powershell -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite apps -FailOnKnownFailure
   - Result: passed=15 failed=0 known_failure=0 skipped=0
2. 已存在补充回归证据：docs/validation/dxf-contract-regression-resume-2026-03-21.md

## Risk

1. 变更横跨 HMI、协议、runtime、gateway 与设备适配层，存在跨层回归风险。
2. 现场设备配置差异仍可能触发未覆盖的互锁组合路径。
3. 预览确认后的状态流转依赖配置与时序，需在目标产线做多批次实机验收。

## Rollback

1. 若发现阻断问题，回滚到基线提交 8115c1a5（aseline-2026-03-21）。
2. 保留本分支与验证报告，针对根因做最小修复后重新执行 pps + protocol-compatibility 门禁。
3. 禁止强推覆盖历史，采用追加修复提交方式回滚风险。
