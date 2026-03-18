# control-core

Siligen 主控工程子项目，当前主要承载 TCP/runtime 入口、基础设施、shared 兼容层和硬件适配能力。

## 当前迁移状态

- 已从 `D:\Projects\Backend_CPP` 首轮迁入核心源码、构建脚本、测试、文档和第三方依赖。
- `src/adapters/hmi` 已按边界拆出，不再属于本项目。
- `packages/process-runtime-core` 已接管 `src/domain`、`src/application`、`modules/process-core`、`modules/motion-core` 的默认构建 owner。

## 目录说明

- `apps/control-runtime`：runtime 兼容入口壳；真实宿主实现已迁到根级 `packages/runtime-host`。
- `apps/control-tcp-server`：当前 TCP 可执行程序薄入口。
- `modules/control-gateway`：legacy 兼容壳；真实 TCP 传输实现已迁到根级 `packages/transport-gateway`。
- `modules/device-hal`：硬件适配器、MultiCard 驱动与厂商 SDK 主实现。
- `src/shared`：共享基础能力与兼容头入口。
- `src/domain` / `src/application`：legacy mirror；默认构建 owner 已切到 `packages/process-runtime-core`，不再接受新增真实依赖。
- `src/infrastructure`：当前仍承担配置、存储、工厂、解析适配等基础设施实现。
- `tests`：基础设施、集成与兼容测试入口；核心内核单测 canonical 位置已切到 `packages/process-runtime-core/tests`。

## 常用命令

```powershell
./scripts/build.ps1
./scripts/test.ps1
```
