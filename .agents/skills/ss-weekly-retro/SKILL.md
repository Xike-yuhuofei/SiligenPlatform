---
name: ss-weekly-retro
description: |
  周度复盘技能。统计本周期提交、缺陷、测试与文档更新质量，输出改进项与下一周期行动。
---

# /ss-weekly-retro

## 步骤 0：上下文归一化（必须先做）

```powershell
$CTX = .\tools\scripts\resolve-workflow-context.ps1 | ConvertFrom-Json
```

## 时间窗口

默认最近 7 天，可指定 14/30 天。

## 执行步骤

1. 汇总提交与变更规模
2. 汇总审查与 QA 问题分布
3. 分析重复故障与高风险模块
4. 形成可执行改进行动

## 输出

写入：`docs/process-model/retro/{yyyy-mm-dd}-{timestamp}.md`

```powershell
$OUT = "docs/process-model/retro/$($CTX.Date)-$($CTX.Timestamp).md"
```

建议结构：

1. 本周产出
2. 问题画像（按模块和严重级别）
3. 流程瓶颈
4. 下周行动项（负责人+截止时间）

## 规则

1. 不写空泛总结，必须有量化证据
2. 每条行动项必须可跟踪
