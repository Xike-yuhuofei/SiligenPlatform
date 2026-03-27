# System Use Cases

## 职责

系统级生命周期管理和紧急控制能力。

## 命名空间

```cpp
namespace Siligen::Application::UseCases::System
```

## Use Cases

### InitializeSystemUseCase
- **职责**: 初始化系统，加载/验证配置，建立硬件连接并可选启动心跳/监控
- **使用场景**: 系统启动时
- **依赖**: IConfigurationPort / DeviceConnectionPort / HomeAxesUseCase / IDiagnosticsPort / IEventPublisherPort(可选) / IHardLimitMonitor(可选)

### EmergencyStopUseCase
- **职责**: 紧急停止所有运动和点胶操作（规则由 EmergencyStopService 统一承载）
- **使用场景**: 异常情况下紧急制动
- **依赖**: MotionControlService / MotionStatusService / CMPService / IMachineExecutionStatePort / ILoggingService
- **规则入口**: EmergencyStopService

## 设计原则

1. 系统级 Use Cases 不应包含业务逻辑
2. 应该快速响应，避免长时间阻塞
3. 必须具备高可靠性和容错能力
4. 宿主后台监控通过 `IHardLimitMonitor` 等抽象接入，不直接依赖 runtime-host 具体实现
