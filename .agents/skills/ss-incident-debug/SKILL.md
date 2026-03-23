---
name: ss-incident-debug
description: |
  故障调查技能。先复现、再定位、后修复，强调根因证据链，避免盲改。
---

# /ss-incident-debug

## 步骤 0：上下文归一化（必须先做）

```powershell
$CTX = .\tools\scripts\resolve-workflow-context.ps1 | ConvertFrom-Json
```

## 原则

1. 未复现前，不给修复结论
2. 未定位根因前，不提交修复
3. 修复后必须验证并记录回归结果

## 执行步骤

1. 复现：明确输入、环境、日志与触发条件
2. 定位：缩小范围到模块/函数/配置
3. 根因：解释“为什么发生”而非只描述“发生了什么”
4. 修复与验证：给出前后证据

## 输出

写入：`docs/process-model/reviews/{ticket}-incident-{timestamp}.md`

```powershell
$OUT = "docs/process-model/reviews/$($CTX.Ticket)-incident-$($CTX.Timestamp).md"
```

至少包含：

1. 事件描述
2. 复现步骤
3. 根因分析
4. 修复说明
5. 回归验证
6. 防复发措施
