---
name: ss-release-pr
description: |
  发布与 PR 提交技能。执行发布前门禁检查，确认测试与审查证据完整后，完成
  push 和 PR 创建，并输出发布摘要。
---

# /ss-release-pr

## 步骤 0：上下文归一化（必须先做）

```powershell
$CTX = .\tools\scripts\resolve-workflow-context.ps1 | ConvertFrom-Json
```

若 `CTX.BranchCompliant = false`，立即中止。

## 步骤 1：确定 base 分支

```powershell
$BASE = (gh pr view --json baseRefName -q .baseRefName 2>$null)
if ([string]::IsNullOrWhiteSpace($BASE)) {
  $BASE = (gh repo view --json defaultBranchRef -q .defaultBranchRef.name 2>$null)
}
if ([string]::IsNullOrWhiteSpace($BASE)) { $BASE = "main" }
```

## 步骤 2：前置门禁校验

1. 分支命名符合规范：`$CTX.BranchCompliant = true`
2. 存在预审报告：`docs/process-model/reviews/$($CTX.BranchSafe)-premerge-*.md`
3. 存在 QA 报告：`docs/process-model/reviews/$($CTX.BranchSafe)-qa-*.md`
4. 当前测试通过且证据可追溯

```powershell
$PREMERGE = Get-ChildItem "docs/process-model/reviews/$($CTX.BranchSafe)-premerge-*.md" -ErrorAction SilentlyContinue |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 1
$QA = Get-ChildItem "docs/process-model/reviews/$($CTX.BranchSafe)-qa-*.md" -ErrorAction SilentlyContinue |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 1
if (-not $CTX.BranchCompliant -or -not $PREMERGE -or -not $QA) {
  throw "发布门禁不通过：分支合规=$($CTX.BranchCompliant), premerge=$([bool]$PREMERGE), qa=$([bool]$QA)"
}
```

任一项不满足，立即中止并输出缺失项清单。

## 执行步骤

1. 同步并合并 base：
```powershell
git fetch origin $BASE
git merge origin/$BASE --no-edit
```
2. 若出现冲突，停止并输出冲突文件列表
3. 执行回归测试（至少）：
```powershell
.\test.ps1 -Profile CI -Suite apps -FailOnKnownFailure
```
4. 测试失败则中止，不允许继续 push
5. 生成发布摘要（范围、风险、验证、回滚）
6. 推送：
```powershell
git push -u origin $($CTX.Branch)
```
7. 创建 PR（`gh` 可用时）：
```powershell
gh pr create --base $BASE --fill
```
8. 若 `gh` 不可用，输出“手工创建 PR 所需信息”，并标记 `DONE_WITH_CONCERNS`

## 输出

1. PR 链接
2. 发布摘要（保存到 `docs/process-model/reviews/{branchSafe}-release-{timestamp}.md`）

```powershell
$OUT = "docs/process-model/reviews/$($CTX.BranchSafe)-release-$($CTX.Timestamp).md"
```

## 规则

1. 禁止在证据不完整时“直接发布”
2. 未解决阻断项必须显式终止流程
3. 使用常规 push，禁止强推覆盖历史
4. 测试失败或合并冲突时，必须输出 `BLOCKED`
