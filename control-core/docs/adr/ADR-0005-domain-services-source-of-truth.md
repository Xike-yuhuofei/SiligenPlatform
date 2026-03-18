---
title: "ADR-0005: 领域服务成为业务规则唯一入口"
date: "2026-02-04"
status: Accepted
tags:
  - architecture
  - domain
  - services
modules:
  - src/domain
  - src/application
summary: >-
  将回零、点动、点胶、互锁、急停、配方生效等关键规则集中到领域服务，应用层用例改为委托调用，以消除重复逻辑并提升一致性与可测试性。
---

# Context

核心业务规则长期分散在 UseCase、旧编排器与状态机中，出现重复校验、规则差异与难以追溯的问题。随着子域增多，应用层逐步膨胀，领域层缺少清晰的规则归属，导致测试需要穿透多层才能覆盖真实业务逻辑。

项目已采用六边形架构，需要明确“规则只在领域层出现一次”的边界，避免应用层和适配器层重复实现规则。

# Decision

在领域层新增并统一使用以下领域服务作为业务规则唯一入口，应用层用例仅负责编排与参数映射：

- Dispensing：`DispensingController`、`PurgeDispenserProcess`、`SupplyStabilizationPolicy`
- Motion：`HomingProcess`、`JogController`、`InterpolationCommandValidator`、`InterpolationProgramPlanner`
- Safety：`EmergencyStopService`、`InterlockPolicy`
- Recipes：`RecipeActivationService`
- Machine：`CalibrationProcess`
- Diagnostics：`ProcessResultService`

移除旧的编排器/状态机（如 `DispensingWorkflowOrchestrator`、`SystemStateMachine`），并将相关用例改为委托领域服务执行。

# Consequences

## 正面影响

- 业务规则单一来源，减少重复实现与行为不一致风险。
- 用例更薄、更清晰，易于组合与复用。
- 领域服务可被直接单测，测试粒度更细、成本更低。

## 负面影响

- 领域服务数量增加，需要统一设计与维护。
- 迁移期需要同步调整用例、适配器与绑定配置。
- 规则变化会集中影响多个用例，需要更严格的评审与回归测试。

# References

- [ADR-0001](./ADR-0001-hexagonal-architecture.md): 采用六边形架构
- [领域层说明](../../src/domain/README.md)
- [领域层最佳实践](../library/domain-layer-best-practices.md)
