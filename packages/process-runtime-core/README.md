# process-runtime-core

`process-runtime-core` 是 `D:\Projects\SiligenSuite` 中业务内核的 canonical owner。默认构建、默认核心测试和公开 target 已切到本 package；`control-core/src/domain`、`control-core/src/application`、`control-core/modules/process-core`、`control-core/modules/motion-core` 不再是默认构建 owner。

## 当前中间态

本次切换优先完成真实构建和真实测试切换，因此当前采用 package 内的 compatibility mirror 承接已编译源码：

- `src/domain`
- `src/application`
- `modules/process-core`
- `modules/motion-core`

顶层 `process/`、`planning/`、`execution/`、`machine/`、`safety/`、`recipes/`、`dispensing/`、`diagnostics-domain/`、`application/` 继续表达包内子域边界和长期落点；在 include 清理完成前，实际编译源码仍主要位于上面的 mirror 目录。禁止新增任何 `../../control-core/...` 真实依赖。

各子域职责、依赖限制和禁止依赖见 [BOUNDARIES.md](./BOUNDARIES.md)。

## 子域与当前承接目录

| 逻辑子域 | 公开 target | 当前 package 内承接目录 |
|---|---|---|
| `process/` | `siligen_process_runtime_core_process` | `modules/process-core`、`src/domain/configuration` |
| `planning/` | `siligen_process_runtime_core_planning` | `src/domain/trajectory`、`src/domain/motion` 中规划算法 |
| `execution/` | `siligen_process_runtime_core_execution` | `src/domain/motion/domain-services`、`src/domain/dispensing/domain-services` |
| `machine/` | `siligen_process_runtime_core_machine` | `src/domain/machine` |
| `safety/` | `siligen_process_runtime_core_safety` | `src/domain/safety`、`modules/motion-core` |
| `recipes/` | `siligen_process_runtime_core_recipes` | `src/domain/recipes`、`src/application/usecases/recipes` |
| `dispensing/` | `siligen_process_runtime_core_dispensing` | `src/domain/dispensing`、`src/application/usecases/dispensing/valve` |
| `diagnostics-domain/` | `siligen_process_runtime_core_diagnostics_domain` | `src/domain/diagnostics` |
| `application/` | `siligen_process_runtime_core_application` | `src/application` |

## 公开 API

推荐依赖以下 CMake target：

- `siligen_process_runtime_core`
- `siligen_process_runtime_core_process`
- `siligen_process_runtime_core_planning`
- `siligen_process_runtime_core_execution`
- `siligen_process_runtime_core_machine`
- `siligen_process_runtime_core_safety`
- `siligen_process_runtime_core_recipes`
- `siligen_process_runtime_core_dispensing`
- `siligen_process_runtime_core_diagnostics_domain`
- `siligen_process_runtime_core_application`

迁移期兼容 target `siligen_control_application`、`siligen_domain_services`、`siligen_process_core`、`siligen_motion_core` 仍保留给旧调用点，但不允许新增消费者。

## 依赖约束

- package 外消费者优先 link `siligen_process_runtime_core*`，不要直接 include package 内部私有目录。
- 允许继续通过 `control-core/src` 获取 `shared/*` 兼容头，但不得重新指向 `control-core/src/domain`、`control-core/src/application`、`control-core/modules/process-core`、`control-core/modules/motion-core`。
- runtime 装配、生命周期、任务调度宿主能力继续归 `packages/runtime-host`。
- TCP 协议、会话与网关能力继续归 `packages/transport-gateway` 和 `control-core/src/adapters/tcp`。
- 设备实现、厂商 SDK 和驱动适配继续归 `modules/device-hal` 及相关 device package。

## 测试入口

- 物理测试源码位于 `packages/process-runtime-core/tests`
- `siligen_unit_tests` 与 `siligen_pr1_tests` 由 package 侧 `tests/CMakeLists.txt` 定义
- `control-core/tests/CMakeLists.txt` 仅保留兼容转发入口
- package 侧测试说明见 `tests/README.md`
