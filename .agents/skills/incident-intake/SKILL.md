---
name: incident-intake
description: "Formalize an incident by defining the symptom, scope, severity, reproducibility, known facts, and unknowns before root-cause work. Use for incident intake, 故障受理, 异常受理, and 问题定义."
---

# Purpose

本 skill 的唯一职责是：先把故障定义清楚，再进入根因分析。

# Use this skill when

- 任务已归类为故障修复。
- 用户描述了报错、异常、性能下降、回归、预览不一致、执行异常等现象。
- 当前需要先界定问题，而不是立即修复。

# Do not use this skill when

- 当前任务是新功能设计。
- 当前任务是架构治理。
- 当前已经确认根因，只差执行修复。

# Inputs

- 故障现象
- 报错信息
- 截图 / 日志 / 样本
- 首次出现条件
- 影响范围
- 是否可复现
- 用户补充观察

# Required outputs

1. 故障定义
2. 影响范围
3. 严重级别
4. 复现状态
5. 初步相关链路
6. 已知事实
7. 当前未知点

# Hard prohibitions

- 不直接下根因结论。
- 不直接给修复方案。
- 不把用户猜测写成事实。
- 不省略复现信息。

# Intake rules

## 1. 现象必须可复述

要把“用户觉得有问题”整理成可判断的工程表述。

## 2. 影响范围必须分层

至少区分输入范围、功能范围、用户可见范围以及设备 / 运行风险范围。

## 3. 复现状态必须明确

- 可稳定复现
- 间歇复现
- 暂不可复现
- 证据不足

## 4. 未知点必须保留

不要用猜测填补未知。

# Output format

严格使用以下结构输出：

## 故障受理单

### 1. 故障现象
-
-

### 2. 影响范围
- 输入范围：
- 功能范围：
- 用户可见范围：
- 设备风险范围：

### 3. 严重级别
-

### 4. 复现状态
-
-

### 5. 初步相关链路
-
-

### 6. 已知事实
-
-

### 7. 当前未知点
-
-

## 建议下一步
- 推荐进入：`contract-freeze`（按需）或 `root-cause-analysis`

# Handoff

- 上游：
  - `task-classifier`
  - `context-bootstrap`
- 下游：
  - `contract-freeze`
  - `root-cause-analysis`
