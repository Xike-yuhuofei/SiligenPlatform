# Valve Coordination Use Cases

## 职责

阀门协同控制。

## 命名空间

```cpp
namespace Siligen::Application::UseCases::Dispensing::Valve::Coordination
```

## Use Cases

### ValveCoordinationUseCase
- **职责**: 协调供料阀和点胶阀的联动
- **使用场景**: 确保供料和出胶时序正确
- **依赖**: Supply 和 Dispenser Use Cases

## 设计原则

1. 实现阀门状态同步
2. 处理阀门间的时序关系
3. 提供协同失败恢复机制
