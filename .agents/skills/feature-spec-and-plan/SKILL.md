---
name: feature-spec-and-plan
description: "Turn a clarified feature into a spec and design plan with behavior, contract changes, owner modules, dependency rules, and implementation order. Use for feature spec, design planning, 功能规格, and 结构方案."
---

# Purpose

本 skill 的唯一职责是：在新功能流中，把已经澄清过的需求和已冻结的契约，转化为可执行的规格与结构方案。

它回答的是：

- 这个功能到底要表现成什么行为
- 数据和状态会怎么变化
- 代码应该落在哪一层、哪个 owner
- 应该如何分阶段实施

# Use this skill when

- 当前任务已经确认属于新功能流。
- 已经完成需求澄清，或者至少已明确目标与成功标准。
- 已经完成或足以完成最小契约冻结。
- 下一步准备进入任务拆分或实现。

# Do not use this skill when

- 当前任务是故障修复，目标是先定位原因。
- 当前任务是架构治理，重点在边界审计。
- 当前任务只是小范围、无行为变化的机械性改动。
- 当前信息不足以定义功能行为和落点。

# Inputs

- 功能目标
- 非目标
- 成功标准
- 用户可见结果
- 契约冻结结果
- 当前模块结构
- 当前验证入口
- 已知约束与风险

# Required outputs

1. 行为规格
2. 数据 / 契约变化
3. 状态影响
4. 代码落点
5. 依赖方向
6. 实施顺序

# Hard prohibitions

- 不直接改代码。
- 不跳过规格只给“直觉方案”。
- 不把兼容层、桥接层、视图层当作长期 owner。
- 不忽略失败路径和回退语义。
- 不在没有契约冻结时擅自定义 authority。

# Planning rules

## 1. 先定义行为，再定义落点

先明确行为是什么、谁拥有该行为、谁只应该消费结果，而不是先看哪个文件改起来方便。

## 2. owner 优先于 adapter

优先让 owner 模块承载业务能力。adapter 只做适配，不做最终业务语义 owner。

## 3. 避免双真值

如果功能同时影响预览和执行，必须明确谁是真值来源、谁是派生结果、是否允许旁路生成。

## 4. 先考虑可验证性

方案必须指出：

- 哪一层最容易建立验证
- 哪些环节必须补测试
- 哪些风险只能在后续联机验证中确认

# Output format

严格使用以下结构输出：

## 功能规格与结构方案

### 1. 功能目标
-
-

### 2. 非目标
-
-

### 3. 行为规格
- 触发条件：
- 主要流程：
- 成功结果：
- 失败场景：

### 4. 数据 / 契约变化
- 新增：
- 修改：
- 不变但依赖的契约：

### 5. 状态影响
- 是否影响状态机：
- 是否影响事件 / 失败分支：
- HMI 是否仅展示：

### 6. 代码落点
- owner 模块：
- 推荐目录：
- 不应落点：

### 7. 依赖方向
- 允许依赖：
- 禁止依赖：
- 需要新增的对象：

### 8. 实施顺序
1.
2.
3.

### 9. 主要风险
-
-

## 建议下一步
- 推荐进入：`task-breakdown`

# Handoff

- 上游：
  - `feature-clarify`
  - `contract-freeze`
- 下游：
  - `change-scope-guard`
  - `task-breakdown`
