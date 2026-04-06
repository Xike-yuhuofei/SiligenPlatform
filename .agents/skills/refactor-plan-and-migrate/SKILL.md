---
name: refactor-plan-and-migrate
description: "Plan staged refactors and migrations with validation gates, compatibility strategy, and rollback paths. Use for refactor planning, migration planning, 重构迁移, and 阶段化治理."
---

# Purpose

本 skill 的唯一职责是：把架构问题转化为可执行的阶段化迁移，而不是一次性大改。

# Use this skill when

- 已经通过边界审计确认存在结构性问题。
- 需要搬迁 owner、修正依赖方向、清理兼容层或拆分职责。
- 需要在不破坏主链路的前提下逐步治理结构。

# Do not use this skill when

- 当前问题只是单点 bug 修复。
- 当前还没有明确的边界审计结论。
- 当前只需要纯分析，不需要迁移方案。

# Inputs

- 边界审计报告
- 当前验证能力
- 风险约束
- 关键链路
- 允许修改范围

# Required outputs

1. 重构目标
2. 分阶段迁移计划
3. 每阶段的验证要求
4. 过渡期兼容策略
5. 回退策略
6. 剩余技术债

# Hard prohibitions

- 不做大爆炸迁移。
- 不一次性处理所有历史问题。
- 不在无验证兜底时迁移 owner。
- 不忽略过渡期兼容成本。

# Migration rules

## 1. 每阶段都必须可验证

不能把“最终全部完成后再统一验证”当作主策略。

## 2. 优先搬语义 owner，再清理表层

先解决 owner 和依赖方向，再处理表面目录美观。

## 3. 兼容层必须有退场计划

如果阶段性引入 bridge / compat，必须说明何时删掉。

## 4. 回退能力必须显式写出

即使理论上不打算回退，也要知道失败后如何恢复到安全状态。

# Output format

严格使用以下结构输出：

## 重构迁移计划与阶段结果

### 1. 重构目标
-
-

### 2. 分阶段计划
#### 阶段 1
- 目标：
- 变更范围：
- 验证要求：
- 回退方式：

#### 阶段 2
- 目标：
- 变更范围：
- 验证要求：
- 回退方式：

### 3. 过渡期兼容策略
-
-

### 4. 剩余技术债
-
-

### 5. 风险说明
-
-

## 建议下一步
- 阶段执行后进入：`closeout-branch`

# Handoff

- 上游：
  - `boundary-audit`
  - `change-scope-guard`
- 下游：
  - `closeout-branch`
