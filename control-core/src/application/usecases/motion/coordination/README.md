# Motion Coordination Use Cases

## 职责

坐标系、插补段管理、速度覆盖与位置触发等协调控制。
插补参数校验与规则由 Domain 统一入口负责，用例只做编排与端口调用。

## 命名空间

```cpp
namespace Siligen::Application::UseCases::Motion::Coordination
```

## Use Cases

### MotionCoordinationUseCase
- **职责**: 坐标系配置、插补段管理、IO/比较输出与串口配置
- **使用场景**: 运动与外设的协同控制
