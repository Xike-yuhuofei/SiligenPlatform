# Motion Trajectory Use Cases

## 职责

轨迹规划和执行。

## 命名空间

```cpp
namespace Siligen::Application::UseCases::Motion::Trajectory
```

## Use Cases

### DeterministicPathExecutionUseCase
- **职责**: 以 `Start/Advance/Status/Cancel` 语义驱动非阻塞路径执行
- **使用场景**: deterministic tick 驱动的仿真/runtime bridge 路径执行
- **依赖**: `IInterpolationPort`、`IMotionStatePort`
- **说明**: 负责路径规划、插补程序生成以及逐 tick 段派发，不阻塞调用线程

### ExecuteTrajectoryUseCase
- **职责**: 执行完整的运动轨迹
- **使用场景**: DXF 点胶路径执行
- **依赖**: IInterpolationPort
- **说明**: 插补段由 Domain 统一生成与校验，IInterpolationPort 默认注入 `ValidatedInterpolationPort`

## 设计原则

1. 支持直线、圆弧、样条等插补类型（插补规则在 Domain）
2. 实时监控运动状态
3. 提供运动暂停和恢复功能
