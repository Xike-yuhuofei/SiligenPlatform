# M8 Dispense-Packaging Residue Ledger

审查日期：2026-04-09
审查范围：`modules/dispense-packaging/`
审查性质：只做 residue ledger 归档，不改代码，不给重构方案。

## 1. Executive Verdict

`modules/dispense-packaging/` 的 canonical 化进行中但 closeout 未完成。

## 2. Baseline Freeze

- Owner 基线：`M8` 的 canonical owner 入口已冻结为模块根与 contracts/application seam，owner 产物冻结为 `ExecutionPackage`、`PlanningArtifactExportRequest`、`AuthorityTriggerLayout`、`TriggerPlan`、`DispensingPlan`；职责冻结为 preview payload 组装、execution package built/validated 转换、离线校验边界、向 `M9` 输出只读执行准备事实。
  证据：`README.md:3-10`、`module.yaml:1-16`、`contracts/README.md:3-19`
- 边界基线：`M6/M7` 只供给上游路径与运动结果，`M8` 内不应继续编译 `*Planner*/*Optimization*`；`workflow` 只能编排 `M8` public service；`PlanningAssemblyTypes.h` 只应是内部 stage 类型面，不是 workflow-facing seam。
  证据：`README.md:18-23`、`README.md:40-43`、`application/README.md:3-8`
- 构建基线：根 target 必须要求 `siligen_dispense_packaging_application_public`，不得回退到 domain target；`module.yaml` 允许依赖仅有 `motion-planning/contracts`、`process-path/contracts`、`process-planning/contracts`、`shared`。
  证据：`CMakeLists.txt:26-39`、`module.yaml:6-10`
- 测试基线：Phase 4 closeout 证据只认 `siligen_dispense_packaging_boundary_tests` 与 `siligen_dispense_packaging_workflow_seam_tests`；`unit_tests` 不是 closeout gate。
  证据：`README.md:46-55`、`tests/CMakeLists.txt:26-34`
- 审查方法基线：本轮是静态架构审查，未运行 build/test；结论仅基于 live CMake、public headers、实现文件、include/link 方向。

## 3. Residue Ledger Summary

- owner residue：9
- build residue：2
- surface residue：3
- naming residue：4
- test residue：4
- placeholder residue：3
- live façade/provider 并存重复：0 个确认项。当前问题不是“双套 seam 还都活着”，而是“单套 canonical seam 仍背着 residual 容器和泄漏面”。
- workflow-facing seam 当前已基本收敛到 `WorkflowPlanningAssemblyOperations.h` + `WorkflowPlanningAssemblyTypes.h`，未发现 workflow 继续直接 include `PlanningAssemblyTypes.h`；残留主要是 public export 泄漏和 namespace alias 漂移。
  证据：`modules/workflow/application/include/application/planning-trigger/PlanningUseCase.h:5`、`application/include/application/services/dispensing/WorkflowPlanningAssemblyOperations.h:3-18`

## 4. Residue Ledger Table

| residue_id | severity | file_or_target | current_role | why_residue | violated_baseline | evidence | action_class | preferred_destination | confidence |
|---|---|---|---|---|---|---|---|---|---|
| BR-01 | P0 | `application/CMakeLists.txt` | `siligen_dispense_packaging_application_public` 与其子 target 的导出图 | application public 仍拉入 `siligen_motion_planning_application_public`，并把 `siligen_valve_core` 一起作为 public export 暴露，已超出 `M8` packaging/validation owner surface | `module.yaml` contracts-only 依赖基线；README 的 `M6/M7` 不应在 `M8` 内继续编译 concrete planning | `application/CMakeLists.txt:15-19,43-64,110-153` | split | 仅保留 M8 assembly services/contracts 的 application public；阀控与执行 concrete 回到 `runtime-execution`；上游 planning concrete 留在 `motion-planning/process-path` owner | high |
| BR-02 | P0 | `domain/dispensing/CMakeLists.txt` | `siligen_dispense_packaging_domain_dispensing` live residual 容器 | domain target 直接吃 `runtime-execution` include root、`workflow` include root，链接 `motion/process_path application_public` 与 `device_contracts`，并重复编译旧 planner/optimization 源文件 | M8 domain 不应依赖 application/concrete；root target 不该被 domain 肥 target 兜底 | `domain/dispensing/CMakeLists.txt:25-35,49-61,74-100` | split | 缩回 M8 owner value objects / authority utilities；planning concrete 回 M6/M7，execution/process concrete 回 M9 | high |
| OR-01 | P0 | `application/services/dispensing/PlanningAssemblyServices.cpp` | canonical assembly seam 实现 | seam 内仍直接实例化 `CmpInterpolationFacade`、`MotionPlanningFacade`、`TrajectoryInterpolationFacade`、`InterpolationProgramFacade`，不是纯 packaging/validated 转换，而是在现场继续做上游 concrete planning | `M8` 只应做 preview/package/validation，不应继续编译上游 planning concrete | `PlanningAssemblyServices.cpp:3-6,1217-1250,1735-1750` | split | 保留 authority/execution/export 组装；planning/interpolation/program 生成改为消费 M6/M7 已有 owner 输出 | high |
| OR-02 | P0 | `domain/dispensing/planning/domain-services/DispensingPlannerService.h/.cpp` | 旧 DXF monolithic planner | 定义旧 `DispensingPlanRequest`、旧 `DispensingPlan`、`DispensingPlanner`，同时完成 DXF 读取、边界平移、轮廓优化、统一轨迹规划、trigger build、插补与程序生成；并与 `domain/dispensing/value-objects/DispensingPlan.h` 同名双生 | `M8` owner 产物已冻结，旧 monolithic planner 不应继续 live build | `DispensingPlannerService.h:50-125`、`DispensingPlannerService.cpp:1284-1499,1570-1843`、`DispensingPlan.h:30-47` | migrate | DXF/path/trajectory concrete 回上游 planning owner；M8 只保留 authority/package 相关 owner 语义 | high |
| OR-03 | P1 | `domain/dispensing/planning/domain-services/UnifiedTrajectoryPlannerService.h/.cpp` | domain planner wrapper | domain 层直接内嵌 `ProcessPathFacade` 与 `MotionPlanningFacade`，实际是 application owner 的 wrapper，不是 M8 domain owner 语义 | domain 不应反向依赖 application façade | `UnifiedTrajectoryPlannerService.h:3-4,48-49`、`UnifiedTrajectoryPlannerService.cpp:99-120` | migrate | `process-path` / `motion-planning` owner application surfaces | high |
| OR-04 | P1 | `domain/dispensing/planning/domain-services/ContourOptimizationService.cpp` + `domain-services/PathOptimizationStrategy.h` | contour reorder + nearest-neighbor/2-opt 优化 | `M8` domain 里仍保有 contour 级重排与 2-opt 优化实现，属于 planner/optimization residual，不是 packaging/validation owner | `M8` 不应继续编译 optimization concrete | `ContourOptimizationService.cpp:284-395`、`PathOptimizationStrategy.h:32-105` | migrate | `process-path` / planning owner | high |
| OR-05 | P0 | `domain/dispensing/domain-services/DispensingProcessService.h/.cpp` | process-control / execution concrete service | 直接依赖 `IInterpolationPort`、`IMotionStatePort`、`DeviceConnectionPort`、`IValvePort`，并做硬件连接校验、坐标系配置、缓冲区发送、暂停/恢复/停止；这是 `M9` runtime-execution 语义，不是 `M8` package owner | `M8` 只向 `M9` 输出可消费事实，不负责执行 concrete | `DispensingProcessService.h:3-10,31-88`、`DispensingProcessService.cpp:502-695,697-1048,1323-1409` | migrate | `modules/runtime-execution` | high |
| OR-06 | P0 | `domain/dispensing/value-objects/DispensingExecutionTypes.h` | contract backing type 的混杂容器 | 把 `DispensingExecutionPlan` 与 `DispensingRuntimeOverrides/Params/Options/Report`、`MachineMode`、`runtime_execution::InterpolationData` 混在一起，导致 `ExecutionPackage` 契约背后带入 runtime/machine 语义 | contracts surface 应只承载 owner 产物，不应反拖 runtime concrete | `DispensingExecutionTypes.h:5-10,30-160`、`contracts/include/domain/dispensing/contracts/ExecutionPackage.h:3,18-69` | split | `DispensingExecutionPlan` 留在 M8 contract DTO；runtime overrides/options/report 外提到 M9 | high |
| OR-07 | P1 | `application/include/dispense_packaging/application/usecases/dispensing/valve/ValveCommandUseCase.h` + `application/usecases/dispensing/valve/ValveUseCases.cpp` | live public valve command surface | public include 暴露 `PurgeDispenserProcess`、`ValveCoordinationService`、`DeviceConnectionPort`；实现里直接做硬件前置检查并构造 purge process，已经是 execution-concrete residual | 冻结 stable public service 列表不包含 valve runtime usecase | `ValveCommandUseCase.h:3-23,25-40`、`ValveUseCases.cpp:25-52,54-97` | migrate | `runtime-execution` / runtime host valve surface | high |
| OR-08 | P1 | `application/usecases/dispensing/valve/coordination/ValveCoordinationUseCase.h` | timing-centric compat usecase | 仍在 live 路径暴露 `ValveTimingConfig`、`TriggerPoint`、`ValveTimingResult`，重新引入 timing-centric surface，不属于冻结 owner 产物集 | naming baseline 与 owner artifact baseline | `ValveCoordinationUseCase.h:17-63` | demote | compat/debug-only surface；不再算入 M8 owner public seam | medium |
| OR-09 | P1 | `domain/dispensing/domain-services/CMPTriggerService.h/.cpp` | live-built CMP process-control service | 被 domain target 编译，但 `.cpp` 自己标明 `Minimal stub for compilation`，核心硬件配置仍是 TODO；这是 process-control/CMP concrete residual，不是 package owner | M8 不应持有 runtime/CMP concrete | `domain/dispensing/CMakeLists.txt:40`、`CMPTriggerService.h:29-179`、`CMPTriggerService.cpp:1,58-85` | migrate | runtime/hardware trigger owner | high |
| SR-01 | P1 | `application/include/application/services/dispensing/PlanningAssemblyTypes.h` | public include root 下的 internal stage 类型面 | workflow 当前没有直接消费它，但它仍通过 application public include root 对外可见，且测试夹具继续把它当 live seam 使用；这与“只保留内部 stage 类型面”不一致 | `PlanningAssemblyTypes.h` 仅内部使用的冻结事实 | `README.md:40-42`、`application/CMakeLists.txt:28-41`、`PlanningAssemblyTypes.h:36-181`、`PlanningAssemblyTestFixtures.h:4-5` | demote | `application/services/dispensing` 私有/internal include | high |
| SR-02 | P1 | `application/include/dispense_packaging/application/services/dispensing/PlanningAssemblyTypes.h` | pure compat forwarder | 仅做旧 include 路径转发，把 internal stage types 再次暴露一层，属于 bridge-only residual | internal stage 类型不应继续保留 workflow/public compat 面 | `application/include/dispense_packaging/application/services/dispensing/PlanningAssemblyTypes.h:1-3` | delete | 无 | high |
| SR-03 | P1 | `contracts/include/domain/dispensing/contracts/PlanningArtifactExportRequest.h` + `application/include/application/services/dispensing/PlanningArtifactExportPort.h` | contract + deprecated compatibility bridge | contract 文件把类型重新别名回 `Application::Services::Dispensing`，同时还保留 deprecated compatibility forwarder；外部消费者已经绑定到 alias，而不是纯 contracts 命名空间 | contracts surface 应与 owner artifact 一致，不应继续由 app namespace alias/compat port 承担稳定 seam | `PlanningArtifactExportRequest.h:24-27`、`PlanningArtifactExportPort.h:3-6`、`modules/workflow/application/include/application/planning-trigger/PlanningUseCase.h:84`、`modules/runtime-execution/runtime/host/runtime/planning/PlanningArtifactExportPortAdapter.h:10-17` | demote | 只保留 `domain/dispensing/contracts` 命名空间；端口接口归消费方 owner | high |
| NR-01 | P2 | `domain/dispensing/README.md` | 子域说明文档 | 文档仍把 `DispensingProcessService`、阀控、CMP、建压稳压、流量控制等定义成正式 owner 范围，且写“❌ 不依赖 application”，与 live CMake 事实相反 | owner scope 与 build facts | `domain/dispensing/README.md:3-15,21-26,31-46,68-73`、`domain/dispensing/CMakeLists.txt:85-100` | shrink | 仅保留 M8 authority/package residual 注记 | high |
| NR-02 | P2 | `application/dispensing/README.md` | application 子目录说明文档 | 仍写“阀控编排”且点名已 deprecated 的 `siligen_process_runtime_core_dispensing`，与冻结 public seam 不一致 | naming baseline 与 stable public service baseline | `application/dispensing/README.md:3,7` | shrink | 文档只描述 assembly/public seam | high |
| NR-03 | P1 | `module.yaml` | 模块元数据 notes | notes 仍使用 `WorkflowDispensingProcessOperationsProvider` 与 `DispensingProcessService` 的旧 runtime 口径，和当前稳定 seam 名单不一致 | canonical seam 命名冻结 | `module.yaml:13-16` | rename | 更新为 `PlanningAssemblyServices` / `WorkflowPlanningAssemblyOperationsProvider` 口径 | high |
| NR-04 | P2 | `README.md` | 模块根 closeout 说明 | 文档仍称 `PreviewSnapshotServiceTest.cpp` 依赖旧 `PreviewSnapshotPayload`；实际测试已改为 `PreviewSnapshotInput/Response`。同时 README 说“所有 live 实现与构建入口均已收敛”，与 residual build/container 现状不符 | closeout 文档口径基线 | `README.md:44,52-55`、`tests/unit/application/services/dispensing/PreviewSnapshotServiceTest.cpp:10-37` | rename | 模块根 README closeout 段落 | high |
| TR-01 | P1 | `tests/CMakeLists.txt` | 测试组织与 gate 入口 | 物理目录和 target 组织仍把 residual planner/process-control tests 聚进 `unit_tests`，但 closeout gate 已换成 boundary/workflow seam 两个 target；组织与 gate 不一致 | test baseline | `tests/CMakeLists.txt:3-18,26-34,48-62` | split | owner closeout targets 与 residual quarantine targets 分离 | high |
| TR-02 | P1 | `tests/unit/domain/dispensing/DispensePackagingBoundaryTest.cpp` | closeout boundary test 混合体 | 同一文件里既有正确的 no-fallback / workflow seam 收敛断言，也有把 `UnifiedTrajectoryPlannerService` 依赖 `ProcessPathFacade`、domain target 链接 `process_path_application_public` 当成应被守护的事实；这会把 residual 拿来当 gate | owner closeout test 应只守 canonical owner 边界，不应守 residual topology | `DispensePackagingBoundaryTest.cpp:28-58,103-149` | split | 保留 owner closeout 断言；residual acceptance 迁出 gate | high |
| TR-03 | P1 | `tests/unit/application/services/dispensing/PlanningAssemblyTestFixtures.h` | seam 测试夹具 | 夹具同时 include `PlanningAssemblyTypes.h` 与 `WorkflowPlanningAssemblyTypes.h`，持续把 internal stage plane 和 workflow plane 耦合在一起 | stage types internal-only baseline | `PlanningAssemblyTestFixtures.h:3-5,13-21` | split | workflow seam tests 只用 workflow types；内部 stage tests 留内聚单测 | high |
| TR-04 | P2 | `ContourPathOptimizerTest.cpp`、`UnifiedTrajectoryPlannerServiceTest.cpp`、`PurgeDispenserProcessTest.cpp`、`ValveCoordinationServiceTest.cpp`、`DispensingControllerTest.cpp`、`TrajectoryTriggerUtilsTest.cpp` | 历史 unit 噪音批次 | 这些测试分别绑定优化 residual、planner wrapper residual、process-control residual、阀控 residual、执行 controller residual，或是 shared util；不构成 M8 owner closeout 证据 | owner closeout 证据边界 | 各文件首行 include：`ContourPathOptimizerTest.cpp:1`、`UnifiedTrajectoryPlannerServiceTest.cpp:1`、`PurgeDispenserProcessTest.cpp:1`、`ValveCoordinationServiceTest.cpp:1`、`DispensingControllerTest.cpp:3`、`TrajectoryTriggerUtilsTest.cpp:1` | split | residual quarantine lane，或回各自真正 owner 的测试域 | high |
| PR-01 | P2 | `domain/dispensing/model/ModelService.h`、`domain/dispensing/simulation/SimulationRecordStore.h`、`domain/dispensing/compensation/TriggerStrategy.h` | 未接入 build 的 closed-loop 占位链 | 三个 header 只在彼此链内互引，未进入 domain target source list，也未形成 owner seam；属于 repository-presence，不是 live owner | live build 与仓库存在必须区分 | `domain/dispensing/CMakeLists.txt:38-62`、`ModelService.h:31-74`、`SimulationRecordStore.h:29-77`、`TriggerStrategy.h:35-60` | delete | 无 | high |
| PR-02 | P2 | `domain/dispensing/domain-services/PositionTriggerController.h/.cpp` | 未接入 build 的硬件 concrete leftover | 直接依赖 `MultiCardInterface/MultiCard`，`.cpp` 还留硬件调用 TODO，但不在 CMake source list；属于未关干净的硬件 concrete 遗留 | repository-presence ≠ live seam | `PositionTriggerController.h:11-21`、`PositionTriggerController.cpp:71-77`、`domain/dispensing/CMakeLists.txt:38-62` | delete | 无；若未来复活，应去硬件 adapter owner | high |
| PR-03 | P2 | `tests/contract/.gitkeep`、`tests/integration/.gitkeep`、`tests/regression/.gitkeep` | 空壳测试目录 | 当前只有 `.gitkeep`，没有 owner 证据测试；是占位壳层，不是 live validation lane | test structure 应与真实 gate/evidence 对齐 | 三个目录仅存在 `.gitkeep` | delete | 无；等真实证据落地后再恢复目录 | high |

## 5. Hotspots

- `application/CMakeLists.txt`：这是 public surface 污染的总开关，决定了 app public 是否继续把 motion-planning concrete 和 valve concrete 一起导出。
- `domain/dispensing/CMakeLists.txt`：这是 domain 肥 target 的装载点，planner residual、process-control residual、CMP stub 都在这里 live build。
- `application/services/dispensing/PlanningAssemblyServices.cpp`：canonical seam 名义最强，但内部仍最重地执行上游 concrete planning。
- `domain/dispensing/planning/domain-services/DispensingPlannerService.cpp`：旧 monolithic planner 的完整行为还在，后续治理不碰它，owner closeout 就过不去。
- `domain/dispensing/domain-services/DispensingProcessService.cpp`：这是 process-control/execution concrete 的主战场，直接把 M9 语义压在 M8 domain 内。
- `domain/dispensing/value-objects/DispensingExecutionTypes.h`：owner contract 与 runtime 语义缠在一起，任何 contract freeze 都会卡在这里。
- `application/include/application/services/dispensing/PlanningAssemblyTypes.h`：不是当前 workflow seam，但仍然处于 public include root，形成 surface leakage。
- `application/services/dispensing/WorkflowPlanningAssemblyOperationsProvider.cpp`：稳定 seam 没错，但里面的双向 DTO 搬运说明 canonical seam 还背着兼容桥角色。
- `tests/CMakeLists.txt`：closeout gate 和历史 unit bundle 混放，后续测试清理都要从这里切。
- `tests/unit/domain/dispensing/DispensePackagingBoundaryTest.cpp`：这是“好证据”和“坏守护”混在一起的 gate 文件，不拆就会把 residual 锁死。

## 6. False Friends

- `siligen_dispense_packaging_application_public`：名字看起来像 canonical public surface，实际导出了 motion-planning application public 和 `siligen_valve_core`。
  证据：`application/CMakeLists.txt:146-153`
- `siligen_dispense_packaging_domain_dispensing`：名字像 M8 domain owner core，实际是 planner/optimization/process-control/CMP stub 的总容器。
  证据：`domain/dispensing/CMakeLists.txt:38-62,74-100`
- `PlanningAssemblyServices.cpp`：名字像 assembly service，实现里却直接跑上游 planning concrete。
  证据：`PlanningAssemblyServices.cpp:1217-1250,1735-1750`
- `WorkflowPlanningAssemblyOperationsProvider.cpp`：名字像薄 provider，实际上承担了 internal stage plane 与 workflow plane 的大段搬运桥接。
  证据：`WorkflowPlanningAssemblyOperationsProvider.cpp:15-230`
- `DispensingExecutionTypes.h`：名字像 owner value object，实际把 runtime overrides/options/report 也一起收进来了。
  证据：`DispensingExecutionTypes.h:30-160`
- `DispensePackagingBoundaryTest.cpp`：名字像 closeout boundary gate，实际有一部分断言是在守护 residual topology。
  证据：`DispensePackagingBoundaryTest.cpp:28-58`

## 7. Closeout Blockers

### 架构阻塞

- `PlanningAssemblyServices.cpp:1217-1250,1735-1750` 仍直接执行上游 concrete planning。
- `DispensingPlannerService.cpp:1570-1843` 仍是 live monolithic planner。
- `DispensingProcessService.cpp:697-1048,1323-1409` 仍在 M8 domain 内承担 execution concrete。
- `DispensingExecutionTypes.h:30-160` 仍未把 contract DTO 与 runtime 语义拆开。

### 构建阻塞

- `application/CMakeLists.txt:149-153` 的 public export 图和 `module.yaml:6-10` 的 allowed deps 明显冲突。
- `domain/dispensing/CMakeLists.txt:74-100` 仍存在 domain -> application/runtime/workflow 的 include/link 越界。
- `domain/dispensing/CMakeLists.txt:49-61` 仍把 residual planner 源文件直接编译进 domain target。

### 测试阻塞

- `tests/CMakeLists.txt:3-18` 的 `unit_tests` 仍混着大量 residual noise。
- `DispensePackagingBoundaryTest.cpp:28-58` 把 residual topology 当成 boundary gate。
- `PlanningAssemblyTestFixtures.h:3-5` 仍耦合 internal/workflow 双 type plane。
- `tests/contract`、`tests/integration`、`tests/regression` 仍是空壳目录，不支撑 owner closeout 证据。

### 文档/命名阻塞

- `module.yaml:13-16` 仍写旧 runtime/provider 口径。
- `domain/dispensing/README.md:3-15` 与 `application/dispensing/README.md:3,7` 仍把 process-control/阀控当正式 owner 面。
- `README.md:52-55` 的 closeout 叙述已与实际测试代码不一致。

## 8. No-Change Zone

- `CMakeLists.txt:27-31` 的“根 target 必须要求 `siligen_dispense_packaging_application_public`，拒绝回退到 domain target”这条 guard 应保持不动。
- `contracts/CMakeLists.txt:17-43` 的 `siligen_dispense_packaging_contracts_public` target 作为 contracts root 应保持。
- `contracts/include/domain/dispensing/contracts/ExecutionPackage.h:18-69` 的 built/validated 二分语义应冻结；问题在 backing type，不在 built/validated 语义本身。
- `contracts/include/domain/dispensing/contracts/PlanningArtifactExportRequest.h:12-20` 的 contract payload 主体应冻结；问题在 alias/compat，不在 request 载荷本体。
- `application/include/application/services/dispensing/WorkflowPlanningAssemblyOperations.h:8-18` 是当前 workflow-facing 稳定 seam，应保持。
- `application/include/application/services/dispensing/WorkflowPlanningAssemblyTypes.h:20-148` 是当前 workflow-facing 类型面，应保持。
- `application/include/application/services/dispensing/AuthorityPreviewAssemblyService.h`、`ExecutionAssemblyService.h`、`PlanningArtifactExportAssemblyService.h`、`WorkflowPlanningAssemblyOperationsProvider.h` 作为当前稳定 public service/provider 名称面应保持。
- `tests/unit/application/services/dispensing/WorkflowPlanningAssemblyOperationsContractTest.cpp` 与 `tests/unit/application/services/dispensing/PreviewSnapshotServiceTest.cpp` 当前是最接近 owner closeout 的证据，不应在本轮扩散处理。

## 9. Next-Phase Input

### migration batches

- Batch M1：`DispensingPlannerService.h/.cpp`、`UnifiedTrajectoryPlannerService.h/.cpp`、`ContourOptimizationService.cpp`、`PathOptimizationStrategy.h`
- Batch M2：`DispensingProcessService.h/.cpp`、`ValveCommandUseCase.h`、`ValveUseCases.cpp`、`ValveCoordinationUseCase.h`、`CMPTriggerService.h/.cpp`、`siligen_valve_core`
- Batch M3：`DispensingExecutionTypes.h` 对 `ExecutionPackage` backing type 的 runtime 字段分离
- Batch M4：`application/CMakeLists.txt` 与 `domain/dispensing/CMakeLists.txt` 的 target graph 收缩

### delete batches

- Batch D1：`application/include/dispense_packaging/application/services/dispensing/PlanningAssemblyTypes.h`
- Batch D2：`domain/dispensing/model/ModelService.h`、`domain/dispensing/simulation/SimulationRecordStore.h`、`domain/dispensing/compensation/TriggerStrategy.h`
- Batch D3：`domain/dispensing/domain-services/PositionTriggerController.h/.cpp`
- Batch D4：`tests/contract/.gitkeep`、`tests/integration/.gitkeep`、`tests/regression/.gitkeep`
- Batch D5：`PlanningArtifactExportPort.h` 的 compat 面，在 consumer 切换完成后删除

### rename batches

- Batch R1：`module.yaml`、`README.md`、`domain/dispensing/README.md`、`application/dispensing/README.md` 的旧 runtime/provider/PreviewSnapshot 口径
- Batch R2：`ValveCoordinationUseCase.h` 中 `ValveTiming*` 命名与 owner artifact 口径对齐或显式降级为 compat 名称

### contract freeze items

- Freeze 1：`ExecutionPackageBuilt` / `ExecutionPackageValidated` 语义与校验边界
- Freeze 2：`PlanningArtifactExportRequest` 只保留 contracts owner 命名空间，不再回灌 `Application::Services::Dispensing` alias
- Freeze 3：`AuthorityTriggerLayout`、`TriggerPlan`、`DispensingPlan` 的 M8 owner 定义位置
- Freeze 4：`PlanningAssemblyTypes.h` internal-only；`WorkflowPlanningAssemblyTypes.h` workflow-only
- Freeze 5：`siligen_dispense_packaging_application_public` 的允许导出集

### test realignment items

- Item T1：拆分 `tests/unit/domain/dispensing/DispensePackagingBoundaryTest.cpp`，把 owner closeout 断言与 residual acceptance 断言分离
- Item T2：从 `siligen_dispense_packaging_unit_tests` 中剥离 residual noise 批次
- Item T3：保留 owner 证据主轴为 `tests/unit/application/services/dispensing/WorkflowPlanningAssemblyOperationsContractTest.cpp`、`tests/unit/application/services/dispensing/PreviewSnapshotServiceTest.cpp`，以及 `AuthorityTriggerLayoutPlanner/TriggerPlanner` 相关 owner 测试子集
- Item T4：让测试目录结构与实际 closeout gate 一致，不再用空壳目录占位
