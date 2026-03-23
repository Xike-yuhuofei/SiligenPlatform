# Doc Sync Report

## Context
- Branch: $(@{Branch=feat/hmi/NOISSUE-dispense-trajectory-preview-v2; BranchSafe=feat-hmi-NOISSUE-dispense-trajectory-preview-v2; Ticket=NOISSUE; BranchCompliant=True; Type=feat; Scope=hmi; ShortDesc=dispense-trajectory-preview-v2; Date=2026-03-23; Timestamp=20260323-084313}.Branch)
- Ticket: $(@{Branch=feat/hmi/NOISSUE-dispense-trajectory-preview-v2; BranchSafe=feat-hmi-NOISSUE-dispense-trajectory-preview-v2; Ticket=NOISSUE; BranchCompliant=True; Type=feat; Scope=hmi; ShortDesc=dispense-trajectory-preview-v2; Date=2026-03-23; Timestamp=20260323-084313}.Ticket)
- Timestamp: $(@{Branch=feat/hmi/NOISSUE-dispense-trajectory-preview-v2; BranchSafe=feat-hmi-NOISSUE-dispense-trajectory-preview-v2; Ticket=NOISSUE; BranchCompliant=True; Type=feat; Scope=hmi; ShortDesc=dispense-trajectory-preview-v2; Date=2026-03-23; Timestamp=20260323-084313}.Timestamp)

## Updated Documents
1. docs/troubleshooting/windows-terminal-color-scheme-follow-system.md
2. docs/troubleshooting/post-refactor-runbook.md
3. docs/README.md

## Key Diffs
1. 新增标准 FAQ：Windows Terminal 配色方案跟随系统的正确配置、反例、根因与验证步骤。
2. 在 runbook 中新增 FAQ 入口，避免知识分散到临时对话。
3. 在 docs 根索引中标注 troubleshooting 目录承载该 FAQ，提升可发现性。

## Consistency Check
1. 路径均为当前仓库 canonical 文档路径。
2. FAQ 结论与已完成修复一致：	heme=system + profiles.defaults.colorScheme.{dark,light}。
