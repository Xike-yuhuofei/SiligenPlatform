# `dispense-packaging` 模块架构审查

- 审查时间：`2026-03-31 07:48:40`（Asia/Shanghai）
- 审查对象：`modules/dispense-packaging/`
- 审查范围：以 `modules/dispense-packaging/` 为主，仅查看判断边界所必需的少量 `modules/workflow/`、`modules/runtime-execution/`、`apps/runtime-service/`、根级文档与 CMake
- 审查方法：先读声明性边界，再读实现，再核对少量上下游调用与构建关系

## 1. 模块定位

- 从文档与代码综合判断，`modules/dispense-packaging/` 被宣称为 `ExecutionPackage` / `DispenseTimingPlan` owner，位于 `M6 process-path`、`M7 motion-planning` 之后，`M9 runtime-execution` 之前，负责把上游路径与运动结果收敛成可执行载荷与预览事实。
  证据：`modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md:77`、`modules/dispense-packaging/README.md:7`
- 主要输入在代码里表现为 `ProcessPath`、`MotionPlan`、触发/补偿/速度/点距等参数；主要输出是 `ExecutionPackageValidated`、authority layout、预览轨迹/胶点与导出请求。
  证据：`modules/dispense-packaging/application/include/application/services/dispensing/DispensePlanningFacade.h:37`、`modules/dispense-packaging/contracts/include/domain/dispensing/contracts/ExecutionPackage.h:18`
- 如果按“点胶执行数据打包与下游执行载荷组织”这个预期角色收口，它应主要依赖 `process-path` / `motion-planning` 的稳定契约、自身 domain、以及 `shared`；它应被 `workflow` 规划编排层依赖，并被 `runtime-execution` 仅以 `ExecutionPackage` 契约消费。
- 现状不是这样。实际代码显示它还承载了触发规划、authority layout 规划、预览几何下采样、运行时点胶流程、阀门/供胶稳压、部分 CMP/路径优化等职责，已经超出“打包模块”范畴。
  证据：`modules/dispense-packaging/domain/dispensing/README.md:3`、`modules/dispense-packaging/domain/dispensing/CMakeLists.txt:16`

## 2. 声明边界 vs 实际实现

- 仓库文档把它定义成 packaging / validation owner，并明确写了“`domain/dispensing/` 当前只保留 packaging/validation 相关 domain 实现，不再编译 planner/optimizer 实现”。
  证据：`modules/dispense-packaging/README.md:8`、`modules/dispense-packaging/README.md:31`
- 但同一模块内的 `domain/dispensing/README.md` 又把它定义成“点胶过程与工艺逻辑、触发控制、阀门协调、胶路建压等高层业务流程”，并把 `DispensingProcessService`、`DispensingController`、`SupplyStabilizationPolicy`、`PathOptimizationStrategy` 都列为 owner 入口。
  证据：`modules/dispense-packaging/domain/dispensing/README.md:3`、`modules/dispense-packaging/domain/dispensing/README.md:32`
- 实际构建也支持后者而不是前者。`domain/dispensing/CMakeLists.txt` 明确编译了 `PathOptimizationStrategy.cpp`、`TriggerPlanner.cpp`、`DispensingController.cpp`、`DispensingProcessService.cpp`，同时还编译 `AuthorityTriggerLayoutPlanner.cpp` 与 `CurveFlatteningService.cpp`。
  证据：`modules/dispense-packaging/domain/dispensing/CMakeLists.txt:16`
- 应有实现确实散落在别处。`workflow/domain/domain/CMakeLists.txt` 仍然编译 `siligen_dispensing_execution_services` 和 `siligen_workflow_dispensing_planning_compat`，对应 `CMPTriggerService`、`PurgeDispenserProcess`、`SupplyStabilizationPolicy`、`ValveCoordinationService`、`DispensingPlannerService` 等点胶 owner 代码。
  证据：`modules/workflow/domain/domain/CMakeLists.txt:121`、`modules/workflow/domain/domain/CMakeLists.txt:145`
- 还有“目录存在但真实实现未收口”的情况。`modules/dispense-packaging/domain/dispensing/planning/domain-services/DispensingPlannerService.cpp` 存在且包含完整规划逻辑，但 M8 自己的 CMake 不编译它；相反，`workflow` 仍在编译自己的版本。
  证据：`modules/dispense-packaging/domain/dispensing/planning/domain-services/DispensingPlannerService.cpp:1718`、`modules/dispense-packaging/domain/dispensing/CMakeLists.txt:16`、`modules/workflow/domain/domain/CMakeLists.txt:148`
- 存在重复建模与重复实现。`workflow` 下仍保留 `TriggerPlan`、`DispensingPlan` 的本地定义，同时 `DispensingExecutionTypes.h` 又转发到 M8；这不是统一 owner，而是迁移中间态固化。
  证据：`modules/workflow/domain/include/domain/dispensing/value-objects/DispensingExecutionTypes.h:3`、`modules/workflow/domain/domain/dispensing/value-objects/TriggerPlan.h:13`、`modules/workflow/domain/domain/dispensing/value-objects/DispensingPlan.h:30`
- 结论：声明边界与实际实现不一致，而且不是轻微偏差，是模块内文档、构建和跨模块 owner 现实三者同时失配。

### 重点检查结论

- 是否存在该模块应有实现却落在别处：存在。`workflow/domain` 仍承载一套 dispensing execution / planning compat。
- 是否存在别的模块的 owner 事实被放进该模块：存在。运行时执行、阀门协同、供胶稳压被放进 M8。
- 是否存在该模块只是“目录存在”，但真实实现散落在外部：存在。`DispensingPlannerService.cpp` 在 M8 目录内存在但不由 M8 编译，workflow 仍保留另一份 live 版本。
- 是否存在与其它模块重复建模、重复流程、重复数据定义：存在。`TriggerPlan`、`DispensingPlan`、多份 dispensing planning/execution 代码并存。

## 3. 模块内部结构审查

- 内部结构判断：混乱。

### 原因

- 目录表面上分成 `contracts / application / domain`，但 `application` 里的 `DispensePlanningFacade` 实际承载了大量规划/绑定/导出组装逻辑，不只是薄协调层。
  证据：`modules/dispense-packaging/application/include/application/services/dispensing/DispensePlanningFacade.h:37`、`modules/dispense-packaging/application/services/dispensing/DispensePlanningFacade.cpp:1305`
- `domain` 同时混放 packaging、authority layout 规划、运行时执行流程、阀门协同、供胶稳压、路径优化，层次不是“单一业务子域”，而是“点胶相关都进来”。
  证据：`modules/dispense-packaging/domain/dispensing/README.md:32`、`modules/dispense-packaging/domain/dispensing/CMakeLists.txt:16`
- 目录内容与构建内容不一致。`CMPTriggerService.cpp` 是“Minimal stub for compilation”，`PositionTriggerController` 还是直接面向 `MultiCard` 的遗留硬件控制器，但它们又与 live owner 文件同层并存。
  证据：`modules/dispense-packaging/domain/dispensing/domain-services/CMPTriggerService.cpp:1`、`modules/dispense-packaging/domain/dispensing/domain-services/PositionTriggerController.h:17`
- 测试所有权没有随 owner 一起迁移。M8 自测只覆盖 `TriggerPlanner`、`DispensingController`、`AuthorityTriggerLayoutPlanner`、`DispensePlanningFacade`、`PreviewSnapshotService`；`DispensingProcessService`、`PurgeDispenserProcess`、`ValveCoordinationService` 的测试仍在 `workflow/tests`。
  证据：`modules/dispense-packaging/tests/CMakeLists.txt:3`、`modules/workflow/tests/process-runtime-core/CMakeLists.txt:34`

### 明确判断

- 目录层次：不清晰
- 公共接口与内部实现是否分离：分离不彻底。`contracts` 明确，但 `application` 和 `domain` 的 live 职责边界不清。
- 是否存在混杂层次：存在
- 是否存在领域逻辑、应用逻辑、适配器逻辑混放：存在
- 是否存在“工具类堆积”掩盖真实职责：存在。`CurveFlatteningService`、`PathArcLengthLocator`、`PathOptimizationStrategy`、预览折线生成逻辑都在为不同层的关注点服务。
- CMake / 构建入口是否反映真实模块边界：不反映

## 4. 对外依赖与被依赖关系

### 合理依赖

- M8 application 依赖 `process-path/contracts`、`motion-planning/contracts` 来接收上游结果并组包，这与它在链路中的位置相符。
  证据：`modules/dispense-packaging/application/CMakeLists.txt:3`、`modules/dispense-packaging/application/include/application/services/dispensing/DispensePlanningFacade.h:37`
- `workflow` 用 `DispensePlanningFacade` 组装 `ExecutionPackageValidated`，`runtime-execution` 用 `ExecutionPackageValidated` 执行，这条主链本身合理。
  证据：`modules/workflow/application/include/application/usecases/dispensing/PlanningUseCase.h:3`、`modules/workflow/application/usecases/dispensing/PlanningUseCase.cpp:630`、`modules/runtime-execution/application/include/runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h:56`

### 可疑依赖

- `module.yaml` 允许的依赖只有 `motion-planning/contracts`、`process-planning/contracts`、`shared`，但 application CMake 明确引入了 `process-path/contracts`；声明与实现不一致。
  证据：`modules/dispense-packaging/module.yaml:6`、`modules/dispense-packaging/application/CMakeLists.txt:3`
- M8 contract 通过 `ExecutionPackage.h -> DispensingExecutionTypes.h` 把 `MachineMode`、`IInterpolationPort` 这类外部运行时类型卷进来了，契约边界不够纯。
  证据：`modules/dispense-packaging/contracts/include/domain/dispensing/contracts/ExecutionPackage.h:3`、`modules/dispense-packaging/domain/dispensing/value-objects/DispensingExecutionTypes.h:7`

### 高风险依赖

- M8 domain target 公开导出了 `SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR`，同时其 public header 直接依赖 `domain/configuration`、`domain/motion`、`domain/machine`、`siligen/device`。这是 M8 对 workflow/runtime 头文件的反向物理依赖。
  证据：`modules/dispense-packaging/domain/dispensing/CMakeLists.txt:38`、`modules/dispense-packaging/domain/dispensing/domain-services/DispensingProcessService.h:3`、`modules/dispense-packaging/domain/dispensing/value-objects/DispensingExecutionTypes.h:7`
- `runtime-execution` 文档要求其 API 只接受已规划执行输入，但实现里仍直接 `#include` 并构造 `DispensingProcessService` concrete；结合 `application/CMakeLists.txt` 的私有 include 上下文，当前证据更支持该 concrete 仍通过 `workflow/process_runtime_core` 旧聚合路径解析，而不是只通过契约消费 M8 结果，也不能据此直接认定为“明确绑定 M8 concrete”。
  证据：`modules/runtime-execution/application/README.md:9`、`modules/runtime-execution/application/usecases/dispensing/DispensingExecutionUseCase.cpp:5`、`modules/runtime-execution/application/usecases/dispensing/DispensingExecutionUseCase.cpp:458`、`modules/runtime-execution/application/CMakeLists.txt:45-82`、`modules/workflow/CMakeLists.txt:53-70`
- `workflow/domain` 一边把 M8 加进来，一边继续编译自己的 dispensing bridge 实现，形成双向演进耦合。显式 target 环依赖证据不足，但 source/include 级双向耦合证据充分。
  证据：`modules/workflow/domain/CMakeLists.txt:71`、`modules/workflow/domain/domain/CMakeLists.txt:121`

### 工程后果

- owner 无法唯一判定
- include 路径命中与实现命中存在迁移期歧义
- 运行时执行与执行包组装无法独立演进
- 任何一侧改动都需要跨模块同步排查

## 5. 结构性问题清单

### 1. M8 owner 实现被拆成两套并长期并存

- 现象：`workflow/domain` 仍编译 dispensing execution / planning compat；`workflow` 下还保留 `TriggerPlan`、`DispensingPlan` 本地定义；其中 `TriggerPlanner.cpp`、`DispensingController.cpp`、`TriggerPlan.h`、`DispensingPlan.h` 与 M8 对应文件本地哈希一致，`DispensingProcessService.cpp` 则已分叉。具体 SHA256 值见文末附录。
- 涉及文件：
  - `modules/workflow/domain/CMakeLists.txt:71`
  - `modules/workflow/domain/domain/CMakeLists.txt:121`
  - `modules/workflow/domain/domain/dispensing/value-objects/TriggerPlan.h:13`
  - `modules/workflow/domain/domain/dispensing/value-objects/DispensingPlan.h:30`
- 为什么这是结构问题而不是局部实现问题：owner 唯一性被破坏，任何功能演进都要同时判断 M8 / workflow 哪一份才是真实现。
- 可能后果：重复修复、行为漂移、include 命中不确定、迁移永远收不了口。
- 优先级：P0

### 2. 模块角色从“执行包打包”漂移成“点胶全域服务集合”

- 现象：顶层 README 宣称 packaging/validation owner，但 domain README 和 domain CMake 同时承载 `DispensingProcessService`、`DispensingController`、`PathOptimizationStrategy`、authority layout 规划等。
- 涉及文件：
  - `modules/dispense-packaging/README.md:8`
  - `modules/dispense-packaging/README.md:31`
  - `modules/dispense-packaging/domain/dispensing/README.md:3`
  - `modules/dispense-packaging/domain/dispensing/CMakeLists.txt:16`
- 为什么这是结构问题而不是局部实现问题：这决定模块在链路中的业务位置，影响谁来依赖它、谁来被它依赖。
- 可能后果：M8 与 M0/M7/M9 之间的边界持续模糊，新代码只能继续叠加而不是收敛。
- 优先级：P1

### 3. 依赖方向反转，M8 对 workflow/runtime 形成物理耦合

- 现象：M8 domain target 公开导出 workflow include root；M8 public header 直接包含 configuration/machine/motion/device 相关头；M9 直接构造 `DispensingProcessService` concrete，但这条 concrete 依赖当前更可能命中 `workflow/process_runtime_core` 旧执行服务上下文，而不是“明确直连 M8 concrete service”。
- 涉及文件：
  - `modules/dispense-packaging/domain/dispensing/CMakeLists.txt:38`
  - `modules/dispense-packaging/domain/dispensing/domain-services/DispensingProcessService.h:3`
  - `modules/runtime-execution/application/usecases/dispensing/DispensingExecutionUseCase.cpp:458`
- 为什么这是结构问题而不是局部实现问题：这是模块依赖图被反向污染，不是某个类写得重。
- 可能后果：无法独立演进或独立测试 M8，任何 runtime/workflow 变化都可能把 M8 一起带坏。
- 优先级：P1

### 4. 目录、构建、测试所有权不一致

- 现象：M8 目录里存在未编译的 `DispensingPlannerService.cpp`、`ContourOptimizationService.cpp`、`UnifiedTrajectoryPlannerService.cpp`，还有 `CMPTriggerService` stub、直接面向 `MultiCard` 的 `PositionTriggerController`；而相关测试仍大量留在 `workflow/tests`。
- 涉及文件：
  - `modules/dispense-packaging/domain/dispensing/planning/domain-services/DispensingPlannerService.cpp:1718`
  - `modules/dispense-packaging/domain/dispensing/domain-services/CMPTriggerService.cpp:1`
  - `modules/dispense-packaging/domain/dispensing/domain-services/PositionTriggerController.h:17`
  - `modules/dispense-packaging/tests/CMakeLists.txt:3`
  - `modules/workflow/tests/process-runtime-core/CMakeLists.txt:34`
- 为什么这是结构问题而不是局部实现问题：文件落点、构建图和验证归属同时不一致，团队无法仅靠目录判断 owner。
- 可能后果：死代码残留、误用 legacy 实现、回归测试职责继续漂移。
- 优先级：P2

## 6. 模块结论

- 宣称职责：`ExecutionPackage` / preview payload / built->validated / 离线校验 owner
- 实际职责：执行包组装 + authority trigger layout 规划 + 预览生成 + 触发/补偿规划 + 运行时点胶流程 + 阀门/供胶稳压 + 残留路径优化/CMP 控制
- 是否职责单一：否
- 是否边界清晰：否
- 是否被侵入：是，主要被 `workflow/domain` 的重复实现与 `workflow/tests` 的残留 owner 测试侵入
- 是否侵入别人：是，明显侵入了 workflow/runtime 方向的 configuration/motion/device/execution 关注点
- 是否适合作为稳定业务模块继续演进：不适合，至少在 owner 收口前不适合
- 最终评级：高风险

## 7. 修复顺序

### 第一步

- 目标：先把 M8 的正式 owner 面收口成一份书面与构建一致的边界。
- 涉及目录/文件：
  - `modules/dispense-packaging/README.md`
  - `modules/dispense-packaging/domain/dispensing/README.md`
  - `modules/dispense-packaging/module.yaml`
  - `modules/dispense-packaging/domain/dispensing/CMakeLists.txt`
- 收益：先消除“文档说打包，代码说执行”的双口径。
- 风险：会暴露当前下游对隐式职责的依赖，短期会出现“哪些类必须迁出/保留”的分歧。

### 第二步

- 目标：把运行时执行职责迁出 M8，只让 M8 保留 package/layout/validation 相关能力。
- 涉及目录/文件：
  - `modules/dispense-packaging/domain/dispensing/domain-services/DispensingProcessService.h`
  - `modules/dispense-packaging/domain/dispensing/domain-services/ValveCoordinationService.h`
  - `modules/dispense-packaging/domain/dispensing/domain-services/PurgeDispenserProcess.h`
  - `modules/runtime-execution/application/usecases/dispensing/DispensingExecutionUseCase.cpp`
- 收益：M8 与 M9 的边界回到“数据包 owner”对“执行 owner”的关系。
- 风险：会改动 runtime wiring 和现有执行链测试。

### 第三步

- 目标：最后统一重复实现与测试归属，删除 workflow bridge 中的 dispensing residual，只保留单一 owner。
- 涉及目录/文件：
  - `modules/workflow/domain/domain/CMakeLists.txt`
  - `modules/workflow/domain/domain/dispensing/`
  - `modules/workflow/tests/process-runtime-core/`
  - `modules/dispense-packaging/tests/`
- 收益：构建图、目录树、测试树三者一致，后续演进才有稳定落点。
- 风险：迁移期间最容易出现 include 路径和旧测试链接失败，需要分阶段删除 residual。

## 8. 证据索引

| 文件路径 | 得出的判断 | 支撑结论 |
|---|---|---|
| `modules/dispense-packaging/README.md:7` | M8 被宣称为 `ExecutionPackage` / preview / validation owner | 模块定位、声明职责 |
| `modules/dispense-packaging/README.md:31` | 文档明确说“不再编译 planner/optimizer” | 声明边界与实际不一致 |
| `modules/dispense-packaging/domain/dispensing/README.md:3` | 同模块内部又把自己定义成点胶过程/阀门/建压 owner | 模块内部口径冲突 |
| `modules/dispense-packaging/domain/dispensing/CMakeLists.txt:16` | 实际编译了 `PathOptimizationStrategy`、`DispensingProcessService` 等非纯打包能力 | 实际职责扩张 |
| `modules/dispense-packaging/domain/dispensing/CMakeLists.txt:38` | M8 public include 直接暴露 workflow domain include root | 依赖方向反转 |
| `modules/dispense-packaging/module.yaml:6` | 声明允许依赖与实际依赖不一致 | 依赖治理失真 |
| `modules/dispense-packaging/application/CMakeLists.txt:3` | application 明确拉入 `process-path/contracts`、`motion-planning/contracts` | 上游契约依赖是 live reality |
| `modules/dispense-packaging/application/include/application/services/dispensing/DispensePlanningFacade.h:37` | 输入/输出显示 M8 的 live 主链确实是“组包 + 预览 + layout” | 模块定位、主要输入输出 |
| `modules/workflow/domain/CMakeLists.txt:71` | workflow 直接把 M8 owner target 拉进来 | `workflow` 与 M8 强耦合 |
| `modules/workflow/domain/domain/CMakeLists.txt:121` | workflow 仍编译自己的 dispensing execution/planning compat | owner 分裂、被侵入 |
| `modules/runtime-execution/application/README.md:9` | M9 自己文档要求只接受已规划执行输入 | M8->M9 契约消费应当收敛 |
| `modules/runtime-execution/application/usecases/dispensing/DispensingExecutionUseCase.cpp:5` | M9 直接依赖 legacy `DispensingProcessService` 头，而不是只吃稳定契约 | 具体实现跨边界耦合 |
| `modules/runtime-execution/application/usecases/dispensing/DispensingExecutionUseCase.cpp:458` | M9 实际直接构造 `DispensingProcessService` concrete，但仅能确认命中 legacy 域类型，不能据此直接认定为 M8 concrete | 具体实现跨边界耦合 |
| `modules/runtime-execution/application/CMakeLists.txt:45-82` | M9 application 私有 include 仍依赖 `PROCESS_RUNTIME_CORE_*` 旧聚合上下文 | 具体实现跨边界耦合 |
| `modules/dispense-packaging/contracts/README.md:15` | M8 合同文档自己承认 `workflow/application/usecases/dispensing` 也是 canonical 实现面 | owner 未收口 |
| `modules/dispense-packaging/tests/CMakeLists.txt:3` | M8 自测未覆盖若干 live execution services | 测试 owner 不完整 |
| `modules/workflow/tests/process-runtime-core/CMakeLists.txt:34` | `PurgeDispenserProcess` / `ValveCoordinationService` / `DispensingProcessService` 测试仍在 workflow | 测试所有权残留 |
| `modules/dispense-packaging/domain/dispensing/domain-services/CMPTriggerService.cpp:1` | 目录内存在 stub/遗留实现 | 目录与构建边界不一致 |
| `modules/dispense-packaging/domain/dispensing/domain-services/PositionTriggerController.h:17` | 同层混入直接面向 `MultiCard` 的遗留控制器 | 模块内部结构混乱 |

## 备注

- 关于“是否存在显式 target 环依赖”：证据不足。我没有看到直接的 CMake 环。
- 关于“是否存在 source/include 级双向耦合”：证据充分，已在上文列出。

## 复核附录

- 2026-03-31 二次复核补充了重复实现的可复现哈希证据，使用命令：
  - `Get-FileHash <path> -Algorithm SHA256`
- 哈希结果：
  - `modules/workflow/domain/domain/dispensing/domain-services/TriggerPlanner.cpp`
    - `6368E33A1A0188885B0D73392055BC294B14C8325F0F6B364BA7007FA4DC58AF`
  - `modules/dispense-packaging/domain/dispensing/domain-services/TriggerPlanner.cpp`
    - `6368E33A1A0188885B0D73392055BC294B14C8325F0F6B364BA7007FA4DC58AF`
  - `modules/workflow/domain/domain/dispensing/domain-services/DispensingController.cpp`
    - `FC0B1B100B58F0EACD97FFB4163E90BF554D45D4C6A38D936C5A559623ED2D08`
  - `modules/dispense-packaging/domain/dispensing/domain-services/DispensingController.cpp`
    - `FC0B1B100B58F0EACD97FFB4163E90BF554D45D4C6A38D936C5A559623ED2D08`
  - `modules/workflow/domain/domain/dispensing/value-objects/TriggerPlan.h`
    - `38E20B6ECE4E68D936CA03341E6136BC958CA536412C47D414AF3A36926B76EB`
  - `modules/dispense-packaging/domain/dispensing/value-objects/TriggerPlan.h`
    - `38E20B6ECE4E68D936CA03341E6136BC958CA536412C47D414AF3A36926B76EB`
  - `modules/workflow/domain/domain/dispensing/value-objects/DispensingPlan.h`
    - `0DB154053E542152E3A76DCD6ACF1B6685DAAE3A05C00971AA5C45CD4801F0F8`
  - `modules/dispense-packaging/domain/dispensing/value-objects/DispensingPlan.h`
    - `0DB154053E542152E3A76DCD6ACF1B6685DAAE3A05C00971AA5C45CD4801F0F8`
  - `modules/workflow/domain/domain/dispensing/domain-services/DispensingProcessService.cpp`
    - `0FF6EBAEC272C3872BB49E780A4EDC74511B28E07A4645179519104186AAE6FF`
  - `modules/dispense-packaging/domain/dispensing/domain-services/DispensingProcessService.cpp`
    - `004FD9C12BD954C71319CE9C3394697DB73A45139B1E95A06744717FC9BB9581`
- 复核说明：
  - 以上结果支持“`workflow` 与 `M8` 存在多处重复实现，且 `DispensingProcessService.cpp` 已经分叉”。
  - 以上结果不支持“`runtime-execution` 已可明确判定为直接绑定 M8 concrete”；这一点仍应按 `workflow/process_runtime_core` 旧上下文解析链来定性。
