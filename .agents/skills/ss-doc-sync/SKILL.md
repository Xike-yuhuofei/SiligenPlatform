---
name: ss-doc-sync
description: |
  文档同步技能。根据本次代码与发布变更，更新 README、架构、运行与排障文档，
  确保文档与实际行为一致。
---

# /ss-doc-sync

## 步骤 0：上下文归一化（必须先做）

```powershell
$CTX = .\tools\scripts\resolve-workflow-context.ps1 | ConvertFrom-Json
```

## 输入

- 本次变更 diff
- 发布摘要或 PR 描述

## 执行步骤

1. 识别受影响文档
2. 更新命令、路径、行为和限制
3. 校验文档与当前 canonical 路径一致

## 优先检查路径

1. `README.md`
2. `docs/architecture/`
3. `docs/runtime/`
4. `docs/troubleshooting/`

## 输出

1. 文档变更列表（建议写入 `docs/process-model/reviews/$($CTX.BranchSafe)-doc-sync-$($CTX.Timestamp).md`）
2. 关键差异摘要（便于 reviewer 快速核对）

## 规则

1. 文档更新必须与代码改动一致，禁止“想当然补写”
2. 若存在不确定项，明确标记待确认问题
