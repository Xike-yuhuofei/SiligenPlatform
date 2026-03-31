# Workflow 模块架构审查

- 审查范围：`modules/workflow/`
- 审查时间：`2026-03-31 07:46:57`
- 工作流上下文：`refactor/workflow/ARCH-202-preview-compat-drain`
- `branchSafe`：`refactor-workflow-ARCH-202-preview-compat-drain`
- `ticket`：`ARCH-202`

## 1. 模块定位

- 从声明性文档看，`workflow` 的核心职责应是 `WorkflowRun` 的阶段推进、`M0 -> 规划链` 触发边界、失败归类与恢复决策，见 `modules/workflow/README.md`、`modules/workflow/module.yaml`。
- 从实现看，它位于宿主应用与规划/执行链之间，但实际还承载了系统初始化、运动控制、回零、点动、配方命令、冗余治理、点胶预览/执行编排等入口，见 `modules/workflow/application/CMakeLists.txt`。
- 主要输入实际包括 `PlanningRequest`、上传/计划请求、设备连接配置、运行时端口、配方仓储端口等，见 `modules/workflow/application/include/application/usecases/dispensing/PlanningUseCase.h`、`modules/workflow/application/include/application/usecases/system/EmergencyStopUseCase.h`。
- 主要输出实际包括 `WorkflowContracts`、规划结果/执行包、系统初始化结果、配方发布/归档结果、预览确认状态等，见 `modules/workflow/contracts/include/workflow/contracts/WorkflowContracts.h`、`modules/workflow/application/include/application/usecases/dispensing/DispensingWorkflowUseCase.h`。
- 如果按模块角色设计，`workflow` 应依赖 `shared`、少量跨模块契约，以及下游 owner 模块的 facade/port；真正依赖它的应主要是 `apps/` 宿主和 `runtime-execution` 这类实现端口的模块，而不是其它 owner 模块的内部构建。

## 2. 声明边界 vs 实际实现

- 仓库文档把它定义为“只承载 M0 编排职责，不直接承载 M4-M8 事实实现”，且 `allowed_dependencies` 只写了 `shared` 与 `*/contracts`，见 `modules/workflow/README.md`、`modules/workflow/module.yaml`。
- 实际代码里，模块内文档明确把 `motion`、`machine`、`safety`、`diagnostics`、`configuration`、`recipes`、`planning` 都定义成 `workflow/domain` 的子域，并把回零、点动、插补、标定、急停、工艺结果、配方生效等能力归到这里，见 `modules/workflow/domain/domain/README.md`。
- 实际构建也按这个更宽的职责在编译：`siligen_application_system`、`siligen_application_motion`、`siligen_application_dispensing`、`siligen_application_redundancy`，以及 `siligen_motion_execution_services`、`siligen_recipe_domain_services`、`siligen_diagnostics_domain_services`、`siligen_safety_domain_services`，见 `modules/workflow/application/CMakeLists.txt`、`modules/workflow/domain/domain/CMakeLists.txt`。
- 两者不一致。偏差不是少量历史残留，而是 owner 事实仍实质性留在 `workflow`：`JogController`、`CalibrationProcess`、`RecipeActivationService` 都是实实现，见 `modules/workflow/domain/domain/motion/domain-services/JogController.cpp`、`modules/workflow/domain/domain/machine/domain-services/CalibrationProcess.cpp`、`modules/workflow/domain/domain/recipes/domain-services/RecipeActivationService.cpp`。
- 不存在“目录存在但实现都在外部”的情况；相反，`workflow` 仍是大量真实实现承载面。同时，它的部分 public surface 又只是对外部模块的再导出或兼容 shim，例如 `PlanningUseCase.h` 直接包含 `process-path`、`motion-planning`、`dispense-packaging` 的 application facade，见 `modules/workflow/application/include/application/usecases/dispensing/PlanningUseCase.h`。
- 重复建模/重复流程已出现。最直接证据是 `RecipeActivationService` 内部做新旧 recipe 模型双向转换，见 `modules/workflow/domain/domain/recipes/domain-services/RecipeActivationService.cpp`；`workflow/domain/domain/CMakeLists.txt` 还明确写着因为 `HomeAxesResponse` 存在 `workflow-only fields`，运行时必须继续使用 workflow 自己的 motion 服务，见 `modules/workflow/domain/domain/CMakeLists.txt`。

### 明确判断

- 是否存在该模块应有实现却落在别处：有，但主要是编排辅助接口被外部 owner 模块承接，例如 `DxfPbPreparationService` 实际转发到 `dxf-geometry`，`IPlanningArtifactExportPort` 的请求/结果类型来自 `dispense-packaging`，见 `modules/workflow/application/include/application/services/dxf/DxfPbPreparationService.h`、`modules/dispense-packaging/application/include/application/services/dispensing/PlanningArtifactExportPort.h`。这说明 `workflow` 的外部边界模型没有完全自有化。
- 是否存在别的模块 owner 事实被放进该模块：有，且量大，集中在 `motion`、`machine`、`recipes`、`safety`、`diagnostics`。
- 是否存在该模块只是“目录存在”，但真实实现散落在外部：否。实际更接近反方向，即 `workflow` 本身仍然是多类实现的容器。
- 是否存在与其它模块重复建模、重复流程、重复数据定义：有，尤其是配方激活的 legacy/new 双模型转换，以及 `workflow-only fields` 导致的 motion 兼容实现分叉。

## 3. 模块内部结构审查

- 明确判断：混乱。

### 原因

- 目录骨架表面清楚，但 live 实现、兼容桥、历史 `process-core/motion-core`、public wrapper 同时共存；`domain/` 下既有 `domain/domain/**`，又有 `process-core/`、`motion-core/`，并且统一暴露进 `siligen_domain`，见 `modules/workflow/domain/CMakeLists.txt`。
- 公共接口与内部实现没有真正分离。`workflow` 的 public header 直接暴露外部模块 application facade、`runtime-execution` usecase 和 implementation shim，见 `modules/workflow/application/include/application/usecases/dispensing/PlanningUseCase.h`、`modules/workflow/application/include/application/usecases/dispensing/DispensingWorkflowUseCase.h`、`modules/workflow/domain/include/domain/motion/domain-services/MotionControlServiceImpl.h`、`modules/workflow/domain/include/domain/motion/domain-services/MotionStatusServiceImpl.h`。
- 构建入口没有反映“模块边界已收口”的状态，反而仍以 `siligen_process_runtime_core` 和 `siligen_domain` 这种跨模块聚合 target 为主，见 `modules/workflow/CMakeLists.txt`。
- 测试仍围绕 `process-runtime-core` 兼容 target 组织，而不是围绕最小的 `workflow` 公共边界组织，见 `modules/workflow/tests/process-runtime-core/CMakeLists.txt`。

### 证据文件

- `modules/workflow/CMakeLists.txt`
- `modules/workflow/domain/CMakeLists.txt`
- `modules/workflow/domain/domain/CMakeLists.txt`
- `modules/workflow/application/CMakeLists.txt`
- `modules/workflow/tests/process-runtime-core/CMakeLists.txt`

## 4. 对外依赖与被依赖关系

### 合理依赖

- `shared` 类型、工具、结果模型。
- `workflow/contracts` 自有契约。
- `PlanningUseCase` 对 `process-path`、`motion-planning`、`dispense-packaging` facade 的编排依赖。这部分符合“工作流协调者”角色，见 `modules/workflow/application/usecases/dispensing/PlanningUseCase.cpp`。

### 可疑依赖

- `workflow` 的 public header 直接依赖 `dxf-geometry`、`runtime-execution/contracts`、其它模块 application facade，而不是通过专门的 orchestration contract 收口，见 `modules/workflow/application/include/application/services/dxf/DxfPbPreparationService.h`、`modules/workflow/application/include/application/usecases/system/EmergencyStopUseCase.h`、`modules/workflow/application/include/application/usecases/dispensing/PlanningUseCase.h`。

### 高风险依赖

- `workflow` 的 public surface 直接暴露 `runtime-execution` 的 application usecase 和 implementation shim，见 `modules/workflow/application/include/application/usecases/dispensing/DispensingWorkflowUseCase.h`、`modules/workflow/domain/include/domain/motion/domain-services/MotionControlServiceImpl.h`、`modules/workflow/domain/include/domain/motion/domain-services/MotionStatusServiceImpl.h`。
- 根构建把 `workflow` 头目录全局注入，并强制要求它提供 `siligen_domain`，见根 `CMakeLists.txt`。
- `runtime-service` 直接 `add_subdirectory` 到 `modules/workflow/adapters/.../dxf` 内部路径，见 `apps/runtime-service/CMakeLists.txt`。
- `runtime-host` 直接硬编码 `modules/workflow/application/include`，见 `modules/runtime-execution/runtime/host/CMakeLists.txt`。
- `dispense-packaging` 和 `process-path` 反向依赖 `siligen_process_runtime_core_boost_headers` 这类由 `workflow` 定义的通用 helper target，见 `modules/dispense-packaging/domain/dispensing/CMakeLists.txt`、`modules/process-path/domain/trajectory/CMakeLists.txt`。

### 工程后果

- public API 变化会跨模块级联。
- `workflow` 内部目录无法安全重排。
- 显式 CMake 环依赖证据不足，但 header/public target 已形成明显的隐性环风险和构建顺序耦合。

## 5. 结构性问题清单

### 1. `M0` 职责失真

- 现象：声明只做编排，实际却承载 `motion/machine/safety/diagnostics/recipes` 等 owner 事实与用例。
- 涉及文件：`modules/workflow/README.md`、`modules/workflow/domain/domain/README.md`、`modules/workflow/application/CMakeLists.txt`
- 为什么这是结构问题而不是局部实现问题：它改变的是模块 owner 定义和依赖图，不是单个类写法。
- 可能后果：owner 永远迁不干净，`workflow` 持续膨胀。
- 优先级：P0

### 2. 兼容桥变成 live 构建脊柱

- 现象：`siligen_process_runtime_core`、`siligen_domain` 仍是根构建和测试的核心聚合，且 owner roots 直接跨多个模块。
- 涉及文件：`modules/workflow/CMakeLists.txt`、根 `CMakeLists.txt`、`modules/workflow/tests/process-runtime-core/CMakeLists.txt`
- 为什么这是结构问题而不是局部实现问题：它决定了全仓库如何认识 `workflow`，会持续放大耦合。
- 可能后果：deprecated target 无法退场，长期演进只能继续围绕旧脊柱修补。
- 优先级：P1

### 3. public surface 直接暴露外部模块实现

- 现象：公开头文件直接包含 sibling application facade、`runtime-execution` usecase 和 implementation shim。
- 涉及文件：`modules/workflow/application/include/application/usecases/dispensing/PlanningUseCase.h`、`modules/workflow/application/include/application/usecases/dispensing/DispensingWorkflowUseCase.h`、`modules/workflow/domain/include/domain/motion/domain-services/MotionControlServiceImpl.h`
- 为什么这是结构问题而不是局部实现问题：它让模块边界失去独立编译和独立演进能力。
- 可能后果：外部模块轻微改动即可破坏 workflow API。
- 优先级：P1

### 4. 外部模块直接消费 workflow 内部路径与 helper target

- 现象：宿主和 owner 模块绕过 canonical public target，直接引用 workflow 内部 adapter/include/helper。
- 涉及文件：`apps/runtime-service/CMakeLists.txt`、`modules/runtime-execution/runtime/host/CMakeLists.txt`、`modules/dispense-packaging/domain/dispensing/CMakeLists.txt`、`modules/process-path/domain/trajectory/CMakeLists.txt`
- 为什么这是结构问题而不是局部实现问题：它锁死目录落点和构建顺序，直接阻断模块收口。
- 可能后果：任何内部整理都会波及宿主和 sibling module。
- 优先级：P1

## 6. 模块结论

- 宣称职责：工作流编排、阶段推进、`M0` 触发边界、恢复决策。
- 实际职责：编排 + 运动/系统/配方/冗余入口 + 多个领域子域规则 + 兼容桥/聚合构建枢纽。
- 是否职责单一：否。
- 是否边界清晰：否。
- 是否被侵入：是，主要是构建层和 public include 层被外部直接使用；业务 owner 事实被外部实现的证据不足。
- 是否侵入别人：是，明显承载了本应属于 `motion`、`recipe`、`machine`、`diagnostics`、部分 `planning/runtime` 的事实或兼容责任。
- 是否适合作为稳定业务模块继续演进：不适合；在未收口前更像迁移中间层，而不是稳定业务模块。
- 最终评级：明显偏移

## 7. 修复顺序

### 第一步：先收口 public surface

- 目标：让 `workflow` 只公开 `M0` 契约与最小 orchestration API。
- 涉及目录/文件：`modules/workflow/CMakeLists.txt`、`modules/workflow/application/include/`、`modules/workflow/domain/include/`、根 `CMakeLists.txt`
- 收益：先把“模块是什么”说清楚。
- 风险：会立即暴露现有宿主和 sibling module 的编译依赖。

### 第二步：再迁移非 `M0` owner 实现

- 目标：把 `motion/machine/recipes/safety/diagnostics` 的 live 规则和兼容 shim 迁回各自 owner，只在 workflow 保留 orchestration port/facade。
- 涉及目录/文件：`modules/workflow/domain/domain/`、`modules/workflow/application/usecases/`、`modules/workflow/domain/domain/dispensing/CMakeLists.txt`
- 收益：owner 与目录落点一致。
- 风险：需要阶段性双写/适配，测试要跟着拆分。

### 第三步：最后统一构建与验证口径

- 目标：去掉全局 `include_directories`、停止外部直连 workflow 内部路径、让测试从 `siligen_process_runtime_core` 切到真实 owner/public target。
- 涉及目录/文件：`apps/runtime-service/CMakeLists.txt`、`modules/runtime-execution/runtime/host/CMakeLists.txt`、`modules/workflow/tests/process-runtime-core/CMakeLists.txt`
- 收益：后续迁移成本下降。
- 风险：构建脚本和 CI 门禁会有一轮集中调整。

## 8. 证据索引

| 文件路径 | 得出的判断 | 支撑结论 |
|---|---|---|
| `modules/workflow/README.md` | 把 `workflow` 定义为纯编排；并声明 `process_runtime_core` 仅兼容保留 | 宣称职责、声明边界 |
| `modules/workflow/module.yaml` | `allowed_dependencies` 仅 `shared` 与 `*/contracts` | 声明依赖方向 |
| `modules/workflow/domain/domain/README.md` | 把 `motion/machine/safety/diagnostics/recipes` 都纳入 workflow domain，并定义统一规则来源 | 实际职责远超编排 |
| `modules/workflow/application/CMakeLists.txt` | 编译 `system/motion/dispensing/redundancy` 多类用例，并把多个 sibling include/target 暴露给 workflow application headers | 应用层职责膨胀、跨模块耦合 |
| `modules/workflow/domain/CMakeLists.txt` | 把 `process-path/motion-planning/dispense-packaging` 拉入 `siligen_domain`，并暴露 `process-core/motion-core` include | 领域层不是自洽 owner，而是聚合枢纽 |
| `modules/workflow/domain/domain/CMakeLists.txt` | 保留 compat planning target 和 workflow-owned motion execution services，且存在 `workflow-only fields` 注释 | 兼容桥仍是 live 实现、模型未统一 |
| `modules/workflow/application/include/application/usecases/dispensing/PlanningUseCase.h` | 直接包含 `process-path/motion-planning/dispense-packaging/dxf` 相关 service header | public surface 非自包含 |
| `modules/workflow/application/include/application/usecases/dispensing/DispensingWorkflowUseCase.h` | 直接依赖 `runtime_execution` 的 `DispensingExecutionUseCase` | workflow 对 runtime-execution 的高风险直连 |
| `modules/workflow/domain/include/domain/motion/domain-services/MotionControlServiceImpl.h` | workflow domain public header 直接 alias `runtime-execution` application implementation | 边界反转、契约未收口 |
| `apps/runtime-service/CMakeLists.txt` | 直接 `add_subdirectory` workflow 内部 dxf adapter 路径 | 外部模块侵入 workflow 构建边界 |
| `modules/runtime-execution/runtime/host/CMakeLists.txt` | 直接硬编码 workflow application include 路径 | runtime-execution 对 workflow 内部路径有编译期耦合 |
| `modules/dispense-packaging/domain/dispensing/CMakeLists.txt` | 反向依赖 workflow domain include 与 workflow 定义的 boost helper target | `M8` owner 仍不能摆脱 workflow |
| `modules/process-path/domain/trajectory/CMakeLists.txt` | 依赖 `siligen_process_runtime_core_boost_headers` | workflow helper target 外溢到其它 owner 模块 |
| `modules/workflow/tests/process-runtime-core/CMakeLists.txt` | 测试以 `siligen_process_runtime_core` 为前提，并链接多个 sibling public target | 验证口径仍围绕 legacy 聚合骨架 |

