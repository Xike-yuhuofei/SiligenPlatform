---
title: "ADR-0010: 移除遗留仿真驱动与过时文档"
date: "2026-02-04"
status: Accepted
tags:
  - maintenance
  - cleanup
  - infrastructure
modules:
  - src/infrastructure
  - docs
summary: >-
  删除不再维护的仿真驱动与旧文档，降低误用与维护成本，后续仿真能力需以端口/适配器方式重建。
---

# Context

旧仿真驱动与配套文档已长期未维护，接口和行为与当前六边形架构及领域服务不一致，容易造成误用和理解偏差。继续保留会增加维护负担与认知噪音。

# Decision

移除遗留仿真驱动及其相关过时文档，保持代码库聚焦于当前架构实现。后续如需仿真能力，应以端口/适配器方式重新建设，并纳入统一的依赖注入与测试策略。

# Consequences

## 正面影响

- 代码与文档更精简，减少历史包袱。
- 避免团队误用已失效的仿真接口。
- 为未来以规范方式重建仿真能力留出空间。

## 负面影响

- 暂时失去旧仿真驱动的可用性。
- 若需要仿真功能需重新设计与实现。

# References

- [ADR-0001](./ADR-0001-hexagonal-architecture.md): 采用六边形架构
- [Mock MultiCard](../../modules/device-hal/src/drivers/multicard/MockMultiCardWrapper.h)
