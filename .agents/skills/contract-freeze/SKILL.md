---
name: contract-freeze
description: "Freeze the semantic contract for the current task by identifying authority artifacts, owner layers, consumers, and contract boundaries. Use for 契约冻结, 真值冻结, owner/consumer 判断, and 语义边界确认."
---

# Purpose

本 skill 的唯一职责是：在任何涉及设备行为、状态、几何、工艺、轨迹、触发、执行、预览或联机动作的任务中，先冻结最小契约，再允许后续设计、修复或实现。

本 skill 的目标不是复述现有实现，而是识别“谁拥有语义解释权，谁只是消费者”。

# Use this skill when

- 任务涉及几何、工艺、轨迹、触发、执行、预览、状态机或联机行为。
- 需要回答哪个工件是 `authority`。
- 需要回答哪一层是 `owner`，哪一层只允许消费。
- 需要保证预览链、执行链、诊断链不出现多重语义 owner。
- 即将进入功能设计、根因定位或代码实现，但当前契约尚未冻结。

# Do not use this skill when

- 当前任务只是单纯的文案、样式或无业务语义的局部修改。
- 当前已经有清晰、冻结且适用的正式契约，且本轮不影响它。
- 当前问题只是收尾阶段整理，不涉及行为或语义变化。

# Inputs

- 任务目标或故障现象
- 相关架构说明
- 接口 / DTO / 中间工件定义
- 状态机定义
- 关键 service / planner / adapter / view model
- 现有测试
- 用户明确说明的预期语义

# Required outputs

1. 输入工件
2. 输出工件
3. 关键中间工件
4. `authority` 工件
5. `owner` 层与 consumer 层
6. 契约风险与证据不足项

# Hard prohibitions

- 不把“当前实现碰巧这样做”当作“正确契约”。
- 不在证据不足时伪造 `authority`。
- 不允许多个层同时拥有同一语义解释权。
- 不直接给实现细节。
- 不把桥接层、兼容层、视图层误认成 owner。

# Contract analysis rules

## 1. 先识别语义对象

优先识别本轮真正关心的语义对象，例如设备状态、几何输入、工艺段、触发条件、执行轨迹、预览结果或诊断事件。

## 2. 再识别链路层级

至少判断输入层、规划层、执行层、展示层、诊断层、适配层中的相关层。

## 3. 找 authority，不找“最方便解释的对象”

`authority` 的判定标准不是“最容易看见”，而是是否被下游真实执行或真实判断依赖，以及是否定义了关键语义。

## 4. 拆分 owner 与 consumer

必须明确：

- owner：有定义语义权
- consumer：只能使用、呈现、透传、适配，不得重新发明语义

# Output format

严格使用以下结构输出：

## 最小契约冻结单

### 1. 本轮语义对象
-
-

### 2. 输入工件
- 名称：
- 生产者：
- 消费者：
- 说明：

### 3. 输出工件
- 名称：
- 生产者：
- 消费者：
- 说明：

### 4. 关键中间工件
- 名称：
- 作用：
- 是否承载语义：

### 5. authority 工件
- 名称：
- 依据：
- 为什么不是其他工件：

### 6. owner 层与 consumer 层
- owner：
- consumer：
- 禁止重解释的层：

### 7. 契约风险
- 多 owner 风险：
- 旁路真值风险：
- 兼容层污染风险：
- 证据不足项：

## 建议下一步
- 推荐进入的下一个 skill：

# Handoff

- 上游：
  - `context-bootstrap`
  - `feature-clarify`
  - `incident-intake`
- 下游：
  - 新功能：`feature-spec-and-plan`
  - 故障修复：`root-cause-analysis`
  - 实现前边界控制：`change-scope-guard`
