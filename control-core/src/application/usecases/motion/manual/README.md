# Motion Manual Use Cases

## 职责

手动/操作者驱动的运动控制交互（点动、步进、手轮等）。

## 命名空间

```cpp
namespace Siligen::Application::UseCases::Motion::Manual
```

## Use Cases

### ManualMotionControlUseCase
- **职责**: 处理手动运动控制请求（点动、步进、手轮）
- **使用场景**: 操作者通过 HMI 或其他基于 TCP 的操作前端进行手动运动控制
- **依赖**: IPositionControlPort / JogController / IHomingPort

## 设计原则

1. 快速响应操作者指令
2. 提供实时位置反馈
3. 支持多种运动模式切换
4. 点动规则统一由 JogController 处理，用例不重复校验或互锁判断
