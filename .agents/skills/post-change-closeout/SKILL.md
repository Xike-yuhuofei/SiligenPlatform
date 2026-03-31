---
name: post-change-closeout
description: "Generate evidence-based closeout for finished changes, including validation status, residual risks, doc sync, and next steps. Use for closeout, 变更收尾, 闭环报告, and 最终交接."
---

# Purpose

本 skill 的唯一职责是：在一轮代码、文档、测试或验证工作结束后，形成工程闭环，而不是继续实现。

“完成”不等于“改完了”，而是要把改了什么、为什么改、如何验证、还有什么风险、哪些文档要同步、下一步该做什么统一沉淀出来。

# Use this skill when

- 本轮实现、修复、审查或验证已经结束。
- 已经有可陈述的改动结果与验证结果。
- 需要准备提交、评审、交接或任务关闭。
- 需要把本轮工作沉淀成团队可复用证据。

# Do not use this skill when

- 当前仍在持续实现中。
- 当前根因还没收敛。
- 当前验证尚未完成，连最基本结果都没有。
- 当前只是想继续展开新需求。

# Inputs

- 本轮任务目标
- 改动清单
- 相关文件 / 模块
- 验证结果
- 未解决问题
- 文档影响
- 后续待办

# Required outputs

1. 变更摘要
2. 验证证据
3. 风险与未覆盖项
4. 文档同步项
5. 下一步建议

# Hard prohibitions

- 不继续实现新内容。
- 不把未验证项写成已验证。
- 不掩盖剩余风险。
- 不省略文档影响。
- 不把新需求混入 closeout。

# Closeout rules

## 1. 以证据而不是感觉收尾

优先引用测试、日志、样本对照、代码落点和回归结果。

## 2. 风险必须显式写出

不能因为主任务完成就省略未覆盖样本、未上机确认、未验证邻近路径、临时兼容逻辑和证据不足项。

## 3. 文档不同步也要说明

如果当前不更新文档，必须明确为什么不更新、风险是什么以及何时应补。

## 4. 结论必须可供下一位工程师接手

输出必须让后续的人快速知道发生了什么、当前状态如何、下一步最合理的动作是什么。

# Output format

严格使用以下结构输出：

## Closeout 报告

### 1. 本轮目标
-

### 2. 实际完成内容
-
-

### 3. 代码 / 文档落点
-
-

### 4. 验证证据
- 验证项：
- 结果：
- 证据充分性：

### 5. 风险与未覆盖项
-
-

### 6. 文档同步建议
- 需要更新：
- 可暂缓但必须记录：

### 7. 当前结论
- 本轮是否达到目标：
- 是否适合进入提交 / 评审 / 合并：

### 8. 下一步建议
1.
2.
3.

# Handoff

- 上游：
  - `task-execute`
  - `targeted-fix-and-regression`
  - `refactor-plan-and-migrate`
  - `online-validation-gate-and-evidence`
- 下游：
  - Git 提交 / PR / 评审
  - 后续联机验证
  - 后续文档同步
  - 后续架构治理或补充回归
