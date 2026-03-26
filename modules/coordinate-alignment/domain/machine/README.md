# Machine 子域 - 设备管理

**职责**: 负责点胶机设备整体状态管理、任务编排、标定流程与硬件连接管理

## 业务范围

- 设备状态与运行模式管理（由 DispenserModel 统一维护）
- 状态流转（UNINITIALIZED → INITIALIZING → READY → DISPENSING → PAUSED → ERROR）
- 标定/校准流程管理
- 任务队列管理
- 任务生命周期控制
- 硬件连接管理
- 设备配置管理
- 统计信息收集

## 自动运行/运行模式规范

- 统一入口: `Aggregates::Legacy::DispenserModel` 为自动运行/运行模式唯一规则来源
- 应用层仅编排调用，不得实现或复制运行模式状态机/规则
- 新增状态/规则必须落在 DispenserModel（或其协作的领域服务）内

## 标定流程规范

- 统一入口: `DomainServices::CalibrationProcess` 为标定流程唯一规则来源
- 应用层仅编排调用，不得实现或复制标定状态/规则
- 设备与结果通过端口接入：`ICalibrationDevicePort` / `ICalibrationResultPort`

## 目录结构

```
machine/
├── aggregates/                # 聚合根
│   └── DispenserModel.*       # 点胶机聚合根
├── domain-services/           # 领域服务
│   └── CalibrationProcess.*   # 标定流程
├── value-objects/             # 值对象
│   └── CalibrationTypes.h
└── ports/                     # 端口接口
    ├── IHardwareConnectionPort
    ├── IHardwareTestPort
    ├── ICalibrationDevicePort
    └── ICalibrationResultPort
```

## 命名空间

```cpp
namespace Siligen::Domain::Machine {
    namespace Aggregates { ... }
    namespace ValueObjects { ... }
    namespace DomainServices { ... }
    namespace Ports { ... }
}
```

## 依赖关系

- ✅ 依赖: `shared/types`, `shared/utils`, `domain/_shared`
- ⚠️ 协调依赖: `domain/motion`, `domain/dispensing`（通过 Port 接口）
- ❌ 不依赖: `infrastructure`, `application`
