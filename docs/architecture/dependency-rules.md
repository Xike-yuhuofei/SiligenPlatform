# 依赖规则

## 1. 目的

本文件定义根级 monorepo 的目标依赖方向。当前阶段不重写全部构建文件，但后续新增内容必须遵守这些规则。

## 2. 目标依赖方向

```text
apps/hmi-app
    -> packages/application-contracts
    -> packages/transport-gateway

apps/control-tcp-server
    -> packages/transport-gateway
    -> packages/runtime-host

apps/control-cli
    -> packages/application-contracts
    -> packages/runtime-host

apps/control-runtime
    -> packages/runtime-host

packages/runtime-host
    -> packages/process-runtime-core
    -> packages/device-adapters
    -> packages/traceability-observability
    -> packages/shared-kernel

packages/process-runtime-core
    -> packages/engineering-data
    -> packages/device-contracts
    -> packages/application-contracts
    -> packages/shared-kernel

packages/device-adapters
    -> packages/device-contracts
    -> packages/shared-kernel

packages/transport-gateway
    -> packages/application-contracts
    -> packages/shared-kernel

packages/engineering-data
    -> packages/engineering-contracts
    -> packages/shared-kernel

packages/simulation-engine
    -> packages/process-runtime-core
    -> packages/device-contracts
    -> packages/engineering-contracts
    -> packages/engineering-data
    -> packages/shared-kernel
```

## 3. 明确禁止的依赖

| 禁止依赖 | 原因 |
|---|---|
| `apps/hmi-app -> packages/process-runtime-core` 内部实现 | HMI 只能通过应用契约和传输边界访问核心 |
| `apps/hmi-app -> packages/device-adapters` | UI 不直接依赖硬件适配 |
| `apps/control-tcp-server -> control-core/modules/control-gateway/src/*` | app 只能通过 `packages/transport-gateway` 的公开入口做 wiring |
| `packages/transport-gateway -> packages/process-runtime-core` 私有对象 | 传输边界只能依赖公开应用契约或 facade 映射 |
| `packages/device-adapters -> packages/process-runtime-core` | 设备层只能实现 device contracts，不反向依赖内核实现 |
| `packages/device-adapters -> control-core/modules/*` | 设备真实适配实现必须留在 package，不再从 legacy 模块偷 include/link |
| `packages/engineering-data -> packages/process-runtime-core` | 离线工程数据不能依赖运行时内核 |
| `packages/runtime-host -> 业务规则实现` | runtime-host 只做装配、配置、宿主生命周期、安全和后台任务，不承接业务规则 |
| `packages/application-contracts -> 任意实现包` | 契约包必须保持中立 |
| `packages/device-contracts -> 任意实现包` | 同上 |
| `packages/device-contracts -> control-core/modules/*` | 纯契约包不得依赖 legacy 实现目录 |
| `packages/engineering-contracts -> 任意实现包` | 同上 |
| `packages/shared-kernel -> 任意业务包` | 共享内核只能向下稳定，不能被业务反向污染 |
| `packages/shared-kernel -> control-core/modules/*` | canonical shared-kernel 已是 owner，不允许反向 include legacy 模块 |

## 4. 当前阶段执行口径

由于现有真实实现还在旧子项目中，当前阶段采用以下折中规则：

1. 可以继续修改旧子项目中的现有实现。
2. 但新增跨边界能力时，必须先按根级 canonical 目标选择 owner。
3. 任何新增文档、脚本、schema、测试基线，不再默认丢进旧子项目根。
4. 若短期必须保留 legacy 兼容壳，必须在新 canonical 目录里留下说明。
5. `apps/control-runtime` 当前只能保留薄入口与兼容 target，不再新增宿主实现。
6. `apps/control-tcp-server` 当前只能保留启动与 wiring，不再新增协议解析、会话和消息分发实现。

## 5. 迁移期依赖警戒线

以下情况出现时，应先停下补治理，而不是继续堆实现：

- 新功能需要同时改三个以上旧子项目且没有明确契约 owner
- 设备适配开始继续吸入 recipe、diagnostics logging、query model
- `device-contracts` / `device-adapters` / `shared-kernel` 的 CMake 或源码重新出现 `control-core/modules/*`
- HMI 需要直接引用 `control-core` 内部模型而不是协议 DTO
- `dxf-pipeline` 同时承担算法、schema、generated code、回归基线且没有独立 contracts owner

## 6. Wave 1 shared target boundary（目标态，不代表已迁移）

本节只定义 Wave 1 的目标态边界，不改变前文的 current canonical 事实。

```text
shared/contracts/application
    <- packages/application-contracts

shared/contracts/device
    <- packages/device-contracts

shared/contracts/engineering
    <- packages/engineering-contracts

shared/kernel
    <- packages/shared-kernel

shared/testing
    <- packages/test-kit

shared/logging
    <- packages/shared-kernel
    <- packages/traceability-observability
```

Wave 1 仅允许新增以下类型的内容：

- 边界说明
- 命名冻结
- 门禁说明
- 迁移占位

Wave 1 不允许把以下内容视为已迁移：

- `packages/*` 实现源码
- `fixtures/tests` 进入 `shared/contracts/*`
- 场景 gate 脚本进入 `shared/testing`
- logging sink 或追溯业务实现进入 `shared/logging`
