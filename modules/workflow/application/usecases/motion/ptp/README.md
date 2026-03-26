# Motion PTP Use Cases

## 职责

点对点（PTP）运动控制。

## 命名空间

```cpp
namespace Siligen::Application::UseCases::Motion::PTP
```

## Use Cases

### MoveToPositionUseCase
- **职责**: 移动到指定位置
- **使用场景**: 点对点运动
- **依赖**: MotionControlService / MotionStatusService / MotionValidationService
