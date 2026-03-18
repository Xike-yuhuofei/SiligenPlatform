# Dispensing 子域 - 点胶工艺

**职责**: 负责点胶过程与工艺逻辑、触发控制、阀门协调、胶路建压等高层业务流程

## 业务范围

- CMP 触发控制（单点、连续、范围）
- 位置触发计算
- 阀门开关协调
- 点胶路径管理
- 工艺参数配置
- 流量控制
- 点胶过程编排与状态管理
- 胶路建压/稳压流程
- 工艺结果评估与记录

> 触发约束：CMP 位置触发使用规划位置（Profile Position）作为比较源，不使用编码器反馈。

### 胶路建压/稳压架构规范

- 胶路建压/稳压规则统一由 Domain 层提供，应用层不得实现等待、超时或时序规则。
- 供胶阀稳压时间来源：`IConfigurationPort::GetDispensingConfig().supply_stabilization_ms`。
- 允许用例传入覆盖值，但必须经过 `SupplyStabilizationPolicy` 校验：
  - 取值范围：0-5000 ms
  - 0 表示使用配置默认值

## 目录结构

```
dispensing/
├── domain-services/                      # 领域服务
│   ├── DispensingProcessService          # 点胶过程编排与校验统一入口
│   ├── DispensingController              # 触发点/规划位置触发计算
│   ├── CMPTriggerService                 # CMP触发服务
│   ├── PurgeDispenserProcess             # 排胶/建压稳压流程
│   ├── SupplyStabilizationPolicy         # 供胶稳压策略（统一入口）
│   ├── PositionTriggerController         # 位置触发控制
│   ├── ValveCoordinationService          # 阀门协调
│   ├── PathOptimizationStrategy          # 路径优化
├── value-objects/                        # 值对象
│   ├── DispensingExecutionTypes          # 运行参数/执行计划/执行报告
└── ports/                                # 端口接口
    ├── IValvePort
    ├── ITriggerControllerPort
    ├── ITaskSchedulerPort
    └── IDispensingExecutionObserver
```

## 命名空间

```cpp
namespace Siligen::Domain::Dispensing {
    namespace Entities { ... }
    namespace ValueObjects { ... }
    namespace DomainServices { ... }
    namespace Ports { ... }
}
```

## 依赖关系

- ✅ 依赖: `shared/types`, `shared/utils`, `domain/_shared`
- ⚠️ 可选依赖: `domain/motion`（触发计算）
- ❌ 不依赖: `infrastructure`, `application`

## 架构规范（点胶过程）

- 点胶执行流程（阀门控制、触发配置、流程顺序与校验）必须由 `DispensingProcessService` 统一入口负责。
- 触发点/规划位置触发计算必须由 `DispensingController` 统一实现，禁止在应用层或基础设施层重复实现。
- 定时触发仅用于阀门单独控制（HMI 设置/调试链路），不参与 DXF 执行。
- 硬件插补程序由 Motion 子域统一生成与校验，点胶流程仅消费插补程序结果。
- 运行参数校验通过 `DispensingRuntimeOverrides::Validate` 完成，用例仅做请求映射与调用。
- 监控/采样仅通过 `IDispensingExecutionObserver` 扩展，禁止在应用层插入业务规则。
