# Configuration 子域 - 配置管理

**职责**: 负责系统配置实现、参数验证，并承载标定/工艺结果相关配置；对外 configuration consumer contract 已外提到 `modules/process-planning/contracts/`

## 业务范围

- 机器配置管理
- CMP 配置管理
- 插值配置管理
- 阀门时序配置管理
- 点胶配置管理
- 标定/校准参数配置（如脉冲-位移换算）
- 工艺结果判定相关阈值配置（如点径/流量目标）
- 配置验证

## 目录结构

```
configuration/
├── DispensingConfig.cpp
├── MachineConfig.cpp
├── ValveTimingConfig.cpp
├── CMPPulseConfig.cpp
├── InterpolationConfig.cpp
├── value-objects/
│   └── ConfigTypes.h
```

## 命名空间

```cpp
namespace Siligen::Domain::Configuration {
    namespace ValueObjects { ... }
    namespace DomainServices { ... }
    namespace Ports { ... }  // configuration owner ports only
}
```

## 配置验证原则

所有配置在应用前必须通过验证:
1. 参数范围检查
2. 业务规则验证
3. 依赖关系检查
4. 完整性验证

## 依赖关系

- ✅ 依赖: `shared/types`, `shared/utils`, `domain/_shared`
- ❌ 不依赖: `infrastructure`, `application`, 其他子域

## 非本目录 owner

- upload file storage seam 不再由 `process-planning` 持有。
- 运行时 host-local storage wiring 当前固定落在 `apps/runtime-service/include/runtime_process_bootstrap/storage/ports/IFileStoragePort.h`。
