---
name: change-scope-guard
description: "Define the allowed edit scope, forbidden areas, cross-module limits, and stop conditions before code changes. Use for scope control, 变更边界, 范围门禁, and 越界判定."
---

# Purpose

本 skill 的唯一职责是：在任何代码改动开始前，先明确本轮允许改什么、不允许改什么，以及什么情况下算越界。

它解决的问题不是“怎么改”，而是“本轮最多能改到哪里”。

# Use this skill when

- 已经完成任务归类，并准备进入实现、修复或迁移。
- 已经有契约冻结、根因结论或方案落点。
- 需要避免任务从单点修复扩散成多模块重构。
- 需要防止 agent 顺手修改无关文件。

# Do not use this skill when

- 当前仍处于澄清、定位或规格阶段。
- 当前没有明确的任务目标、根因或方案。
- 当前只是进行只读审查，不涉及任何改动。

# Inputs

- 当前任务类型
- 本轮目标
- 契约冻结结果 / 根因结论 / 结构方案
- 当前相关目录与模块
- 用户已声明的约束
- 当前分支与工作树

# Required outputs

1. 允许修改的目录 / 文件类型
2. 禁止修改的目录 / 文件类型
3. 是否允许跨模块
4. 最大允许变更层级
5. 越界判定条件
6. 若超界时应如何处理

# Hard prohibitions

- 不放宽边界以“顺手修更多问题”。
- 不将 unrelated cleanup 混入主任务。
- 不将边界声明写成模糊措辞。
- 不把“目前看起来方便修改”当成合理边界。

# Scope rules

## 1. 单点修复优先单点边界

故障修复默认边界应尽量收敛为故障直接责任层、必要的邻近适配层和必要的测试补充。

## 2. 新功能优先 owner 层边界

新功能默认允许 owner 模块、与之直接相关的接口 / 契约，以及必要测试与文档。默认不允许为了实现方便把功能落到 view 层、bridge 层或 compat 层。

## 3. 架构治理优先阶段边界

结构迁移的边界应按阶段限定，而不是一次性开放全仓库修改权限。

## 4. 联机验证默认禁止代码扩散

联机验证前后的问题记录可以新增证据和文档；除非明确转入修复流，否则不应顺手扩大代码改动。

# Output format

严格使用以下结构输出：

## 变更边界声明

### 1. 本轮目标
-

### 2. 允许修改
- 目录：
- 文件类型：
- 允许原因：

### 3. 禁止修改
- 目录：
- 文件类型：
- 禁止原因：

### 4. 跨模块规则
- 是否允许跨模块：
- 若允许，限定为：

### 5. 最大允许变更层级
-

### 6. 越界判定条件
-
-

### 7. 若发现必须越界
- 应停止当前执行
- 回到哪个 skill：
- 需要补充什么证据：

# Handoff

- 上游：
  - `contract-freeze`
  - `feature-spec-and-plan`
  - `root-cause-analysis`
  - `boundary-audit`
- 下游：
  - `task-breakdown`
  - `task-execute`
  - `targeted-fix-and-regression`
  - `refactor-plan-and-migrate`
