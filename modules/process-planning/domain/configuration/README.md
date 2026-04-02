# Configuration 子域 - 配置管理

**职责**: 负责 configuration implementation、参数验证、配置迁移，并承载标定/工艺结果相关配置；public header owner 已切到 `process-planning/contracts/configuration/**`

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
├── *.cpp                         # implementation owner
├── services/                    # no public wrapper remains
├── value-objects/               # no public wrapper remains
└── ports/                       # no public wrapper remains
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

## 统一规则入口

- live configuration value-object consumer 必须经 `process_planning/contracts/configuration/ConfigTypes.h` 消费配置模型，不得继续直接依赖 workflow residual `domain/configuration/value-objects/ConfigTypes.h`。
- canonical `ConfigTypes.h` 已自持 `Position3D` 与 `InterlockConfig`；consumer 不得再通过 `M4` 配置契约隐式带入 `motion-planning` / `workflow` owner 类型。
- `process-planning/domain/configuration/{ports,services,value-objects}/*.h` 已全部退役；禁止在该目录重新长出新的 public owner 头定义。
- `ReadyZeroSpeedResolver` 是 ready-zero / locate velocity 兼容解析的唯一配置 helper。
- runtime / workflow consumer 必须经 `process_planning/contracts/configuration/ReadyZeroSpeedResolver.h` 消费该规则，不得回流到任何 workflow configuration wrapper。
- file-storage contract 已迁出到 `job_ingest/contracts/storage/IFileStoragePort.h`，`process-planning/domain/configuration` 不再保留该 wrapper。

## 依赖关系

- ✅ 依赖: `shared/types`, `shared/utils`, `domain/_shared`
- ❌ 不依赖: `infrastructure`, `application`, 其他子域
