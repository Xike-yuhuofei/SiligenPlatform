---
name: boundary-audit
description: "Audit module ownership, dependency direction, and boundary violations across apps, modules, shared, and adapters. Use for boundary audit, 边界审计, 职责审查, and owner 漂移检查."
---

# Purpose

本 skill 的唯一职责是：从架构层面判断“结构是否正确”，而不是检查代码风格或局部实现技巧。

# Use this skill when

- 任务目标是审查模块职责、边界、依赖方向。
- 需要判断 `apps/`、`modules/`、`shared/` 分工是否真正落地。
- 需要识别 bridge、compat、fallback 是否承载了真实实现。
- 需要为重构迁移提供证据。

# Do not use this skill when

- 当前只是在做单点 bug 修复。
- 当前只是新增小功能，不涉及结构边界。
- 当前需要的是联机验证计划。

# Inputs

- 目录结构
- 模块实现
- 架构文档
- 依赖关系
- 关键链路代码
- 用户明确关注点

# Required outputs

1. 模块实际职责
2. 边界是否清晰
3. 依赖方向是否合理
4. 是否存在职责重叠 / 漂移
5. 是否存在 bridge / fallback / compat 污染
6. 证据文件路径
7. 风险等级排序

# Hard prohibitions

- 不只看命名风格。
- 不用“代码能跑”掩盖结构问题。
- 不直接跳入重构实现。
- 不给无证据的泛泛结论。

# Audit rules

## 1. 先看说明，再看实现

优先阅读架构说明、目录规则和模块说明，再下钻实现。

## 2. 关注 owner，而不是表面目录名

名字叫 adapter，不代表它真的只是 adapter；要看它是否承载业务语义。

## 3. 重点审 4 类问题

- 职责重叠
- 职责漂移
- 依赖反向
- 兼容层污染

## 4. 结论必须绑定代码证据

每个结论都应能指出具体路径、文件或调用链。

# Output format

严格使用以下结构输出：

## 边界审计报告

### 1. 审计对象
-
-

### 2. 模块实际职责
- 模块：
- 实际职责：

### 3. 边界判断
- 清晰 / 不清晰
- 证据：

### 4. 依赖方向判断
- 合理 / 不合理
- 证据：

### 5. 结构性问题
- 职责重叠：
- 职责漂移：
- bridge / compat / fallback 污染：
- 语义多 owner：

### 6. 风险排序
1.
2.
3.

### 7. 证据路径
-
-

## 建议下一步
- 推荐进入：`refactor-plan-and-migrate`
- 若只是局部修复：转 `change-scope-guard`

# Handoff

- 上游：
  - `task-classifier`
  - `context-bootstrap`
- 下游：
  - `refactor-plan-and-migrate`
  - `change-scope-guard`
