# Motion Safety Use Cases

## 职责

急停、停止、报警清理等安全控制。

## 命名空间

```cpp
namespace Siligen::Application::UseCases::Motion::Safety
```

## Use Cases

### MotionSafetyUseCase
- **职责**: 急停与停止所有轴
- **使用场景**: 异常处理与安全停机
- **规范**: 急停必须通过 EmergencyStopService 触发；StopAllAxes 仅表示停止动作（立即/平滑），不替代急停流程
