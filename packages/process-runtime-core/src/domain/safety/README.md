# Safety 子域 - 安全监控

**职责**: 负责安全规则验证、限位检测、安全互锁监控、急停处理

## 业务范围

- 软限位验证
- 硬限位监控
- 安全互锁条件检查
- 急停处理（紧急停止）
- 安全规则引擎
- 安全事件记录

## 目录结构

```
safety/
└── domain-services/         # 领域服务
    ├── EmergencyStopService # 急停规则统一入口
    ├── SoftLimitValidator   # 软限位验证（现有）
    └── InterlockPolicy      # 互锁判定策略
└── ports/                   # 端口
    └── IInterlockSignalPort # 互锁信号端口
└── value-objects/           # 值对象
    └── InterlockTypes       # 互锁信号/判定类型
```

## 命名空间

```cpp
namespace Siligen::Domain::Safety {
    namespace ValueObjects { ... }
    namespace DomainServices { ... }
    namespace Ports { ... }
}
```

## 安全规则

所有运动操作执行前必须通过安全验证:
1. 软限位检查（位置是否在有效范围）
2. 硬限位检查（限位开关状态）
3. 安全互锁条件检查（依赖设备状态）
4. 急停状态检查

## 互锁统一规范

- `motion-core` 是互锁规则的主实现与规则来源
- `InterlockPolicy` 保留为兼容包装层，其他层优先通过 bridge 或模块接口调用 `motion-core`
- 互锁信号由基础设施采集，通过 `IInterlockSignalPort` 提供给 Domain
- 应用层监控服务仅做采集/转发/执行动作，不重复判定

## 急停统一规范

- `EmergencyStopService` 是急停规则唯一入口
- 应用层仅触发与记录结果，不得自行实现急停状态更新/清理流程
- 立即停止（`StopAllAxes(true)` / `StopAxis(..., true)`）不等同急停流程

## 依赖关系

- ✅ 依赖: `shared/types`, `shared/utils`, `domain/_shared`
- ❌ 不依赖: `infrastructure`, `application`, 其他子域
