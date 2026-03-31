---
name: feature-clarify
description: "Clarify feature intent, non-goals, success criteria, user-visible outcomes, and boundaries before planning. Use for feature clarification, 功能澄清, 需求澄清, and 非目标确认."
---

# Purpose

本 skill 的唯一职责是：把“想新增什么能力”说清楚，不让实现阶段代替需求定义阶段。

# Use this skill when

- 任务已经被归类为新功能。
- 用户提出新增能力、流程、页面、接口、契约或行为变化。
- 当前仍存在“到底要实现什么”的歧义。

# Do not use this skill when

- 当前任务是故障修复，重点是先定位原因。
- 当前任务是架构治理，重点是边界审计。
- 当前需求已经冻结，且只差进入实现。

# Inputs

- 用户目标
- 业务背景
- 期望结果
- 当前痛点
- 已知约束
- 相关现有功能

# Required outputs

1. 功能目标
2. 非目标
3. 用户可见结果
4. 成功标准
5. 边界条件
6. 涉及层级
7. 待确认风险

# Hard prohibitions

- 不直接输出代码方案。
- 不提前决定文件落点。
- 不把实现偏好写成需求。
- 不省略失败场景与边界条件。

# Clarification rules

## 1. 目标与非目标必须同时写

只写“要做什么”不够，必须明确“本轮不做什么”。

## 2. 成功标准必须可判断

不能写成“更合理”“更好用”这种不可验证表述。

## 3. 用户可见结果与内部实现分开

先定义用户或系统最终能观察到什么，再谈内部结构。

## 4. 边界条件必须显式写出

例如：

- 哪些输入不支持
- 哪些状态下不可触发
- 是否要求与执行真值一致
- 是否允许仅预览不执行

# Output format

严格使用以下结构输出：

## 功能澄清单

### 1. 功能目标
-
-

### 2. 非目标
-
-

### 3. 用户可见结果
-
-

### 4. 成功标准
-
-

### 5. 边界条件
-
-

### 6. 涉及层级
-
-

### 7. 关键风险 / 待确认项
-
-

## 建议下一步
- 推荐进入：`contract-freeze` 或 `feature-spec-and-plan`

# Handoff

- 上游：
  - `task-classifier`
  - `context-bootstrap`
- 下游：
  - `contract-freeze`
  - `feature-spec-and-plan`
