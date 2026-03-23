---
name: ss-scope-clarify
description: |
  需求澄清与范围锁定技能。用于把含糊请求转换为可执行范围、验收标准和风险清单，
  并输出 scope 文档供后续架构和开发阶段复用。
---

# /ss-scope-clarify

## 步骤 0：上下文归一化（必须先做）

```powershell
$CTX = .\tools\scripts\resolve-workflow-context.ps1 | ConvertFrom-Json
```

若 `CTX.BranchCompliant = false`，先中止并提示分支改名。

## 输入

- 用户目标或问题描述
- 当前分支名（通过 `CTX.Branch` 获取）

## 执行步骤

1. 检查分支命名是否符合：`<type>/<scope>/<ticket>-<short-desc>`
2. 识别业务目标、受影响模块、非目标范围
3. 提出最多 5 个关键澄清问题（只问必要问题）
4. 输出“范围锁定文档”

## 输出模板

写入：`docs/process-model/plans/{ticket}-scope-{timestamp}.md`

```powershell
$OUT = "docs/process-model/plans/$($CTX.Ticket)-scope-$($CTX.Timestamp).md"
```

内容至少包含：

1. 背景与目标
2. In Scope / Out of Scope
3. 受影响模块清单（apps/packages/integration/tools）
4. 验收标准（可测试、可观察）
5. 风险与前置依赖
6. 明确不做项

## 规则

1. 不做代码实现
2. 不跳过 Out of Scope
3. 验收标准必须可执行，禁止空泛表述
4. 输出文件必须使用 `CTX.Ticket` 与 `CTX.Timestamp`，禁止手写占位符
