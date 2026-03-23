---
name: ss-ui-plan-check
description: |
  HMI/UI 方案审查技能。用于在实现前评估交互状态覆盖、信息层级、移动端适配
  与可访问性，降低“模板化 UI”与状态缺失风险。
---

# /ss-ui-plan-check

## 步骤 0：上下文归一化（必须先做）

```powershell
$CTX = .\tools\scripts\resolve-workflow-context.ps1 | ConvertFrom-Json
```

## 适用条件

仅用于涉及 `apps/hmi-app` 或用户可见交互变更的需求。

## 执行步骤

1. 读取 scope/arch plan，识别 UI 变更点
2. 对每个页面/流程检查 5 类状态：
   - 初始/加载
   - 正常
   - 空状态
   - 错误
   - 权限或网络异常
3. 检查响应式与键盘可达性
4. 给出必须修复项与可后置项

## 输出

写入：`docs/process-model/reviews/{ticket}-ui-plan-check-{timestamp}.md`

```powershell
$OUT = "docs/process-model/reviews/$($CTX.Ticket)-ui-plan-check-$($CTX.Timestamp).md"
```

## 规则

1. 审查结论必须关联具体页面或组件
2. 禁止只给“风格建议”而不落到状态覆盖
