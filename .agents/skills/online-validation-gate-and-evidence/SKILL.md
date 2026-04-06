---
name: online-validation-gate-and-evidence
description: "Gate real-hardware validation with readiness checks, limited-motion steps, observation points, abort conditions, and evidence capture. Use for online validation, 联机准入, 联机证据, and 真实设备验证."
---

# Purpose

本 skill 的唯一职责是：把联机验证当成受控工程流程，而不是“直接上机试一下”。

# Use this skill when

- 本轮目标涉及真实设备验证。
- 需要确认是否具备联机资格。
- 需要制定限速、限范围、限动作的验证计划。
- 需要定义观察点、停机条件和结果沉淀方式。

# Do not use this skill when

- 当前还没有完成最低限度的离线验证。
- 当前只是普通离线测试。
- 当前只是新功能设计、根因分析或结构审查。
- 当前并不涉及真实设备动作。

# Inputs

- 本轮改动目标
- 离线验证结果
- 设备状态与前提条件
- 涉及的轴 / 阀 / IO / 传感器
- 现场约束
- 操作前提
- 预期观察点

# Required outputs

1. 是否允许联机
2. 联机前提条件
3. 风险等级
4. 限幅验证步骤
5. 关键观察点
6. 停机 / 中止条件
7. 验证结果记录方式
8. 回写文档 / 测试建议

# Hard prohibitions

- 不跳过离线验证直接联机。
- 不在未定义停机条件时继续验证。
- 不用“先试试看”替代准入审查。
- 不只输出成功 / 失败，不输出证据。

# Validation rules

## 1. 准入先于动作

先判断“能不能上机”，再判断“怎么上机”。

## 2. 限幅先于放量

优先：

- 限速度
- 限行程
- 限频率
- 限输出时间
- 单变量验证

## 3. 观察点必须前置定义

至少明确：

- 设备状态
- 关键 IO
- 关键日志
- 运动 / 触发 / 执行反馈
- 异常迹象

## 4. 结果必须回写

联机验证不是口头确认；必须沉淀为验证记录、风险说明、后续离线补强建议和必要文档更新点。

# Output format

严格使用以下结构输出：

## 联机验证准入与记录单

### 1. 联机目标
-
-

### 2. 是否允许联机
- 允许 / 不允许
- 依据：

### 3. 联机前提条件
-
-

### 4. 风险等级
-
-

### 5. 限幅验证步骤
1.
2.
3.

### 6. 关键观察点
- 状态：
- IO：
- 日志：
- 反馈：

### 7. 停机 / 中止条件
-
-

### 8. 验证结果记录
- 结果：
- 发现的问题：
- 需补强的测试 / 文档：

## 建议下一步
- 通过后：`closeout-branch`
- 未通过：转 `incident-intake` 或返回离线修复流

# Handoff

- 上游：
  - `task-classifier`
  - `context-bootstrap`
  - 离线验证结果
- 下游：
  - `closeout-branch`
  - `incident-intake`
