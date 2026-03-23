---
name: ss-arch-plan
description: |
  架构与测试计划技能。读取已锁定范围，输出可实现的技术方案、数据与控制流、
  失败模式和测试计划，作为开发与 QA 的统一输入。
---

# /ss-arch-plan

## 步骤 0：上下文归一化（必须先做）

```powershell
$CTX = .\tools\scripts\resolve-workflow-context.ps1 | ConvertFrom-Json
```

## 输入

- 最近一份 scope 文档（按 `docs/process-model/plans/{ticket}-scope-*.md` 逆序取最新）
- 当前分支代码状态

```powershell
$SCOPE_FILE = Get-ChildItem "docs/process-model/plans/$($CTX.Ticket)-scope-*.md" -ErrorAction SilentlyContinue |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 1
if (-not $SCOPE_FILE) { throw "缺少 scope 文档：$($CTX.Ticket)-scope-*.md" }
```

## 执行步骤

1. 复核范围文档，确认模块边界
2. 产出架构方案（组件、依赖、边界、回滚）
3. 产出测试计划（单元/集成/端到端/回归）
4. 标注性能、稳定性与安全风险

## 输出

1. `docs/process-model/plans/{ticket}-arch-plan-{timestamp}.md`
2. `docs/process-model/plans/{ticket}-test-plan-{timestamp}.md`

```powershell
$ARCH_OUT = "docs/process-model/plans/$($CTX.Ticket)-arch-plan-$($CTX.Timestamp).md"
$TEST_OUT = "docs/process-model/plans/$($CTX.Ticket)-test-plan-$($CTX.Timestamp).md"
```

## 最低内容要求

架构方案至少包含：

1. 受影响目录与文件类型
2. 数据流/调用流（可用 ASCII 图）
3. 失败模式与补救策略
4. 发布与回滚策略

测试计划至少包含：

1. 关键路径
2. 边界场景
3. 回归基线
4. 对应命令（优先根级命令）

## 规则

1. 不写代码，只写可执行计划
2. 测试计划必须对应到“具体路径/命令/预期结果”
3. scope 文档不存在时必须明确阻断，不允许跳过
