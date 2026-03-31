---
name: task-breakdown
description: "Break a frozen feature spec and plan into executable, verifiable, dependency-ordered tasks with stop conditions. Use for task breakdown, 任务拆分, 任务分解, and 实施排序."
---

# Purpose

本 skill 的唯一职责是：把规格与方案变成可执行任务，而不是让实现阶段临时自由发挥。

# Use this skill when

- 新功能规格和结构方案已经明确。
- 已经知道 owner 模块与主要落点。
- 需要将工作拆成可提交、可验证的任务单元。

# Do not use this skill when

- 当前尚未冻结功能规格。
- 当前是故障修复，且目标只是最小修复。
- 当前只是做一次性微小修改，不需要任务拆分。

# Inputs

- 功能规格
- 结构方案
- 契约冻结结果
- 变更边界声明
- 现有验证入口

# Required outputs

1. task 列表
2. 每个 task 的输入 / 输出
3. 串行 / 并行关系
4. 每个 task 的验证方式
5. 每个 task 的停止条件
6. 风险最高的 task 标识

# Hard prohibitions

- 不重新定义需求。
- 不把多个不同 owner 的改动硬塞成一个 task。
- 不输出无法验证的 task。
- 不省略停止条件。

# Breakdown rules

## 1. 先按 owner 和依赖拆

优先按 owner 模块和语义边界拆分，而不是按文件数量拆分。

## 2. 每个 task 必须可验证

task 完成后必须能判断“完成 / 未完成”，不能靠主观感受。

## 3. 高风险 task 应靠前暴露

与契约、状态机、执行真值相关的 task，应尽早验证，不应全部压到最后。

## 4. 任务粒度以“一次有意义提交”为宜

既不要大到横跨多个语义层，也不要小到只剩机械改名。

# Output format

严格使用以下结构输出：

## 任务分解清单

### Task 1
- 名称：
- 目标：
- 输入：
- 输出：
- 依赖：
- 验证方式：
- 停止条件：

### Task 2
- 名称：
- 目标：
- 输入：
- 输出：
- 依赖：
- 验证方式：
- 停止条件：

### 串并行关系
- 串行：
- 并行：

### 高风险任务
-
-

## 建议下一步
- 推荐进入：`task-execute`

# Handoff

- 上游：
  - `feature-spec-and-plan`
  - `change-scope-guard`
- 下游：
  - `task-execute`
