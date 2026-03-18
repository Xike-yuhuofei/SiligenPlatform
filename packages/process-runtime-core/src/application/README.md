# Application Layer (应用层)

## 目录

本层协调业务流程、处理用例和管理依赖注入。
回零/点动/急停/安全互锁/插补/自动运行/点胶过程/胶路建压/配方生效/工艺结果/标定流程的核心规则与状态归属于领域层，本层只负责编排与触发。
标定流程必须以 `Domain::Machine::DomainServices::CalibrationProcess` 为唯一规则入口，用例不得实现或复制标定状态/规则。
插补策略/参数校验/插补程序生成统一由 `Domain::Motion::DomainServices` 提供（如 `InterpolationProgramPlanner` / `ValidatedInterpolationPort`），应用层不得复写规则。
自动运行/运行模式必须以 `Domain::Machine::Aggregates::Legacy::DispenserModel` 为唯一规则入口，用例不得实现或复制运行模式状态机/规则。
点动流程必须以 `Domain::Motion::DomainServices::JogController` 为唯一规则入口，用例不得实现或复制点动规则。
互锁规则统一来源于 `motion-core`；应用层通过 `domain/safety/bridges/MotionCoreInterlockBridge.h` 调用模块能力，不实现互锁判定逻辑，仅执行动作。
急停业务规则必须以 `Domain::Safety::DomainServices::EmergencyStopService` 为唯一入口，用例/应用服务不得自行实现急停状态更新、清队列、禁用触发或硬件停机流程；`StopAllAxes(true)`/`StopAxis(..., true)` 仅表示立即停止动作，不能替代急停流程。
点胶执行流程必须以 `Domain::Dispensing::DomainServices::DispensingProcessService` 为统一入口，用例仅负责请求/响应与观察（如速度采样）。
配方校验/生效规则统一来源于 `process-core`；应用层通过 `recipes/RecipeUseCaseHelpers.h` 调用模块能力，不得直接修改配方生效状态或重复实现审计规则。
胶路建压/稳压必须通过领域层统一入口（`Domain::Dispensing::DomainServices::PurgeDispenserProcess` /
`DispensingProcessService` + `SupplyStabilizationPolicy`），用例不得自行计算稳压等待或超时。
如需覆盖稳压时间，仅允许在请求中提供 `supply_stabilization_ms`，由领域层校验范围（0-5000ms，0 表示使用配置默认值）。

### usecases/
用例实现,包括:
- `system/*` - 初始化/急停等系统用例
- `motion/*` - 回零/轨迹/手动控制用例
- `dispensing/*` - 点胶与阀门用例

> 说明: `*TestUseCase` 已迁移到 `tests/unit/usecases`，不再编译进应用层库。

### handlers/
事件处理器,负责处理领域事件

### container/
运行时容器兼容入口:
- 真实实现已迁入 `packages/runtime-host/src/container/`
- `src/application/container/ApplicationContainer.h` 仅保留兼容 include

## 依赖原则

- **依赖** shared层和domain层
- **不依赖** infrastructure或adapters层
- 通过接口使用基础设施服务

## 命名空间

```cpp
namespace Siligen::Application {
    namespace UseCases { }   // 用例
    namespace Handlers { }   // 事件处理器
    namespace DI { }         // 依赖注入
}
```
