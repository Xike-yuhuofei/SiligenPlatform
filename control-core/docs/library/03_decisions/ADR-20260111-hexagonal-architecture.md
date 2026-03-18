---
Owner: @Xike
Status: active
Last reviewed: 2026-01-11
Scope: 整体架构
---

# ADR-20260111-六边形架构

## Context(背景)
- 项目是工业运动控制系统,需要对接多种硬件(控制卡、传感器)
- 需要进行单元测试,但直接依赖硬件会导致测试困难
- 硬件接口可能变化,核心业务逻辑应该稳定
- 影响范围: 整个后端系统架构

## Decision(决策)
- 我们决定: 采用六边形架构(Hexagonal Architecture),将领域逻辑与外部依赖隔离

## Consequences(影响/代价)
### 正面
- 领域层不依赖硬件,可以进行纯单元测试
- 硬件变化只需修改Adapter,不影响核心逻辑
- 代码职责清晰,易于维护

### 负面/风险
- 增加了抽象层,代码量增加
- 学习曲线较陡
- 需要编写更多的接口和适配器代码

## Alternatives considered(备选方案)
- 方案 A: 传统分层架构(UI -> Business -> Data Access)
  - 为什么没选: 业务逻辑容易与数据访问耦合,难以测试
- 方案 B: 直接调用硬件API
  - 为什么没选: 无法进行单元测试,硬件依赖太强

## Rollout / Migration(落地与迁移)
- 项目从0开始采用此架构
- 无需迁移

## References(参考)
- 相关 PR: N/A(初始架构)
- 当前架构说明: `docs/library/02_architecture.md`
