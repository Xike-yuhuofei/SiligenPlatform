---
Owner: @Xike
Status: active
Last reviewed: 2026-02-04
Scope: 控制卡/电机/点胶与供胶子系统
---

# 系统架构

## 一图看懂

```
[HMI / TCP Client]
        |
[apps/control-tcp-server]
        |
[modules/control-gateway]
        |
[apps/control-runtime]
        |
[Application Layer (UseCases)]
        |
[Domain Layer (Ports)]
        |
[modules/device-hal]
        |
[MultiCard 控制卡]
   /          |          \
[伺服驱动] [点胶阀] [供胶系统]
  -> [电机]    |        |
            [传感器] [气源]
```

## 模块边界(软件)

### src/domain/
- **motion**: 回零、点动、插补、轨迹规划与执行
  - 输入: 目标位置/速度/加速度/配置
  - 输出: 轨迹点序列/运动策略
  - 点动规则统一入口: `Domain::Motion::DomainServices::JogController`
  - 应用层/适配层不得复写点动校验与互锁判定
  - 插补规则统一入口: `InterpolationProgramPlanner` / `InterpolationCommandValidator` / `ValidatedInterpolationPort`
- **safety**: 急停、安全互锁、限位规则
- **machine**: 设备状态与运行模式（自动运行/手动/暂停/标定流程）
  - 自动运行/运行模式规则统一入口：`Domain::Machine::Aggregates::Legacy::DispenserModel`
  - 标定流程规则统一入口：`Domain::Machine::DomainServices::CalibrationProcess`
  - 标定设备与结果通过端口接入：`ICalibrationDevicePort` / `ICalibrationResultPort`
- **dispensing**: 点胶过程与工艺逻辑（胶路建压、点胶过程）
  - 输入: 轨迹、点胶参数
  - 输出: 点胶指令序列
  - 统一入口: `Domain::Dispensing::DomainServices::DispensingProcessService`
  - 触发计算: `Domain::Dispensing::DomainServices::DispensingController`
  - 胶路建压/稳压规则统一入口: `SupplyStabilizationPolicy` / `PurgeDispenserProcess` /
    `DispensingProcessService`
  - 配置通过 `IConfigurationPort` 提供；应用层不得复写规则；基础设施层仅负责阀门 I/O
- **diagnostics**: 诊断与工艺/测试结果记录
  - 结构化定义/校验/聚合在 Domain
  - 持久化通过 `ITestRecordRepository` 端口完成
- **recipes**: 配方生命周期与生效
  - 配方生效唯一入口：`Domain::Recipes::DomainServices::RecipeActivationService`
  - 审计记录通过 `IAuditRepositoryPort` 写入
- **triggering**: CMP触发配置
  - 输入: 触发位置、输出通道
  - 输出: CMP配置数据
- **ports**: 硬件控制抽象接口
  - IHardwareControllerPort: 运动控制、IO控制
  - ICalibrationDevicePort / ICalibrationResultPort: 标定设备执行与结果持久化

### 当前应用装配与编排
- **usecases**: 用例编排层
  - MotionControlUseCase: 运动控制用例
  - DispensingUseCase: 点胶用例
  - EmergencyStop: 紧急停止用例
- 当前 runtime 装配主入口位于 `apps/control-runtime/container` 与 `apps/control-runtime/bootstrap`

### 当前基础设施与设备实现
- **adapters/motion/controller**: 运动控制适配器
  - MultiCardMotionAdapter: MultiCard SDK封装
  - HardwareConnectionAdapter: 硬件连接管理
- **adapters/diagnostics/health**: 诊断与硬件测试
  - HardwareTestAdapter: 硬件测试适配器
- **adapters/system/persistence**: 配置/记录持久化
- 当前硬件适配器与驱动主实现位于 `modules/device-hal/src`

### 当前外部入口
- **TCP**: 当前程序化入口由 `apps/control-tcp-server` + `modules/control-gateway` 提供
- **HMI**: 通过 TCP 后端接入控制核心
- **CLI**: 面向人的独立 CLI 客户端已正式退役，不属于当前受支持入口；当前主链路仅保留 HMI + TCP backend

## 架构规范：自动运行/运行模式

- 规则唯一来源：`Domain::Machine::Aggregates::Legacy::DispenserModel`
- Application 用例仅做请求校验与流程编排，禁止复写自动运行/运行模式状态机或规则
- 新增的自动运行状态与规则必须落在 machine 子域（聚合或领域服务）内
- 状态变化事件由 Domain 产生（如 `SystemStateChangedEvent`），基础设施仅负责发布

## 关键数据流/时序

### 点胶动作时序

1. 解析DXF文件 → 生成轨迹点
2. Domain 统一生成插补程序/规划 → 生成运动指令
3. 应用层组装执行计划与运行参数，调用 `DispensingProcessService`
4. 领域层完成坐标系配置、触发计算、阀门时序与过程校验
5. 通过 `IInterpolationPort` / `IValvePort` 下发执行
6. 监控状态与错误

> 供胶阀打开后的稳压等待由 Domain 统一规则控制（`SupplyStabilizationPolicy`），应用层不实现时序。

## 架构规范：插补

- 插补策略选择、参数校验、插补程序生成必须位于 Domain（统一入口）。
- Application 仅做流程编排与参数映射；插补轨迹点生成只用于预览/分析，不得复写规则。
- Infrastructure 仅调用硬件插补 API，负责单位转换与错误映射，不包含策略/校验。

## 架构规范：工艺结果

- 工艺/测试结果的结构化定义、校验与聚合由 Domain 统一负责
- 统一入口：`Domain::Diagnostics::DomainServices::ProcessResultService`
- JSON 格式由 Domain 序列化模块生成并视为外部契约
- Infrastructure 仅负责持久化实现，不得重写结构或业务校验

## 互锁与安全

互锁与急停的判定规则属于领域层（Safety 子域），基础设施层负责采集硬件信号并执行停机动作。
互锁信号通过 `IInterlockSignalPort` 提供给领域策略（`InterlockPolicy`），实现“规则在 Domain、信号在 Infra”。

统一规范（强制）:
- `InterlockPolicy` 是互锁规则唯一入口与规则来源
- 应用层/基础设施层不得重复互锁判定，仅负责采集、转发与执行动作
- 诊断适配器保持硬件直读/直判，不纳入互锁统一入口

### 运动互锁条件
- 急停未触发
- 伺服使能状态
- 软限位未触发
- 硬限位未触发

### 点胶互锁条件
- 运动到位
- 气压达标
- 安全门关闭
- 无急停信号

### 紧急停止行为
- 立即停止所有运动
- 关闭所有输出
- 不等待任何操作
- 必须在10ms内完成

## 配方生效与审计

配方生效属于 Recipes 子域的核心规则，应用层仅编排调用，基础设施层仅负责持久化与适配：

- 统一入口：`Domain::Recipes::DomainServices::RecipeActivationService`
- 生效规则：仅已发布版本可生效；配方归档后禁止生效；生效状态由 `Recipe.active_version_id` 承载
- 审计规则：生效动作必须记录审计，写入 `IAuditRepositoryPort`，存储由基础设施层实现

## 依赖清单

### 硬件依赖
- 控制卡: 乐创 MultiCard (型号待补充)
- 固件版本: 待补充
- 驱动版本: MultiCard.dll x.y.z

### 软件依赖
- 操作系统: Windows 10/11
- 编译器: MSVC / Clang-CL
- 构建系统: CMake + Ninja

> 备注: 详细 IO 映射、阈值、版本矩阵请看 [参考手册](./06_reference.md)
