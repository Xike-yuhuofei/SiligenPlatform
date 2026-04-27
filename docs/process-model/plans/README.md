# Plans 目录说明

本目录用于沉淀每个任务的规划产物，推荐文件：

1. `{ticket}-scope-{timestamp}.md`
2. `{ticket}-arch-plan-{timestamp}.md`
3. `{ticket}-test-plan-{timestamp}.md`

补充说明：

1. 当任务的首要目标是“冻结残留边界、拆分 carry-forward 批次、确定后续承接入口”时，仍归入 `scope` 文档。
2. 这类 `scope` 文档必须显式写出：残留来源、authority / owner、批次分类、推荐分支/worktree、最小验证与停止条件。

约定：

1. `ticket` 通过 `scripts/validation/resolve-workflow-context.ps1` 提取；失败时固定 `NOISSUE`
2. `timestamp` 格式：`yyyyMMdd-HHmmss`
