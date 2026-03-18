# Motion Initialization Use Cases

## 职责

硬件连接、轴使能、控制器复位等初始化流程。

## 命名空间

```cpp
namespace Siligen::Application::UseCases::Motion::Initialization
```

## Use Cases

### MotionInitializationUseCase
- **职责**: 连接/断开控制器、轴使能与复位
- **使用场景**: 系统启动、人工重连、维护操作
