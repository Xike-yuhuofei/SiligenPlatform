# 应用层用例 (Application Use Cases)

## 概述

用例(Use Cases)是应用层的核心组件,负责编排业务流程。每个用例代表一个完整的业务操作,通过调用领域端口或稳定 contract 完成工作。
涉及回零、点动、急停、安全互锁、插补、点胶过程、胶路建压、配方生效、工艺结果的业务规则与状态均在领域层实现，用例只负责流程编排与端口调用。
标定执行流程统一通过 `runtime_execution/contracts/system/ICalibrationExecutionPort` 与 `ICalibrationResultSink` 承接；`M5 coordinate-alignment` 仅负责测量事实到 `CoordinateTransformSet` 的结果收敛。
插补相关规则（策略选择、参数校验、插补程序生成）统一在 Domain 层完成，用例只做参数映射与调用，禁止重复实现。
运行时执行态、急停态、手动运动许可等跨模块状态必须通过 `runtime_execution/contracts/system/IMachineExecutionStatePort` 读取，不得在应用层直接依赖旧 machine 聚合或历史 calibration 兼容类型。
安全互锁用例与监控的规则统一来源于 `motion-core`；用例与监控通过 `domain/safety/bridges/MotionCoreInterlockBridge.h` 调用模块能力，不得复写互锁判定逻辑。
配方校验/生效规则统一来源于 `process-core`；用例通过 `recipes/RecipeUseCaseHelpers.h` 调用模块能力，不得直接修改配方生效状态或重复实现审计规则。
胶路建压/稳压流程必须调用领域层统一入口（`Domain::Dispensing::DomainServices::PurgeDispenserProcess` /
`DispensingProcessService` + `SupplyStabilizationPolicy`），用例不得实现稳压等待、超时或建压时序。
如需覆盖稳压时间，仅允许通过请求参数 `supply_stabilization_ms` 传递，范围由领域层校验（0-5000ms，0 表示使用配置默认值）。

## 设计原则

### 1. 单一职责
- 每个用例只负责一个业务流程
- 用例之间相互独立,不直接调用

### 2. 依赖端口接口
```
应用层用例
    ↓ (依赖)
领域层端口接口
    ↑ (实现)
基础设施层适配器
```

### 3. 请求-响应模式
```cpp
struct SomeRequest { /* 输入参数 */ };
struct SomeResponse { /* 输出结果 */ };

Result<SomeResponse> Execute(const SomeRequest& request);
```

### 4. 点胶流程统一入口
- 点胶执行流程（阀门控制、触发配置、流程顺序与校验）必须由 `Domain::Dispensing::DomainServices::DispensingProcessService` 统一负责。
- 用例只做请求映射、端口编排与可观测性扩展（如速度采样），不得复制流程规则。

## 用例列表

### InitializeSystemUseCase
**职责**: 系统初始化
- 加载/验证配置
- 连接硬件并可选启动心跳/监控
- 可选健康检查
- 可选回零操作

**依赖**:
- IConfigurationPort
- DeviceConnectionPort
- HomeAxesUseCase
- IDiagnosticsPort
- IEventPublisherPort (可选)

### HomeAxesUseCase
**职责**: 轴回零操作
- 验证请求
- 使用已加载配置（轴数量/范围）
- 执行回零
- 等待完成
- 验证状态

**依赖端口**:
- IHomingPort
- IConfigurationPort
- IMotionConnectionPort
- IEventPublisherPort

### ExecuteTrajectoryUseCase
**职责**: 执行运动轨迹
- 验证轨迹
- 配置触发
- 逐段执行
- 监控状态
- 发布事件

**依赖端口**:
- IPositionControlPort
- ITriggerControllerPort
- IEventPublisherPort

### MoveToPositionUseCase (PTP)
**职责**: 点对点移动到指定位置
- 位置移动
- 状态验证
- 精度检查

### MotionInitializationUseCase
**职责**: 控制器连接与轴初始化

### MotionSafetyUseCase
**职责**: 急停与停止所有轴
**规范**: 急停通过 EmergencyStopService 触发；StopAllAxes 仅表示停止动作（立即/平滑），不替代急停流程

### MotionMonitoringUseCase
**职责**: 状态/位置与IO监控

### MotionCoordinationUseCase
**职责**: 坐标系与IO/串口协调控制

### ManualMotionControlUseCase
**职责**: 手动运动控制（点动/步进/手轮）
**依赖**: IPositionControlPort / JogController / IHomingPort
**规范**: 点动规则统一入口为 JogController，用例不重复实现校验与互锁判断

### EmergencyStopUseCase
**职责**: 紧急停止
- 停止所有运动
- 禁用触发
- 更新状态
- 记录日志
**规范**: 业务规则统一由 EmergencyStopService 承载，用例仅做编排与日志记录

### 测试专用用例（已迁移到 `tests/unit/usecases`）
以下 *TestUseCase 不再编译进应用层库，仅用于单元测试与验证。

## 使用示例

### 基本用法

```cpp
#include "usecases/system/InitializeSystemUseCase.h"

using namespace Siligen::Application::UseCases::System;

// 创建用例(通过依赖注入)
auto initialize_usecase = container->Resolve<InitializeSystemUseCase>();

// 准备请求
InitializeSystemRequest request;
request.load_configuration = true;
request.validate_configuration = true;
request.auto_connect_hardware = true;
request.start_heartbeat = true;
request.auto_home_axes = true;

// 执行用例
auto result = initialize_usecase->Execute(request);

if (result.IsSuccess()) {
    auto response = result.Value();
    std::cout << "系统初始化成功" << std::endl;
    std::cout << "是否回零成功: " << response.axes_homed << std::endl;
} else {
    std::cerr << "初始化失败: " << result.GetError().ToString() << std::endl;
}
```

### 组合多个用例

```cpp
// 1. 初始化系统
auto init_result = initialize_usecase->Execute(init_request);
if (init_result.IsError()) {
    return init_result.GetError();
}

// 2. 回零所有轴
HomeAxesRequest home_request;
home_request.home_all_axes = true;
auto home_result = home_axes_usecase->Execute(home_request);
if (home_result.IsError()) {
    return home_result.GetError();
}

// 3. 执行轨迹
ExecuteTrajectoryRequest traj_request;
traj_request.trajectory_segments = GenerateTrajectory(...);
auto traj_result = execute_trajectory_usecase->Execute(traj_request);

return traj_result;
```

### 错误处理

```cpp
auto result = usecase->Execute(request);

if (result.IsError()) {
    auto error = result.GetError();

    switch (error.GetCode()) {
        case ErrorCode::INVALID_STATE:
            // 处理状态错误
            ResetSystem();
            break;

        case ErrorCode::TIMEOUT:
            // 处理超时
            RetryOperation();
            break;

        case ErrorCode::HARDWARE_ERROR:
            // 处理硬件错误
            NotifyUser("硬件错误");
            EmergencyStop();
            break;

        default:
            // 记录未知错误
            LogError(error.ToString());
            break;
    }
}
```

## 创建新用例

### 步骤

1. **定义请求和响应结构**
```cpp
struct NewOperationRequest {
    // 输入参数
    int32 param1;
    std::string param2;

    bool Validate() const {
        // 验证逻辑
        return param1 > 0 && !param2.empty();
    }
};

struct NewOperationResponse {
    // 输出结果
    bool success;
    std::string message;
};
```

2. **创建用例类**
```cpp
class NewOperationUseCase {
public:
    explicit NewOperationUseCase(
        std::shared_ptr<ISomePort> some_port);

    Result<NewOperationResponse> Execute(
        const NewOperationRequest& request);

private:
    std::shared_ptr<ISomePort> some_port_;

    // 业务流程步骤方法
    Result<void> Step1();
    Result<void> Step2();
};
```

3. **实现业务流程**
```cpp
Result<NewOperationResponse> NewOperationUseCase::Execute(
    const NewOperationRequest& request) {

    NewOperationResponse response;

    // 1. 验证请求
    if (!request.Validate()) {
        return Result<NewOperationResponse>(
            Error(ErrorCode::INVALID_PARAMETER, "Invalid request"));
    }

    // 2. 执行业务逻辑
    auto step1_result = Step1();
    if (step1_result.IsError()) {
        return Result<NewOperationResponse>(step1_result.GetError());
    }

    auto step2_result = Step2();
    if (step2_result.IsError()) {
        return Result<NewOperationResponse>(step2_result.GetError());
    }

    // 3. 返回结果
    response.success = true;
    response.message = "Operation completed successfully";
    return Result<NewOperationResponse>(response);
}
```

## 用例实现规范

### 请求验证
- 所有请求参数必须验证
- 提供Validate()方法
- 返回清晰的错误消息

### 错误处理
- 使用Result<T>模式
- 传播领域层错误
- 添加用例上下文信息

### 业务流程
- 步骤清晰分离
- 每步可独立测试
- 支持事务回滚(如需要)

### 急停规则
- 急停统一通过 `EmergencyStopService` 触发与处理
- 用例不得直接调用 `IPositionControlPort::EmergencyStop` 或用 `StopAllAxes(true)`/`StopAxis(..., true)` 替代急停流程
- 立即停止/单轴停止仅用于非急停的保护或控制逻辑

### 日志记录
- 记录关键业务操作
- 记录错误和异常
- 包含足够的上下文信息

### 性能考虑
- 避免不必要的端口调用
- 批量操作优于循环调用
- 考虑超时和取消机制

## 测试策略

### 单元测试
```cpp
TEST(InitializeSystemUseCaseTest, SuccessfulInitialization) {
    // Arrange
    auto mock_config_port = std::make_shared<MockConfigurationPort>();
    auto mock_connection_port = std::make_shared<MockDeviceConnectionPort>();
    // ... 其他mock

    auto usecase = std::make_shared<InitializeSystemUseCase>(
        mock_config_port, mock_connection_port, ...);

    InitializeSystemRequest request;
    request.load_configuration = true;
    request.auto_connect_hardware = true;

    // Act
    auto result = usecase->Execute(request);

    // Assert
    EXPECT_TRUE(result.IsSuccess());
    EXPECT_TRUE(result.Value().config_loaded);
}
```

### 集成测试
- 使用真实端口实现
- 测试完整业务流程
- 验证状态转换正确性

## 相关文档

- 领域端口: `src/domain/<subdomain>/ports/`
- 基础设施适配器: `src/infrastructure/adapters/README.md`
- 六边形架构: `docs/architecture/hexagonal-architecture-refactoring-plan.md`
