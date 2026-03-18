---
title: "ADR-0007: 轨迹几何工具抽取与Boost适配"
date: "2026-02-04"
status: Accepted
tags:
  - architecture
  - trajectory
  - geometry
modules:
  - src/domain/trajectory
  - src/shared/Geometry
summary: >-
  抽取轨迹几何公共函数为 GeometryUtils，并引入 Boost 几何适配封装距离/相交计算，减少重复实现并提升一致性。
---

# Context

轨迹域内多个服务重复实现角度归一化、弧长计算、线段方向等几何操作，且容差与实现细节不统一。随着轨迹能力扩展，重复代码增加了维护成本与算法差异风险。

# Decision

做出以下调整以统一几何计算：

- 抽取 `GeometryUtils` 作为轨迹几何的公共函数集合。
- 新增 `GeometryBoostAdapter` 封装 Boost.Geometry 的距离/相交计算能力。
- 轨迹相关领域服务统一使用上述工具，避免在服务内重复实现几何细节。

# Consequences

## 正面影响

- 统一几何计算实现，减少重复与差异。
- 复杂几何计算交由成熟库处理，降低错误概率。
- 轨迹服务更简洁，便于后续优化与测试。

## 负面影响

- 引入 Boost.Geometry 依赖与适配层，需关注构建与兼容性。
- 弧线/曲线计算采用采样近似时，需要明确精度与性能权衡。

# References

- [GeometryUtils](../../src/domain/trajectory/value-objects/GeometryUtils.h)
- [GeometryBoostAdapter](../../src/domain/trajectory/value-objects/GeometryBoostAdapter.h)
- [Shared Boost Adapter](../../src/shared/Geometry/BoostGeometryAdapter.h)
