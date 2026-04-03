---
name: targeted-fix-and-regression
description: "Apply the smallest valid fix after root-cause convergence and run direct plus adjacent regression checks. Use for minimal fixes, targeted regression, 最小修复, and 针对性回归."
---

# Purpose

本 skill 的唯一职责是：在根因已收敛的前提下，实施最小修复，并验证主故障与邻近链路。

# Use this skill when

- 根因分析已足够收敛。
- 已有变更边界声明。
- 当前任务目标是修复问题，不是做结构重构。

# Do not use this skill when

- 当前根因仍不明确。
- 当前任务其实是新功能实现。
- 当前已判断需要大规模重构，应转入架构治理流。

# Inputs

- 根因分析单
- 契约冻结结果（若有）
- 变更边界声明
- 相关代码落点
- 现有测试入口

# Required outputs

1. 最小修复内容
2. 代码落点
3. 主故障回归结果
4. 邻近路径回归结果
5. 是否需新增自动化测试
6. 剩余风险

# Hard prohibitions

- 不扩大成结构重构。
- 不混入 unrelated cleanup。
- 不只验证主故障消失。
- 不在没有回归的情况下宣布修复完成。

# Fix rules

## 1. 修复必须对应根因

每个修改点都应能回溯到根因结论，而不是“多改一点更保险”。

## 2. 主故障与邻近路径都要验证

至少验证原故障路径、直接相邻路径和已知高风险边界条件。

## 3. 必要时补测试

如果该故障容易回归，应输出具体测试补强建议。

## 4. 一旦发现超出边界，应停止升级为重构

不要在修复流里偷偷切到架构治理流。

# Output format

严格使用以下结构输出：

## 修复与回归结果单

### 1. 最小修复
- 代码落点：
- 修复内容：
- 对应根因：

### 2. 主故障回归
- 用例：
- 结果：

### 3. 邻近路径回归
- 用例：
- 结果：

### 4. 是否新增测试
- 是 / 否
- 建议：

### 5. 剩余风险
-
-

## 建议下一步
- 推荐进入：`branch-closeout`
- 若发现结构性问题：转 `boundary-audit`

# Handoff

- 上游：
  - `root-cause-analysis`
  - `change-scope-guard`
- 下游：
  - `branch-closeout`
  - `boundary-audit`
