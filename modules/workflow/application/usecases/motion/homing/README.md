# Motion Homing Use Cases

## 职责

轴回零和原点设定。

## 命名空间

```cpp
namespace Siligen::Application::UseCases::Motion::Homing
```

## Use Cases

### HomeAxesUseCase
- **职责**: 执行多轴回零操作（使用已加载配置）
- **使用场景**: 系统初始化，建立坐标系
- **依赖**: IHomingPort / IConfigurationPort / IMotionConnectionPort / IEventPublisherPort

## 设计原则

1. 支持顺序回零和并行回零
2. 提供回零状态监控
3. 处理回零失败情况
