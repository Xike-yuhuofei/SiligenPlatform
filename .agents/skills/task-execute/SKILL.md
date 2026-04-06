---
name: task-execute
description: "Execute already-scoped tasks with minimal implementation, bounded changes, and immediate validation feedback. Use for task execution, 任务执行, 实施推进, and 最小实现."
---

# Purpose

本 skill 的唯一职责是：在边界内实施，不重新发明需求、不重新定义契约、不擅自扩大任务。

# Use this skill when

- 已有明确的规格、方案、任务拆分和变更边界。
- 当前进入新功能的实施阶段。
- 需要按任务推进并输出阶段性结果。

# Do not use this skill when

- 当前仍在需求澄清、根因定位或边界审计阶段。
- 当前没有明确的 task 与边界。
- 当前任务实际是故障修复或联机验证。

# Inputs

- 功能规格
- 结构方案
- task 清单
- 变更边界声明
- 契约冻结结果
- 当前代码与测试入口

# Required outputs

1. 已完成的 task
2. 对应代码落点
3. 已补充或需补充的测试
4. 文档更新点
5. 未完成项 / 阻塞项
6. 当前是否适合进入 closeout

# Hard prohibitions

- 不重写需求。
- 不改变 authority 或 owner 结论。
- 不扩大改动范围。
- 不顺手做 unrelated cleanup。
- 不跳过最小验证直接宣称完成。

# Execution rules

## 1. 按 task 顺序推进

默认遵守任务依赖顺序；若要调整，必须说明原因。

## 2. 实现必须对应规格

每一项改动都应能回溯到规格和 task，而不是“想到什么做什么”。

## 3. 最小验证随 task 同步进行

不是全部做完再一次性验证，而是每个 task 应有最小验证闭环。

## 4. 发现越界应立即停止

若发现本轮必须跨出边界，必须停止并回到上游 skill 重新冻结。

# Output format

严格使用以下结构输出：

## 实现结果单

### 1. 已完成任务
- Task：
- 代码落点：
- 结果：

### 2. 最小验证
- 验证项：
- 结果：

### 3. 测试 / 文档补充
-
-

### 4. 未完成项 / 阻塞项
-
-

### 5. 是否越界
- 否 / 是
- 若是，越界原因：

## 建议下一步
- 推荐进入：`closeout-branch`
- 若越界：返回 `change-scope-guard` 或 `feature-spec-and-plan`

# Handoff

- 上游：
  - `task-breakdown`
  - `change-scope-guard`
- 下游：
  - `closeout-branch`
