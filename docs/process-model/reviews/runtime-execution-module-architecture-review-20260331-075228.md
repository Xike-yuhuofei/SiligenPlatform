# runtime-execution 模块架构审查

- 审查时间：2026-03-31 07:52:28
- 审查范围：`modules/runtime-execution/`，以及判断边界所需的少量关联文件
- 审查目标：职责、边界、依赖、落点、构建组织、结构性问题

## 1. 模块定位

- 核心职责：从声明性文档看，`modules/runtime-execution/` 是 `M9 runtime-execution` 的 canonical owner 根，owner artifact 是 `ExecutionSession`，应负责“消费已验证执行输入、驱动执行、收敛执行状态与失败归责”，以及执行域 runtime contracts、host core、设备联动桥接，见 `modules/runtime-execution/README.md`、`modules/runtime-execution/module.yaml`、`modules/runtime-execution/application/README.md`、`modules/runtime-execution/runtime/host/README.md`。
- 业务位置：它位于规划链之后、宿主应用之前，承接上游已经完成的规划/编排结果，向 `apps/runtime-service`、`apps/runtime-gateway`、`apps/planner-cli` 暴露运行期执行面；这一点在仓库根 `README.md`、模块 `README.md` 与 `modules/runtime-execution/README.md` 的描述是一致的。
- 主要输入：按声明应是“已验证执行输入”和运行期端口；按 2026-03-31 审查时的代码证据，可见输入实际包括点胶执行计划与覆盖参数、motion/runtime port、device connection port、配置端口、当时仍存在的 machine state adapter，见 `modules/runtime-execution/application/usecases/dispensing/DispensingExecutionUseCase.cpp`、`modules/runtime-execution/runtime/host/runtime/system/MachineExecutionStatePortAdapter.cpp`。
- 主要输出：执行结果、执行状态快照、事件发布、运行期 motion services、planning artifact 导出桥与设备动作下发，见 `modules/runtime-execution/runtime/host/runtime/motion/WorkflowMotionRuntimeServicesProvider.h`、`modules/runtime-execution/runtime/host/runtime/planning/PlanningArtifactExportPortAdapter.h`、`modules/runtime-execution/runtime/host/runtime/system/MachineExecutionStatePortAdapter.cpp`。
- 应当依赖谁：从 `module.yaml` 看，白名单只允许 `dispense-packaging/contracts` 与 `shared`；结合 `contracts/README.md`，它还应通过 `shared/contracts/device` 这类稳定契约消费设备能力，而不应直接依赖 `workflow` 内部 application/domain 实现。
- 谁应当依赖它：宿主应用与编排入口应依赖其 `application public`、`runtime contracts`、`host core`。`modules/runtime-execution/README.md` 直接写明 `apps/runtime-service`、`apps/planner-cli`、`apps/runtime-gateway` 消费 canonical runtime surfaces。至于其它业务 owner 模块是否应直接依赖其内部实现，当前证据不支持。

## 2. 声明边界 vs 实际实现

- 仓库文档里，这个模块被定义成 `ExecutionSession` 的 owner，范围已收紧为执行域和 host core，不再承担进程级 bootstrap、配置路径解析、recipe persistence wiring、storage、security 与 DXF/workflow/job-ingest 宿主装配，见 `modules/runtime-execution/README.md`、`modules/runtime-execution/runtime/host/README.md`。
- 实际代码里，这个模块确实承载了部分执行用例、host 运行期桥接与 device adapters，但执行核心 owner 并未完全收口在本模块内部。
- 两者不一致，偏差主要体现在四个方面。

### 2.1 该模块应有实现仍落在别处

- `DispensingExecutionUseCase.cpp` 直接包含 `domain/dispensing/domain-services/DispensingProcessService.h`，而该头实际位于 `modules/workflow/domain/domain/dispensing/domain-services/DispensingProcessService.h`。这说明执行主流程依赖的核心领域服务 owner 仍在 `workflow`。
- `MachineExecutionStatePortAdapter.cpp` 在 2026-03-31 审查时对应的 machine execution seam 仍直接围绕 `DispenserModel` 建模；这一点解释了当时为何判断执行态快照依赖的机器执行状态模型仍在 `workflow`。
- `modules/runtime-execution/domain/` 与 `modules/runtime-execution/services/` 目录只有 `README.md`，没有 live 实现；而根 `CMakeLists.txt` 的 `SILIGEN_TARGET_IMPLEMENTATION_ROOTS` 却把这两个目录列为模块实现根之一，说明“owner 骨架”已建立，但真实 owner 事实没有回收到这些目录。

### 2.2 别的模块 owner 事实被放进了本模块 public surface

- `contracts/runtime` 中多处只是旧 `workflow/domain` 端口别名而非 M9 自有契约：
  - `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/dispensing/ITaskSchedulerPort.h`
  - `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/system/IEventPublisherPort.h`
  - `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/configuration/IConfigurationPort.h`
- 这些文件表明 `runtime-execution/contracts` 的一部分 public face 仍是对旧 domain port 的转发壳，而非 `ExecutionSession` owner 专属语义模型。

### 2.3 该模块不是“目录存在但实现完整在内”，真实实现仍散落外部

- `MotionControlUseCase.cpp` 直接包含 `workflow/application` 下的 `HomeAxesUseCase`、`ManualMotionControlUseCase`、`MotionMonitoringUseCase`。
- `WorkflowMotionRuntimeServicesProvider.h` 直接实现 `workflow/application/services/motion/runtime/IMotionRuntimeServicesProvider.h`。
- `PlanningArtifactExportPortAdapter.h` 直接实现 `workflow/application/services/dispensing/IPlanningArtifactExportPort`。
- 这说明 `runtime-execution` 虽然建立了独立目录与 target，但真实执行控制面仍有相当部分挂靠在 `workflow` 的 use case 与 service 定义上。

### 2.4 存在重复建模、重复流程、重复数据定义

- `modules/runtime-execution/contracts/README.md` 明确写明 `shared/contracts/device/` 才是稳定设备契约 canonical root，`modules/runtime-execution/contracts/device/` 只保留兼容装配入口。
- 但 `modules/runtime-execution/contracts/device/include/siligen/device/contracts/**` 与 `shared/contracts/device/include/siligen/device/contracts/**` 仍保留一套完整重复头树。对 `device_contracts.h` 与 `device_ports.h` 的哈希比对显示两边文件内容完全一致。
- `modules/runtime-execution/adapters/device/include/siligen/device/adapters/dispensing/ValveAdapter.h`、`.../TriggerControllerAdapter.h`、`.../motion/MotionRuntimeFacade.h` 这些 public 头直接 `#include` 私有 `src/**` 头，说明 public surface 本身仍处于兼容壳状态，而不是稳定接口。

### 明确判断

- 仓库文档定义与实际实现不一致。
- 偏差不在局部命名或代码风格，而在 owner 事实没有真正收口，public contract 仍有大面积兼容壳，模块实现与 `workflow` 之间仍存在迁移期双向耦合。

## 3. 模块内部结构审查

- 内部结构：混乱。

### 原因

- 目录层次表面完整，但 live 实现集中在 `application/`、`runtime/host/`、`adapters/device/`；`domain/`、`services/` 只有说明文档，没有实现，owner 骨架与真实实现分布不一致。
- 公共接口与内部实现没有真正分离。`adapters/device` 的 public 头直接包含私有 `src` 头，导致 include surface 与实现边界重叠。
- 领域逻辑、应用逻辑、兼容桥接逻辑混放。`application/usecases/motion/MotionControlUseCase.cpp` 本应是 M9 公共用例面，但实际只是对 `workflow` 运动用例的封装壳；`runtime/host` 中的 provider/adapter 也多数是对 `workflow` application interface 或 domain model 的桥接。
- 构建入口没有反映真实模块边界。`modules/runtime-execution/CMakeLists.txt` 声称 `domain;services;application;adapters;runtime;tests;examples` 都是 implementation roots，但真实 build 只编 `application`、`adapters/device`、`runtime/host`。这会制造“结构已收口”的假象。

### 证据文件

- `modules/runtime-execution/CMakeLists.txt`
- `modules/runtime-execution/domain/README.md`
- `modules/runtime-execution/services/README.md`
- `modules/runtime-execution/application/CMakeLists.txt`
- `modules/runtime-execution/runtime/host/CMakeLists.txt`
- `modules/runtime-execution/adapters/device/CMakeLists.txt`
- `modules/runtime-execution/adapters/device/include/siligen/device/adapters/dispensing/ValveAdapter.h`
- `modules/runtime-execution/adapters/device/include/siligen/device/adapters/dispensing/TriggerControllerAdapter.h`
- `modules/runtime-execution/adapters/device/include/siligen/device/adapters/motion/MotionRuntimeFacade.h`

## 4. 对外依赖与被依赖关系

### 合理依赖

- `shared` 与 `shared/contracts/device`：执行模块需要消费稳定类型与设备契约，这与 `contracts/README.md` 和各 CMake target 的目标方向基本一致。
- `dispense-packaging/contracts`：作为执行前上游输出契约，`runtime-execution` 依赖其 contract 是合理的，见 `modules/runtime-execution/module.yaml` 与 `modules/runtime-execution/application/CMakeLists.txt`。

### 可疑依赖

- `process-planning/contracts` 的 `siligen_process_planning_legacy_configuration_bridge_public`：`application/CMakeLists.txt` 与 `adapters/device/CMakeLists.txt` 都引入了 legacy configuration bridge，但 `application/README.md` 又强调 M9 只接受已规划执行输入。该依赖更像迁移期遗留，不像稳定 owner 依赖。
- `modules/runtime-execution/contracts/device/`：文档声明它不再是 canonical owner，但构建图仍把它作为模块内子目录加进来。这会维持重复契约面。

### 高风险依赖

- `M9 -> M0 workflow` 直接依赖：
  - `modules/runtime-execution/application/usecases/motion/MotionControlUseCase.cpp`
  - `modules/runtime-execution/runtime/host/runtime/motion/WorkflowMotionRuntimeServicesProvider.h`
  - `modules/runtime-execution/runtime/host/runtime/planning/PlanningArtifactExportPortAdapter.h`
  - `modules/runtime-execution/runtime/host/runtime/system/MachineExecutionStatePortAdapter.cpp`
- `runtime/host/CMakeLists.txt` 直接把 `modules/workflow/application/include` 加入 `PUBLIC` include root。
- `workflow/application/CMakeLists.txt` 同时又把 `modules/runtime-execution/application/include` 放进 public include，并链接 `siligen_runtime_execution_application_public`。
- 这已经不是单向编排依赖，而是 `workflow` 与 `runtime-execution` 之间的双向耦合。

### 证据文件

- `modules/runtime-execution/application/CMakeLists.txt`
- `modules/runtime-execution/runtime/host/CMakeLists.txt`
- `modules/runtime-execution/adapters/device/CMakeLists.txt`
- `modules/workflow/application/CMakeLists.txt`
- `modules/runtime-execution/application/usecases/motion/MotionControlUseCase.cpp`
- `modules/runtime-execution/runtime/host/runtime/motion/WorkflowMotionRuntimeServicesProvider.h`
- `modules/runtime-execution/runtime/host/runtime/planning/PlanningArtifactExportPortAdapter.h`
- `modules/runtime-execution/runtime/host/runtime/system/MachineExecutionStatePortAdapter.cpp`

### 工程后果

- `ExecutionSession` 无法形成独立 owner 面，M9 的 public API 变化会被 `workflow` 直接牵动。
- `runtime-execution` 内部目录无法安全重排，因为 public header 与外部模块实现细节已经相互渗透。
- 构建上显式 CMake 环依赖证据不足，但 include graph 和 target graph 已形成明显反向依赖风险。
- 迁移会长期停滞在“兼容壳可编译，但 owner 不独立”的中间态。

## 5. 结构性问题清单

### 1. `ExecutionSession` owner 未真正落在 `M9`

- 现象：`runtime-execution` 宣称 owner artifact 是 `ExecutionSession`，但执行主流程服务 `DispensingProcessService` 和机器执行状态模型 `DispenserModel` 仍由 `workflow` 提供。
- 涉及文件：
  - `modules/runtime-execution/module.yaml`
  - `modules/runtime-execution/application/usecases/dispensing/DispensingExecutionUseCase.cpp`
  - `modules/runtime-execution/runtime/host/runtime/system/MachineExecutionStatePortAdapter.cpp`
  - `modules/workflow/domain/domain/dispensing/domain-services/DispensingProcessService.h`
  - `modules/workflow/domain/domain/machine/aggregates/DispenserModel.cpp`
- 为什么这是结构问题而不是局部实现问题：缺的不是一个函数或类，而是模块 owner 核心事实仍在外部模块，导致 `runtime-execution` 不是执行域真正的边界。
- 可能后果：执行域语义无法稳定版本化，迁移无法完成，任何执行相关变更都需要同步触碰 `workflow`。
- 优先级：P0

### 2. `M9 runtime-execution` 与 `M0 workflow` 已形成双向耦合

- 现象：`runtime-execution` 直接依赖 `workflow` 的 motion usecase、planning export port、runtime services provider interface 与 machine aggregate；同时 `workflow/application` 又公开依赖 `runtime-execution/application`。
- 涉及文件：
  - `modules/runtime-execution/application/usecases/motion/MotionControlUseCase.cpp`
  - `modules/runtime-execution/runtime/host/runtime/motion/WorkflowMotionRuntimeServicesProvider.h`
  - `modules/runtime-execution/runtime/host/runtime/planning/PlanningArtifactExportPortAdapter.h`
  - `modules/runtime-execution/runtime/host/CMakeLists.txt`
  - `modules/workflow/application/CMakeLists.txt`
  - `modules/workflow/application/include/application/usecases/motion/homing/HomeAxesUseCase.h`
  - `modules/workflow/application/include/application/usecases/motion/manual/ManualMotionControlUseCase.h`
  - `modules/workflow/application/include/application/usecases/motion/monitoring/MotionMonitoringUseCase.h`
- 为什么这是结构问题而不是局部实现问题：这是模块图方向错误，不是单个 include 写错。只要这种互相引用存在，两个 owner 模块就不能独立演进。
- 可能后果：构建顺序与 public API 演化被锁死，后续再拆边界时成本指数增长。
- 优先级：P0

### 3. public surface 仍大量是兼容壳与重复面

- 现象：runtime contracts 中多个头只是旧 domain port 的 `using` 转发；`contracts/device` 与 `shared/contracts/device` 同时保留完整重复树；device adapter 的 public 头直接转 private `src`。
- 涉及文件：
  - `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/dispensing/ITaskSchedulerPort.h`
  - `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/system/IEventPublisherPort.h`
  - `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/configuration/IConfigurationPort.h`
  - `modules/runtime-execution/contracts/README.md`
  - `modules/runtime-execution/contracts/device/include/siligen/device/contracts/device_contracts.h`
  - `shared/contracts/device/include/siligen/device/contracts/device_contracts.h`
  - `modules/runtime-execution/adapters/device/include/siligen/device/adapters/dispensing/ValveAdapter.h`
  - `modules/runtime-execution/adapters/device/include/siligen/device/adapters/dispensing/TriggerControllerAdapter.h`
  - `modules/runtime-execution/adapters/device/include/siligen/device/adapters/motion/MotionRuntimeFacade.h`
- 为什么这是结构问题而不是局部实现问题：public surface 是模块边界本身。边界如果仍是兼容壳和重复树，就无法成为长期稳定契约。
- 可能后果：消费者无法判断 canonical 入口，重复修改、误依赖和 ABI/include 破坏风险持续存在。
- 优先级：P1

### 4. 文档、目录骨架、构建边界长期失配

- 现象：`runtime-execution/README.md` 与 `runtime/host/README.md` 明确说 bootstrap/config/security/storage 已迁到 `apps/runtime-service`；但 `modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md` 仍将 `M9 runtime-execution` 统计为 `164` 项、`79` 个实现文件，并继续把大块 host/device 实现作为模块主实现面呈现。与此同时，`domain/`、`services/` 只有 README，没有 live 实现。
- 涉及文件：
  - `modules/runtime-execution/README.md`
  - `modules/runtime-execution/runtime/host/README.md`
  - `modules/runtime-execution/CMakeLists.txt`
  - `modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md`
  - `modules/runtime-execution/domain/README.md`
  - `modules/runtime-execution/services/README.md`
- 为什么这是结构问题而不是局部实现问题：这会让文档、目录、构建三套“模块边界”各说各话，团队无法基于统一边界做持续迁移。
- 可能后果：评审标准失效，后续 owner 收口和代码迁移缺少可执行基线。
- 优先级：P1

## 6. 模块结论

- 宣称职责：运行期执行控制、执行链驱动与设备联动；owner artifact 为 `ExecutionSession`。
- 实际职责：部分执行用例、host 运行期桥接、device adapters，以及大量面向 `workflow` 的兼容桥与转发壳。
- 是否职责单一：否。
- 是否边界清晰：否。
- 是否被侵入：是。`workflow` 的 domain/application 事实仍直接侵入该模块实现。
- 是否侵入别人：是。该模块又以 host/application public surface 反向依赖 `workflow` 内部接口，形成跨边界直连。
- 是否适合作为稳定业务模块继续演进：否，至少在 owner 收口与反向依赖切断前不适合。
- 最终评级：高风险。

## 7. 修复顺序

### 第一步：先收口 owner 事实与 contract

- 目标：明确 `DispensingProcessService`、`DispenserModel`、执行参数校验语义、机器执行状态口到底归谁；把真正属于执行域的 owner 事实迁回 `modules/runtime-execution/domain/`、`modules/runtime-execution/services/` 或其明确替代目录。
- 涉及目录/文件：
  - `modules/runtime-execution/domain/`
  - `modules/runtime-execution/services/`
  - `modules/runtime-execution/application/usecases/dispensing/DispensingExecutionUseCase*.cpp`
  - `modules/runtime-execution/runtime/host/runtime/system/MachineExecutionStatePortAdapter.*`
  - `modules/workflow/domain/domain/dispensing/domain-services/DispensingProcessService.*`
  - `modules/workflow/domain/domain/machine/aggregates/DispenserModel.*`
- 收益：先恢复 `M9` 的 owner 身份，再谈后续依赖整理，避免继续围绕兼容壳修补。
- 风险：现有 `workflow` 与宿主代码可能同时依赖这些类型，迁移过程中需要临时兼容层。

### 第二步：再切断 `M9 -> M0` 反向依赖

- 目标：把 `MotionControlUseCase`、`WorkflowMotionRuntimeServicesProvider`、`PlanningArtifactExportPortAdapter` 等桥接点改为依赖 M9 自有抽象或上游稳定 contract，不再直接引用 `workflow/application` 实现类型。
- 涉及目录/文件：
  - `modules/runtime-execution/application/usecases/motion/MotionControlUseCase.cpp`
  - `modules/runtime-execution/runtime/host/runtime/motion/WorkflowMotionRuntimeServicesProvider.*`
  - `modules/runtime-execution/runtime/host/runtime/planning/PlanningArtifactExportPortAdapter.*`
  - `modules/runtime-execution/runtime/host/CMakeLists.txt`
  - `modules/workflow/application/CMakeLists.txt`
- 收益：恢复依赖方向，降低 include graph 和 public target graph 的环风险。
- 风险：会暴露出当前 `workflow` public surface 中对 M9 的隐式假设，需要同步梳理编排入口。

### 第三步：最后统一 public surface、目录与文档

- 目标：删除或收拢 `contracts/device` 重复树，禁止 public 头包含私有 `src`，清理空骨架或补齐真实实现，并同步修正文档与 `module.yaml`、模块文件树文档。
- 涉及目录/文件：
  - `modules/runtime-execution/contracts/device/`
  - `shared/contracts/device/`
  - `modules/runtime-execution/adapters/device/include/`
  - `modules/runtime-execution/README.md`
  - `modules/runtime-execution/runtime/host/README.md`
  - `modules/runtime-execution/module.yaml`
  - `modules/runtime-execution/CMakeLists.txt`
  - `modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md`
- 收益：消费者只看到一个 canonical 入口，文档、目录、构建边界重新一致，后续演进才有稳定基线。
- 风险：include 路径和构建 target 会发生变化，需要安排过渡期并补最小回归验证。

## 8. 证据索引

| 文件路径 | 得出的判断 | 支撑结论 |
|---|---|---|
| `modules/runtime-execution/README.md` | M9 宣称只 owner 执行域与 host core，bootstrap/config/security/storage 已迁出 | 模块定位；声明边界与实际偏差；文档边界失配 |
| `modules/runtime-execution/module.yaml` | owner artifact 是 `ExecutionSession`，白名单依赖仅列 `dispense-packaging/contracts`、`shared` | 模块定位；职责单一性判断；依赖偏差 |
| `modules/runtime-execution/application/README.md` | 明确 M9 application 应只消费已规划执行输入，禁止重新做 planning/DXF 准备 | 声明边界；可疑 legacy 依赖判断 |
| `modules/runtime-execution/runtime/host/README.md` | host core 应只保留执行域最小桥接，旧 bootstrap 只剩 forwarder 兼容壳 | 模块定位；声明边界；结构失配 |
| `modules/runtime-execution/CMakeLists.txt` | 根 target 声称 `domain/services/tests/examples` 也属于 implementation roots | 内部结构混乱；目录骨架与构建不一致 |
| `modules/runtime-execution/domain/README.md` | `domain/` 是 owner 事实应收敛目录，但当前无实现 | owner 未收口；内部结构混乱 |
| `modules/runtime-execution/services/README.md` | `services/` 是模块级编排应收敛目录，但当前无实现 | owner 未收口；内部结构混乱 |
| `modules/runtime-execution/application/CMakeLists.txt` | M9 application 实际链接 `process-planning` legacy bridge、`siligen_domain` 等，并编译执行 usecase | 实际职责；可疑依赖；构建边界判断 |
| `modules/runtime-execution/runtime/host/CMakeLists.txt` | host 直接暴露 `modules/workflow/application/include`，并编译 workflow bridge adapter | 高风险依赖；双向耦合 |
| `modules/runtime-execution/adapters/device/CMakeLists.txt` | device adapter 同时承载 fake/real/diagnostics/homing/runtime/legacy bridge，并依赖 legacy configuration bridge | 内部结构混放；可疑依赖 |
| `modules/runtime-execution/application/usecases/dispensing/DispensingExecutionUseCase.cpp` | 执行用例直接依赖 `workflow` 的 `DispensingProcessService` | `ExecutionSession` owner 未落在 M9 |
| `modules/runtime-execution/application/usecases/motion/MotionControlUseCase.cpp` | M9 public usecase 实质封装 `workflow` motion usecases | M9 -> M0 反向依赖；真实实现散落外部 |
| `modules/runtime-execution/runtime/host/runtime/motion/WorkflowMotionRuntimeServicesProvider.h` | M9 host 直接实现 `workflow` runtime service provider interface | M9 -> M0 反向依赖 |
| `modules/runtime-execution/runtime/host/runtime/system/MachineExecutionStatePortAdapter.cpp` | 执行态适配器直接绑定 `workflow` 的 `DispenserModel` | owner 未收口；边界被侵入 |
| `modules/runtime-execution/runtime/host/runtime/planning/PlanningArtifactExportPortAdapter.h` | planning artifact export 口仍使用 `workflow/application` 服务接口 | M9 -> M0 反向依赖 |
| `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/dispensing/ITaskSchedulerPort.h` | runtime contract 只是旧 domain port `using` 别名 | public surface 兼容壳化 |
| `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/system/IEventPublisherPort.h` | runtime contract 只是旧 domain port `using` 别名 | public surface 兼容壳化 |
| `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/configuration/IConfigurationPort.h` | runtime contract 直接转发旧配置 port | public surface 兼容壳化 |
| `modules/runtime-execution/contracts/device/include/siligen/device/contracts/device_contracts.h` | 与 `shared/contracts/device` 保留重复契约文件树 | 重复建模；边界不清 |
| `shared/contracts/device/include/siligen/device/contracts/device_contracts.h` | 文档宣称这里才是 canonical root，但仓库仍保留模块内副本 | 重复建模；canonical root 不唯一 |
| `modules/runtime-execution/adapters/device/include/siligen/device/adapters/dispensing/ValveAdapter.h` | public 头直接包含私有 `src` 头 | 公共接口与内部实现未分离 |
| `modules/runtime-execution/adapters/device/include/siligen/device/adapters/dispensing/TriggerControllerAdapter.h` | public 头直接包含私有 `src` 头 | 公共接口与内部实现未分离 |
| `modules/runtime-execution/adapters/device/include/siligen/device/adapters/motion/MotionRuntimeFacade.h` | public 头直接包含私有 `src` 头 | 公共接口与内部实现未分离 |
| `modules/workflow/application/CMakeLists.txt` | `workflow/application` 公开包含 `runtime-execution/application/include` 并链接 `siligen_runtime_execution_application_public` | 与 M9 构成双向耦合 |
| `modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md` | 文档统计仍把 M9 作为大体量 live 实现模块呈现，与 README 收紧边界不一致 | 文档、目录、构建边界失配 |

