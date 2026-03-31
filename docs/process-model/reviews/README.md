# Reviews 目录说明

本目录用于沉淀架构审查、复核报告、测试结果与 closeout 补档。

## ARCH-203 baseline pack

`refactor/process/ARCH-203-process-model-owner-repair` 当前冻结了 10 份 `2026-03-31` baseline 文档：

1. `architecture-review-recheck-report-20260331-082102.md`
2. `coordinate-alignment-module-architecture-review-20260331-074844.md`
3. `dispense-packaging-module-architecture-review-20260331-074840.md`
4. `hmi-application-module-architecture-review-20260331-074433.md`
5. `motion-planning-module-architecture-review-20260331-075136-944.md`
6. `process-path-module-architecture-review-20260331-074844.md`
7. `process-planning-module-architecture-review-20260331-075201.md`
8. `refactor-workflow-ARCH-202-preview-compat-drain-workflow-module-architecture-review-20260331-074657.md`
9. `runtime-execution-module-architecture-review-20260331-075228.md`
10. `topology-feature-module-architecture-review-20260331-075200.md`

其中以下 4 份评审已补齐 closeout supplement，正文或附录必须保留 `hash`、`diff`、`command` 与精确索引：

1. `coordinate-alignment-module-architecture-review-20260331-074844.md`
2. `dispense-packaging-module-architecture-review-20260331-074840.md`
3. `process-planning-module-architecture-review-20260331-075201.md`
4. `topology-feature-module-architecture-review-20260331-075200.md`

## 其它推荐文件

1. `{branchSafe}-premerge-{timestamp}.md`
2. `{branchSafe}-qa-{timestamp}.md`
3. `{branchSafe}-release-{timestamp}.md`
4. `{ticket}-incident-{timestamp}.md`

## 约定

1. `branchSafe` 使用 `scripts/validation/resolve-workflow-context.ps1` 的输出
2. 不允许直接使用原始分支名写文件
3. 如评审文档被用作 implementation closeout 证据，必须能在当前 worktree 内独立复查，不允许依赖 sibling workspace 的未跟踪文件
