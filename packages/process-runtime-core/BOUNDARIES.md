# process-runtime-core 边界说明

## 1. owner 与当前物理承接

`packages/process-runtime-core` 是业务内核的唯一 canonical owner。当前构建已切到 package 内源码，但为了先完成真实构建和测试切换，仍采用 package-local compatibility mirror 承接已编译内容：

- `src/domain`
- `src/application`
- `modules/process-core`
- `modules/motion-core`

| 子域 | 当前主目标 | 当前 package 内承接目录 |
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

顶层 `process/`、`planning/`、`execution/`、`machine/`、`safety/`、`recipes/`、`dispensing/`、`diagnostics-domain/`、`application/` 继续用于表达子域 owner 和长期物理落点。新代码只能进入本 package 对应承接目录，不得再回写 `control-core/src/domain`、`control-core/src/application`、`control-core/modules/process-core`、`control-core/modules/motion-core`。

## 2. 依赖限制

允许依赖：

- `packages/device-contracts`
- `packages/application-contracts`
- `packages/engineering-data` 的离线产物、schema 和兼容基线
- `packages/shared-kernel` 中稳定、通用、非业务化的基础类型
- `control-core/src` 下仅用于 `shared/*` 的兼容入口

禁止依赖：

- `packages/runtime-host`
- `packages/transport-gateway`
- `packages/device-adapters`
- `control-core/src/infrastructure/runtime/*`
- `control-core/src/adapters/tcp/*`
- `control-core/modules/device-hal/*`
- 新增任何直达 `control-core/src/domain/*`、`control-core/src/application/*`、`control-core/modules/process-core/*`、`control-core/modules/motion-core/*` 的 include 或 link 依赖

## 3. 子域间规则

- `application` 只能编排子域能力和端口，不承接 runtime 装配、TCP 会话、设备实现。
- `execution` 可以依赖 `planning`、`process`、`safety`，不能反向把宿主线程、任务调度实现卷回 core。
- `dispensing`、`recipes`、`diagnostics-domain` 不得直接依赖传输协议 DTO。
- `machine` 只表达设备能力抽象和校准/模型规则，不直接依赖硬件驱动实现。
- `shared-kernel` 只保留小而稳的通用基础，不新增业务拼盘对象。

## 4. 公开 API

稳定公开的 CMake target：

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

迁移期兼容 target：

- `siligen_control_application`
- `siligen_domain_services`
- `siligen_process_core`
- `siligen_motion_core`

兼容 target 仅用于旧调用点过渡，不允许新增消费者。

## 5. 当前收口方式

当前版本先以 `packages/process-runtime-core` 作为 canonical owner，并通过 package 侧 target、tests、include 顺序和模块注册完成真实切换。后续物理迁移应继续把源码收敛到本包内部，而不是扩大 legacy mirror 的生命周期。
