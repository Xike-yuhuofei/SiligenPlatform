---
name: task-classifier
description: "Classify the current request into feature, incident, boundary-audit, or online-validation workflows. Use for task routing, workflow selection, 任务归类, 流程分流, and 禁止跨流判断."
---

# Purpose

本 skill 的唯一职责是：在任何实施动作开始前，先判定当前任务属于哪一种工程流程，并明确下一步应该进入哪个 skill。

它解决的问题不是“怎么做”，而是“这件事应该按哪条流程做”。

# Use this skill when

- 用户提出一个新任务，但任务类型尚未明确。
- 用户只描述了目标、现象、问题或想法，尚未确认后续流程。
- 需要判断任务更适合走新功能流、故障修复流、架构治理流还是联机验证流。
- 需要在正式工作前阻止流程混淆，例如把故障修复误当作新功能，或把联机验证误当作普通离线测试。

# Do not use this skill when

- 当前任务类型已经明确，且已经进入具体子流程。
- 当前需要的是详细设计、根因分析、实现、回归或收尾。
- 当前问题只是需要补充少量上下文，而不是重新归类任务。

# Inputs

尽可能从用户输入、仓库说明和现有文档中提取以下信息：

- 用户目标
- 当前现象或问题描述
- 期望结果
- 是否涉及真实设备、真实 IO、真实轴、真实阀
- 是否涉及模块职责、目录边界、依赖方向
- 是否已经存在报错、日志或复现路径
- 是否已经明确要新增能力或修改行为

如果信息不足，可以明确写出“证据不足”，但仍必须给出当前最合理的初步归类。

# Required outputs

1. 任务类型
   - `feature`
   - `incident`
   - `boundary-audit`
   - `online-validation`
2. 归类依据
3. 推荐后续流程
4. 当前禁止动作
5. 风险标签

# Hard prohibitions

- 不改代码。
- 不直接给实现方案。
- 不直接下根因结论。
- 不跳过任务归类直接进入实现、联机或重构。
- 不用“都有一点”“先试试看”这类模糊措辞替代分类。

# Classification rules

## 1. 联机验证优先识别

如果本轮核心目标涉及以下任一项，优先考虑归类为 `online-validation`：

- 真实设备动作验证
- 真实轴运动
- 真实阀输出
- 真实 IO 联动
- 安全联锁
- 现场验证计划
- 联机观察点
- 停机 / 回退条件

## 2. 架构治理其次识别

如果核心目标是模块职责、目录边界、依赖方向、bridge/fallback/compat 污染审查，应归类为 `boundary-audit`。

## 3. 故障修复再次识别

如果描述中包含报错、异常现象、性能下降、回归、失败样本、日志分析或“先定位原因”，应归类为 `incident`。

## 4. 其余默认为新功能

新增能力、新增契约、新增模块行为、HMI 或系统行为扩展，默认归类为 `feature`。

# Output format

严格使用以下结构输出：

## 任务归类结论
- 类型：
- 归类依据：
- 风险标签：

## 推荐后续流程
1.
2.
3.

## 当前禁止动作
-
-
-

## 备注
- 信息充足 / 证据不足：
- 若证据不足，当前最合理初判为：

# Handoff

- 上游：通常无，也可作为任何非平凡任务的第一个 skill。
- 下游：
  - 新功能：`context-bootstrap` -> `feature-clarify`
  - 故障修复：`context-bootstrap` -> `incident-intake`
  - 架构治理：`context-bootstrap` -> `boundary-audit`
  - 联机验证：`context-bootstrap` -> `online-validation-gate-and-evidence`
