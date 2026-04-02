# Configuration 子域 - 配置管理

**职责**: 保留 configuration implementation `.cpp` 与子域占位，不再承载 private compat header 或重复配置模型残余

## 业务范围

- 机器配置管理
- CMP 配置管理
- 插值配置管理
- 阀门时序配置管理
- 点胶配置管理
- 标定/校准参数配置（如脉冲-位移换算）
- 工艺结果判定相关阈值配置（如点径/流量目标）
- 配置验证
- 配置迁移（版本兼容）

## 目录结构

```
configuration/
├── DispensingConfig.cpp
├── MachineConfig.cpp
├── ValveTimingConfig.cpp
├── CMPPulseConfig.cpp
├── InterpolationConfig.cpp
└── ports/                      # private compat headers 已移除
```

## 命名空间

```cpp
namespace Siligen::Domain::Configuration {
    namespace ValueObjects { ... }
    namespace DomainServices { ... }
    namespace Ports { ... }
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

## 兼容约束

- `workflow/domain/include/domain/configuration/**` 已全部退役；live consumer 必须直接包含 canonical owner contracts。
- `workflow/domain/domain/configuration/**` 不再允许恢复 `IConfigurationPort`、`IFileStoragePort`、`ValveConfig`、`ConfigTypes` private wrapper。
