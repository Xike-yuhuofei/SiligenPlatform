---
title: "ADR-0006: 互锁策略统一与安全检查集成"
date: "2026-02-04"
status: Accepted
tags:
  - architecture
  - safety
  - interlock
  - motion
modules:
  - src/domain/safety
  - src/domain/motion
  - src/application
  - src/infrastructure
summary: >-
  将互锁判定集中到 InterlockPolicy，并在限位监控、点动与回零流程中统一调用，确保安全规则一致、可追溯且便于测试。
---

# Context

互锁判定、急停/限位检查曾分散在监控器、用例与控制流程中，导致优先级、错误码和处理策略不一致，存在遗漏安全条件的风险。随着安全规则增多，需要一个可复用、可审计的统一入口。

# Decision

确立 `Safety::DomainServices::InterlockPolicy` 为互锁规则唯一入口，并完成以下集成：

- 软/硬限位监控使用统一的互锁判定与触发规则。
- `MotionSafetyUseCase` 与相关流程统一调用互锁判定结果。
- `JogController` 与 `HomingProcess` 通过 `InterlockPolicy` 执行点动/回零互锁检查。
- 互锁信号统一通过端口读取，保持六边形架构依赖方向。

# Consequences

## 正面影响

- 安全规则集中，优先级与错误处理一致。
- 互锁判定可单测验证，便于回归与审计。
- 运动流程与监控器共享规则，减少重复实现。

## 负面影响

- 互锁规则变更会影响多个流程，需要更严格的回归测试。
- 端口需提供完整的互锁信号快照，集成成本增加。
- 部分行为可能变得更严格，需要同步更新流程期望与文档。

# References

- [ADR-0005](./ADR-0005-domain-services-source-of-truth.md): 领域服务成为业务规则唯一入口
- [互锁策略定义](../../src/domain/safety/domain-services/InterlockPolicy.h)
- [互锁策略单测](../../tests/unit/domain/safety/InterlockPolicyTest.cpp)
