---
title: "ADR-0009: 领域服务单元测试基线"
date: "2026-02-04"
status: Accepted
tags:
  - testing
  - domain
  - quality
modules:
  - tests/unit/domain
  - tests
  - src/domain
summary: >-
  为新增领域服务建立 GTest 单元测试基线，覆盖安全、运动、点胶、配方、诊断与轨迹规则，确保规则可回归且易维护。
---

# Context

业务规则集中到领域服务后，若仍依赖端到端测试验证，将导致回归成本高、定位困难。需要一套稳定、快速且可独立运行的单元测试基线来覆盖核心规则。

# Decision

在 `tests/unit/domain` 下新增并扩展领域服务单测，采用 GTest 与端口假实现（Fake/Mock）覆盖：

- 点胶流程与稳压策略
- 回零/点动与插补规划规则
- 互锁与急停策略
- 配方生效与诊断结果处理
- 轨迹几何与规划相关算法

同时更新测试构建配置，保证新增测试可被持续集成执行。

# Consequences

## 正面影响

- 领域规则具备快速、稳定的回归保障。
- 规则变更可在单测层快速发现影响。
- 便于团队理解与维护业务逻辑边界。

## 负面影响

- 维护成本增加，规则变化需同步更新测试。
- 单测数量增多，构建与执行时间上升。

# References

- [领域单测目录](../../tests/unit/domain)
- [测试构建配置](../../tests/CMakeLists.txt)
