---
title: "ADR-0008: 基础设施适配器对齐领域服务"
date: "2026-02-04"
status: Accepted
tags:
  - architecture
  - infrastructure
  - adapters
  - wiring
modules:
  - src/infrastructure
  - src/application
  - src/adapters
summary: >-
  更新基础设施适配器与启动绑定以对接新的领域服务与端口，实现一致的依赖注入与接口对齐。
---

# Context

领域服务重构后，旧的适配器与启动绑定仍面向历史接口或分散逻辑，导致端口实现不一致、用例集成成本上升。为保持六边形架构边界，需要同步整理基础设施层与适配器层的对接方式。

# Decision

对基础设施适配器与启动绑定做统一对齐：

- 更新插补、配方、配置、互锁监控等适配器以实现新的端口契约。
- 使用 `ValidatedInterpolationPort` 作为插补端口的统一验证入口。
- 在 `InfrastructureBindingsBuilder` 中集中注册与注入新的端口与适配器。
- TCP/HMI 等外部适配器调用应用层用例，不直接实现领域规则。

# Consequences

## 正面影响

- 端口实现与注入方式统一，降低集成歧义。
- 适配器层职责更清晰，保持六边形架构依赖方向。
- 用例对接成本降低，便于新增或替换基础设施实现。

## 负面影响

- 启动绑定更复杂，需要维护更完整的依赖图。
- 配置与适配器变更增加集成测试工作量。

# References

- [ADR-0001](./ADR-0001-hexagonal-architecture.md): 采用六边形架构
- [ADR-0005](./ADR-0005-domain-services-source-of-truth.md): 领域服务成为业务规则唯一入口
- [基础设施绑定](../../apps/control-runtime/bootstrap/InfrastructureBindingsBuilder.cpp)
