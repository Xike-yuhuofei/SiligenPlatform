# Shared Layer (共享接口层)

## 目录

本层包含系统的共享接口定义、通用类型和工厂类,为所有其他层提供契约定义。

### interfaces/
端口接口定义,包括:
- `IHardwareController.h` - 硬件控制器接口
- `ILoggingService.h` - 日志服务接口

### types/
共享类型定义,包括:
- `Result.h` - 统一结果类型
- `Error.h` - 错误类型
- `Point2D.h` - 二维坐标点
- `AxisStatus.h` - 轴状态类型
- `CMPTypes.h` - CMP触发类型(统一定义)
- `LogTypes.h` - 日志类型
- `ConfigTypes.h` - 配置类型
- `DomainEvent.h` - 领域事件基类

### logging/
日志宏定义,包括:
- `LoggingMacros.h` - 统一的日志宏定义(SILIGEN_LOG_*)

## 依赖原则

**共享层不依赖任何其他层**。所有其他层都可以依赖共享层。

## 命名空间

```cpp
namespace Siligen::Shared {
    namespace Interfaces { }  // 接口定义
    namespace Types { }       // 类型定义
}
```
