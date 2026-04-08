# Domain Layer (领域层)

本层包含核心业务逻辑、领域模型和业务规则，不依赖具体技术实现。

## 目录结构（按子域）

- `_shared/`：跨子域共享的值对象、规约、领域事件
- `dispensing/`：点胶工艺子域（聚合/实体/值对象/领域服务/端口）
- `motion/`：运动控制子域
- `machine/`：设备与运行状态子域
- `safety/`：安全与互锁子域
- `diagnostics/` / `configuration/`：live concrete 已退出当前 bridge-domain；仅保留 `../include/domain/**` 下的兼容合同面
- `recipes/`：配方子域
- `system/`：系统级横切能力子域
- `planning/`：规划与可视化子域

端口接口按子域归类放置在 `*/ports/`，系统级端口位于 `system/ports/`。

## 领域能力归属（业务能力）

以下能力明确归属于领域层（业务规则与状态），应用层仅负责编排与触发：

- 回零（Homing）— Motion 子域
- 点动（Jog）— Motion 子域
- 插补（Interpolation）— Motion 子域
- 急停（Emergency Stop）— Safety 子域
- 安全互锁（Safety Interlock）— Safety 子域
- 自动运行（Auto Run / Run Mode）— Machine 子域
- 标定流程（Calibration Flow）— Machine 子域
- 点胶过程（Dispensing Process）— Dispensing 子域
- 胶路建压（Pressure Build-Up）— Dispensing 子域
- 工艺/测试结果（Process Result/Test Record）— Diagnostics 子域
- 配方生效（Recipe Activation）— Recipes 子域

### 插补统一规范

- 插补策略选择、参数校验、插补程序生成必须在 Motion 子域统一实现。
- Application 仅负责编排与参数映射，禁止复写插补规则。
- Infrastructure 仅调用硬件插补 API，负责单位转换与错误映射。

### 胶路建压/稳压统一规范

- `Dispensing::DomainServices::SupplyStabilizationPolicy` 是稳压时间解析与校验的唯一入口
- workflow 本地业务流程必须通过 `PurgeDispenserProcess` 触发稳压等待；DXF 执行态由 `runtime_execution/contracts/dispensing/IDispensingProcessPort` 承接
- 应用层禁止自行实现稳压等待、超时或时序规则
- 稳压时间来自 `IConfigurationPort::GetDispensingConfig().supply_stabilization_ms`；
  允许请求覆盖，但范围由领域层校验（0-5000ms，0 表示使用配置默认值）

### 标定流程统一规范

- `Machine::DomainServices::CalibrationProcess` 是标定流程唯一规则来源
- 应用层仅负责编排调用，禁止在用例中复写标定状态机或规则
- 标定设备与结果存储通过端口接入（`ICalibrationDevicePort` / `ICalibrationResultPort`）

### 自动运行/运行模式统一规范

- `Machine::Aggregates::Legacy::DispenserModel` 是自动运行/运行模式唯一规则来源
- 应用层仅负责编排调用，禁止在用例中复写运行模式状态机或规则
- 与自动运行相关的状态变化事件由领域层产生并通过端口发布

### 点动统一规范

- `Motion::DomainServices::JogController` 是点动规则唯一入口与规则来源
- 应用层/适配层不得重复实现点动参数校验、互锁判定或限位规则，仅负责请求转发与协议映射

### 互锁统一规范

- `motion-core` 是互锁规则的主实现与规则来源
- `Safety::DomainServices::InterlockPolicy` 保留为兼容包装层，内部委托 `motion-core`
- 应用层/基础设施层不得重复实现互锁判定，仅负责信号采集、状态转发与动作执行
- 诊断适配器保持硬件直读/直判，不纳入互锁统一入口

### 急停统一规范

- `Safety::DomainServices::EmergencyStopService` 是急停规则唯一入口与规则来源
- 急停流程（运动急停、禁用触发、清任务队列、硬件停机、状态置位）由该服务统一编排与校验
- 应用层/适配层仅负责触发与结果处理，不得在用例或服务中复制急停状态更新或清理逻辑
- `StopAllAxes(true)` / `StopAxis(..., true)` 仅表示立即停止动作，不能替代急停流程

### 配方生效统一规范

- `process-core` 是配方校验与生效规则的主实现与规则来源
- `Recipes::DomainServices::RecipeActivationService` / `RecipeValidationService` 保留为兼容包装层，内部委托 `process-core`
- 配方 JSON 编解码已拆到独立 target `siligen_recipe_json_codec`，不再并入 `siligen_recipe_domain_services`
- 应用层仅负责发布/显式激活的编排与参数校验，禁止直接改写 `Recipe.active_version_id`
- 领域层通过 `IRecipeRepositoryPort` 持久化配方与版本状态，通过 `IAuditRepositoryPort` 记录审计

### 工艺结果统一规范

- 当前 workflow bridge-domain 不再编译 `ProcessResultService` / `ProcessResultSerialization` concrete
- 诊断合同面仅保留 `TestDataTypes` / `TestRecord` / `IDiagnosticsPort` / `ITestRecordRepository` 等 public contracts
- 持久化仅通过 `ITestRecordRepository` 端口完成，Infrastructure 不得重写规则或结构

## 依赖原则

- **只依赖** shared层的接口和类型
- **不依赖** infrastructure、application 或 adapters 层
- 所有外部依赖通过端口接口注入

## 命名空间

```cpp
namespace Siligen::Domain {
    namespace Dispensing { }
    namespace Motion { }
    namespace Machine { }
    namespace Safety { }
    namespace Diagnostics { }
    namespace Configuration { }
    namespace System { }
    namespace Planning { }
}
```
