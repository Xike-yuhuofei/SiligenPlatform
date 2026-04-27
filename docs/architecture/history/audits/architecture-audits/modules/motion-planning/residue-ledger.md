# motion-planning Residue Ledger

## 1. Executive Verdict
- 一句话判定：canonical 化进行中但 closeout 未完成。

## 2. Scope
- 本轮只审查目录范围：`modules/motion-planning/`
- 为判断边界仅引用少量关联对象：`apps/runtime-service/container/ApplicationContainer.Motion.cpp`、`apps/runtime-service/include/runtime_process_bootstrap/diagnostics/value-objects/TestDataTypes.h`、`modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/motion/IInterpolationPort.h`、`modules/runtime-execution/adapters/device/src/adapters/motion/controller/homing/HomingPortAdapter.h`、`modules/workflow/domain/include/domain/safety/domain-services/EmergencyStopService.h`、`modules/dispense-packaging/domain/dispensing/planning/domain-services/DispensingPlannerService.cpp`、`modules/process-planning/domain/configuration/value-objects/ConfigTypes.h`、`modules/coordinate-alignment/domain/machine/ports/IHardwareTestPort.h`

## 3. Baseline Freeze
- owner 基线：`README.md:3-21`、`contracts/README.md:3-28`、`module.yaml:1-10` 将本模块冻结为 `M7 motion-planning` 的 canonical owner，owner 事实集中在 `MotionTrajectory`、`MotionPlanningReport`、`TimePlanningConfig`、`MotionPlanner`，调用主链为 `MotionPlanningFacade -> MotionPlanner -> MotionTrajectory`。
- 职责边界基线：`README.md:24-25` 与 `domain/motion/README.md:3,15-18,100` 明确 runtime/control/execution owner 不应回流到 `modules/motion-planning`；跨模块稳定公共契约也不应长期停留在本模块 `contracts/`。
- public surface 基线：`README.md:20` 与 `module.yaml:10` 冻结对外 surface 为 `motion_planning/contracts/*` 加四个 facade：`MotionPlanningFacade`、`CmpInterpolationFacade`、`InterpolationProgramFacade`、`TrajectoryInterpolationFacade`；`InterpolationAlgorithm` authority 固定在 `contracts/include/motion_planning/contracts/InterpolationTypes.h`。
- build 基线：`module.yaml:6-8` 只允许 `process-path/contracts` 与 `shared`；根 `CMakeLists.txt` 通过 `siligen_module_motion_planning` 聚合 `siligen_motion_planning_application_public` 与 `siligen_motion_planning_contracts_public`；`domain/motion/README.md:32` 要求缺失 canonical owner target 时显式失败，且 `siligen_motion` 只编译规划相关实现。
- test 基线：`tests/README.md` 要求模块级验证入口收敛在 `tests/`；测试应支撑 M7 owner closeout，而不是承担跨仓库 cleanup 审计或本机诊断脚本角色。

## 4. Top-Level Findings
- 核心规划 owner 已基本收敛到 M7：`MotionPlanner`、轨迹/时间/CMP 规划实现和 `motion_planning/contracts/*` 都在本模块内，主线 owner 没有再回到 workflow。
- 但 `module.yaml` 的 allowed dependency 已不再真实：`application/CMakeLists.txt:3-4,27-29,58-60` 和 `domain/motion/CMakeLists.txt:12-13,75-77` 都把 `runtime_execution_runtime_contracts` 拉进 live build。
- `siligen_motion_planning_application_public` 的 live surface 已超过冻结口径：除四个 facade 外，还公开了 `motion_planning/application/usecases/.../InterpolationPlanningUseCase.h`，且 `apps/runtime-service/container/ApplicationContainer.Motion.cpp:7` 已在消费它。
- `InterpolationProgramFacade` 与 `ValidatedInterpolationPort` 把 execution-specific DTO/port 直接塞进 M7 application/domain surface，导致规划 owner 与执行 owner 的 seam 没有真正收口。
- `domain/motion` 树里仍保留执行控制接口与 alias shim：`MotionControlService*`、`MotionStatusService*`、`IAxisControlPort`、`IJogControlPort`、`IHomingPort` 等仍然存在，并被 workflow compatibility 路径与安全服务继续引用。
- `MotionTypes.h`、`HardwareTestTypes.h`、`TrajectoryAnalysisTypes.h` 已成为 runtime/diagnostics/coordinate-alignment/process-planning 的共享来源，这不是 motion-planning owner 应长期承载的职责。
- `SemanticPath` 与 `TimeTrajectoryPlanner::Plan(const SemanticPath&)` 维持了一条 workflow-era 双 seam；当前 authority input 已是 `ProcessPath::Contracts::ProcessPath`，这条支线更像兼容壳而不是 live owner。
- `tests/` 已混合三类东西：模块本地算法证据、跨模块文件删除/owner 审计、以及依赖本机 `Demo.pb` 的诊断场景；closeout gate 不再清晰。
- README / module.yaml / domain README 三者对 execution owner 的叙述互相打架：文档仍写 workflow path，测试却已把 live concrete owner 固定到 `runtime-execution/application`。

## 5. Residue Ledger Summary
按类别统计（允许交叉记账）：
- owner residue：4
- build residue：2
- surface residue：3
- naming residue：1
- test residue：3
- placeholder residue：2

## 6. Residue Ledger Table
| residue_id | severity | file_or_target | current_role | why_residue | violated_baseline | evidence | action_class | preferred_destination | confidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| MP-R001 | P0 | `module.yaml`<br>`application/CMakeLists.txt`<br>`domain/motion/CMakeLists.txt` | 冻结依赖声明与 live build graph | `module.yaml:6-8` 只允许 `process-path/contracts` 与 `shared`，但 application/domain target 都把 `runtime_execution_runtime_contracts` 引入 live build，freeze 与事实脱节 | build 基线：allowed dependency 必须与 live graph 一致 | `module.yaml:6-10`；`application/CMakeLists.txt:3-4,27-29,58-60`；`domain/motion/CMakeLists.txt:12-13,75-77` | externalize | 先把 runtime-bound seam 迁到 `modules/runtime-execution/*`，再让 `module.yaml` 回到真实最小依赖 | high |
| MP-R002 | P1 | `application/CMakeLists.txt`<br>`application/include/motion_planning/application/usecases/motion/interpolation/InterpolationPlanningUseCase.h` | application public target 暴露的额外 app-facing seam | 冻结 public surface 只允许四个 facade，但 live public include root 还导出了 `InterpolationPlanningUseCase`；该头只是一个相对路径 re-export，且已被 runtime-service 直接 include | public surface 基线：只冻结四个 facade + contracts | `README.md:20`；`module.yaml:10`；`application/CMakeLists.txt:10,45,69`；`application/include/.../InterpolationPlanningUseCase.h:3`；`apps/runtime-service/container/ApplicationContainer.Motion.cpp:7` | migrate | `apps/runtime-service/` 或 `modules/runtime-execution/application/usecases/motion/interpolation/` | high |
| MP-R003 | P1 | `application/include/application/services/motion_planning/InterpolationProgramFacade.h`<br>`application/services/motion_planning/InterpolationProgramFacade.cpp` | 规划模块对外 facade | facade 直接返回 `runtime_execution::InterpolationData`，把 execution DTO 作为 M7 public surface 的一部分；`dispense-packaging` 与 `runtime-execution` 还在直接消费这条 seam | 职责边界基线：M7 public surface 只承诺规划事实，不重新吸收 runtime/control 语义 | `InterpolationProgramFacade.h:5`；`InterpolationProgramFacade.cpp:1-10`；`domain/motion/README.md:100`；`modules/dispense-packaging/.../PlanningAssemblyServices.cpp:4-5`；`modules/runtime-execution/application/usecases/motion/trajectory/DeterministicPathExecutionUseCase.cpp:3-4` | split | `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/motion/` + `modules/runtime-execution/application/usecases/motion/trajectory/` | high |
| MP-R004 | P1 | `domain/motion/CMakeLists.txt`<br>`domain/motion/domain-services/interpolation/ValidatedInterpolationPort.h`<br>`domain/motion/domain-services/interpolation/ValidatedInterpolationPort.cpp` | `siligen_motion` 的 live domain source | `ValidatedInterpolationPort` 继承 runtime-execution 的 `IInterpolationPort`，并被编进 `siligen_motion`；这不是规划算法 owner，而是 execution-adjacent validation wrapper | build 基线：`siligen_motion` 只编译规划相关实现；职责边界基线：不得重新吸收 runtime owner 语义 | `domain/motion/CMakeLists.txt:33,43,75-77`；`ValidatedInterpolationPort.h:4,10`；`domain/motion/README.md:95,100` | migrate | `modules/runtime-execution/application/services/motion/trajectory/` 或 `modules/runtime-execution/adapters/` | high |
| MP-R005 | P1 | `domain/motion/domain-services/MotionControlService.h`<br>`domain/motion/domain-services/MotionStatusService.h`<br>`domain/motion/domain-services/MotionControlServiceImpl.h`<br>`domain/motion/domain-services/MotionStatusServiceImpl.h` | 树内仍可解析的 execution service seam / alias shim | 这些文件不在 `siligen_motion` source list 中，但仍是 live header seam；workflow safety 继续 include 抽象服务，`*Impl.h` 则直接 alias 到 runtime-execution application implementation | 职责边界基线：`README.md:24` 已声明这组 owner 已迁到 `modules/runtime-execution`，M7 不再保留 shim | `README.md:24`；`MotionControlService.h:14`；`MotionStatusService.h:17`；`MotionControlServiceImpl.h:7`；`MotionStatusServiceImpl.h:7`；`modules/workflow/domain/include/domain/safety/domain-services/EmergencyStopService.h:14-15` | externalize | `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/motion/` 与 `modules/runtime-execution/application/include/runtime_execution/application/services/motion/` | high |
| MP-R006 | P1 | `domain/motion/ports/IAxisControlPort.h`<br>`domain/motion/ports/IJogControlPort.h`<br>`domain/motion/ports/IHomingPort.h` | 树内 execution control ports | 这些接口仍定义轴控制、JOG、回零等执行语义，且 workflow compatibility 头仍在透传它们；它们不是规划 owner，但仍让外部把 M7 当成执行接口来源 | 职责边界基线：runtime/control owner 不应长期停留在 M7 | `IAxisControlPort.h:34`；`IJogControlPort.h:31`；`IHomingPort.h:43`；`domain/motion/README.md:15-17,66-73,100`；`modules/workflow/domain/domain/motion/ports/IAxisControlPort.h:4`；`.../IJogControlPort.h:4`；`.../IHomingPort.h:4` | externalize | `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/motion/` | high |
| MP-R007 | P1 | `domain/motion/value-objects/MotionTypes.h`<br>`domain/motion/value-objects/HardwareTestTypes.h`<br>`domain/motion/value-objects/TrajectoryAnalysisTypes.h` | 被多个模块共享消费的 runtime/diagnostics/test 类型桶 | `MotionTypes.h` 承载 `MotionMode`、`HomingStage`、`MotionStatus`、`SafetyStatus`；`HardwareTestTypes.h` 承载回零/触发/硬件测试结果；`TrajectoryAnalysisTypes.h` 承载诊断分析。它们已被 runtime-service、runtime-execution、coordinate-alignment、process-planning 与 workflow shim 共同消费，明显超出规划 owner | owner 基线：M7 应拥有规划事实与轨迹求解，不应变成 runtime/diagnostics 的共享 owner | `MotionTypes.h:20,26,31,75,95,120,132`；`HardwareTestTypes.h:23,31,60,101,120,140`；`TrajectoryAnalysisTypes.h:30,58,72`；`apps/runtime-service/include/runtime_process_bootstrap/diagnostics/value-objects/TestDataTypes.h:4-6`；`modules/runtime-execution/.../HomingPortAdapter.h:5`；`modules/process-planning/domain/configuration/value-objects/ConfigTypes.h:3`；`modules/coordinate-alignment/domain/machine/ports/IHardwareTestPort.h:4` | split | 运行/诊断相关部分迁到 `modules/runtime-execution/contracts/runtime/` 或 `shared/contracts/diagnostics/`；仅保留真正规划 owner 所需的最小类型 | high |
| MP-R008 | P2 | `domain/motion/value-objects/SemanticPath.h`<br>`domain/motion/domain-services/TimeTrajectoryPlanner.h`<br>`domain/motion/domain-services/TimeTrajectoryPlanner.cpp` | workflow-era 双输入 seam | `TimeTrajectoryPlanner` 同时接受 `SemanticPath` 与 `ProcessPath`；`SemanticPath` 唯一外部痕迹已退化为 workflow compat include，而实现只是把它转换成 `ProcessPath` 再交给 `MotionPlanner` | public surface / contract freeze 基线：authority input 已是 `ProcessPath::Contracts::ProcessPath` | `TimeTrajectoryPlanner.h:20-21`；`SemanticPath.h:13,38,46,53`；`TimeTrajectoryPlanner.cpp:32-67`；`modules/workflow/domain/include/domain/motion/value-objects/SemanticPath.h:5` | demote | 优先收缩到 workflow compat-only shell；无消费者后直接删除 `SemanticPath` overload | high |
| MP-R009 | P2 | `contracts/include/motion_planning/contracts/MotionPlanningRequest.h` | 已导出的 contracts surface | 该 DTO 已经进入 contracts public include，但在 `apps/`、`modules/`、`shared/` 源码内没有任何消费；它扩大了 stable seam，却没有 live owner 价值 | public surface 基线：closeout 应收敛到最小稳定 seam | `MotionPlanningRequest.h:8`；全仓 `MotionPlanningRequest` 仅命中该文件本身 | delete | 无；若未来需要再以真实 consumer 反推新增 | high |
| MP-R010 | P1 | `tests/CMakeLists.txt` | 模块级单一 unit test target | 当前单个 `siligen_motion_planning_unit_tests` 同时链接 workflow include、dispense-packaging application、runtime-execution contracts、parsing adapter，已经是跨模块 residual 容器，不是纯 M7 closeout gate | test 基线：模块测试目标与物理目录应一致，且能直接支撑 owner closeout | `tests/CMakeLists.txt:21,35-37` | split | `tests/unit/` 保留纯 M7 owner 证据；跨模块场景拆到 `tests/integration/` 或消费者模块测试 | high |
| MP-R011 | P1 | `tests/unit/domain/motion/MotionPlanningOwnerBoundaryTest.cpp`<br>`tests/unit/domain/motion/MotionExecutionOwnerBoundaryTest.cpp`<br>`tests/unit/domain/motion/NoRuntimeControlLeakTest.cpp` | repo-wide 结构治理测试被放在模块 unit 目录 | 这些测试主要在检查 workflow/runtime-execution/dispense-packaging 的文件存在性、compat shim、target 清理与 alias 关系；它们有价值，但不是 motion-planning 自身的主证据 | test 基线：模块 unit suite 不应成为跨模块 cleanup 审计器 | `MotionExecutionOwnerBoundaryTest.cpp:50-158`；`MotionPlanningOwnerBoundaryTest.cpp:85-318`；`NoRuntimeControlLeakTest.cpp:19-36` | demote | 仓库级 architecture-contract lane，或专门的 cross-module contract tests 目录 | high |
| MP-R012 | P2 | `tests/unit/application/services/motion_planning/MotionPlanningFacadeTest.cpp`<br>`tests/unit/domain/motion/MainlineTrajectoryAuditTest.cpp` | 历史诊断/集成测试仍位于 unit 目录 | `MotionPlanningFacadeTest` 含本机 `Demo.pb` 诊断导出和 CSV 写盘；`MainlineTrajectoryAuditTest` 直接依赖 `DispensingPlannerService`。两者都不是稳定的本模块 unit gate | test 基线：主证据应是 deterministic、owner-local 的规划行为测试 | `MotionPlanningFacadeTest.cpp:93-160`；`MotionPlanningFacadeTest.cpp:94-96`；`MainlineTrajectoryAuditTest.cpp:2,147-180` | migrate | `tests/integration/`、手工诊断脚本，或对应 consumer module 的集成测试 | high |
| MP-R013 | P2 | `services/`<br>`adapters/`<br>`examples/` | 占位壳层目录 | 三个目录当前只有 README，没有实现、target 或测试；但根 target property 仍把它们列入 implementation roots，容易造成“骨架已完成”的假象 | placeholder 基线：目录名、语义、落点应一致 | `README.md:29-31`；`CMakeLists.txt:52-54`；目录实际仅含 `services/README.md`、`adapters/README.md`、`examples/README.md` | delete | 未使用就移出 implementation roots；确需保留时把其角色降级为显式 skeleton 说明 | high |
| MP-R014 | P1 | `README.md`<br>`module.yaml`<br>`domain/motion/README.md` | 模块级 frozen documentation | 文档仍写 execution source 固定在 workflow path，而测试已把 live concrete owner 锁定到 `runtime-execution/application`；`domain/motion/README.md` 目录树还列出了已删除的 `IInterpolationPort`、`IMotionRuntimePort`、`IIOControlPort` | naming / documentation 基线：README、module.yaml、CMake、tests 叙述必须互相一致 | `README.md:25`；`module.yaml:10`；`domain/motion/README.md:18,23,66-73,95,100`；`MotionExecutionOwnerBoundaryTest.cpp:132-141` | rename | 同步为与 live build/test 一致的 frozen wording，并删除已退场目录树项 | high |

## 7. Hotspots
- `application/CMakeLists.txt`：同时决定 public include root、public target 暴露物，以及对 `runtime_execution_runtime_contracts` 的 live 依赖。
- `application/include/motion_planning/application/usecases/motion/interpolation/InterpolationPlanningUseCase.h`：看似 canonical namespaced public header，实际只是 brittle re-export shim。
- `application/include/application/services/motion_planning/InterpolationProgramFacade.h`：M7 facade 直接暴露 execution DTO。
- `domain/motion/CMakeLists.txt`：`siligen_motion` 是否仍保持 planning-only owner，取决于这里是否继续编译 `ValidatedInterpolationPort.cpp` 并链接 runtime contracts。
- `domain/motion/domain-services/interpolation/ValidatedInterpolationPort.h`：名义是 domain service，实质是 runtime port wrapper。
- `domain/motion/value-objects/MotionTypes.h`：已演变成 motion status / safety / homing 状态桶。
- `domain/motion/value-objects/HardwareTestTypes.h`：diagnostics、runtime homing、coordinate-alignment 共同依赖的杂糅类型源。
- `tests/CMakeLists.txt`：单个 unit target 已吸入 workflow、dispense-packaging、runtime-execution、parsing 四类邻接依赖。
- `tests/unit/domain/motion/MotionPlanningOwnerBoundaryTest.cpp`：高价值，但本质是跨模块架构清理审计，不是本模块纯单测。
- `README.md` + `module.yaml` + `domain/motion/README.md`：freeze 叙述彼此冲突，容易误导后续 closeout。

## 8. False Friends
- `siligen_motion_planning_application_public`：名字像“干净 public target”，实际同时暴露了 usecase shim 和 runtime-execution 依赖。
- `ValidatedInterpolationPort`：名字像“规划领域校验器”，实际是 execution port wrapper。
- `MotionControlServiceImpl.h` / `MotionStatusServiceImpl.h`：看起来像本模块类，实际只是 `runtime-execution` application implementation 的 alias shim。
- `SemanticPath`：看起来像 motion-planning 自有 canonical path，实际上只是通往 `ProcessPath` 的兼容转换壳。
- `services/`、`adapters/`、`examples/`：看起来像骨架已就绪的 owner 子层，实际上仍是 README-only 占位壳。
- `MotionPlan`：名字像独立 contract，实际上在 `MotionPlan.h:7` 只是 `MotionTrajectory` 的业务别名；它不是新的 owner 对象。

## 9. Closeout Blockers
### 架构阻塞
- execution 控制口、execution service seam、runtime alias shim 仍停在 `modules/motion-planning/domain/motion/` 下。
- `InterpolationProgramFacade` 与 `ValidatedInterpolationPort` 让 execution 语义持续穿过 M7 surface / domain。
- `MotionTypes` / `HardwareTestTypes` / `TrajectoryAnalysisTypes` 已成为多模块共享 owner，motion-planning 无法独立 closeout。
- `SemanticPath` 与公开 usecase seam 让 M7 仍维持 workflow-era 双 surface。

### 构建阻塞
- `module.yaml` 与 live link graph 不一致，依赖冻结失真。
- `siligen_motion` 仍要靠 `runtime_execution_runtime_contracts` 才能完成编译。
- 根 target property 仍把 README-only 目录当作 implementation roots。

### 测试阻塞
- 单个 `siligen_motion_planning_unit_tests` 无法表达“纯本模块 gate”和“跨模块集成/治理 gate”的边界。
- 多个 `unit` 文件实际上在扫 workflow/runtime-execution/dispense-packaging 的文件布局，而不是只验证 M7 owner。
- `Demo.pb` 诊断测试和 `DispensingPlannerService` 场景污染了模块单测物理目录与语义边界。

### 文档 / 命名阻塞
- README / module.yaml / domain README 对 execution owner 的落点互相矛盾。
- domain README 目录树仍列出已经删除的 runtime headers。
- `application/include/application/...` 与 `application/include/motion_planning/...` 两套命名同时存在，surface 语义不一致。

## 10. No-Change Zone
- `contracts/include/motion_planning/contracts/InterpolationTypes.h`、`MotionTrajectory.h`、`TimePlanningConfig.h`、`MotionPlanningReport.h`、`MotionPlan.h`：当前是本轮冻结的 authority artifacts，不应在 residue closeout 前随意改语义。
- `domain/motion/domain-services/MotionPlanner.cpp`、`TrajectoryPlanner.cpp`、`SpeedPlanner.cpp`、`TriggerCalculator.cpp`、`SevenSegmentSCurveProfile.cpp` 与 interpolation 算法实现：先不要做算法级重写，本轮问题不在求解逻辑本身。
- 四个冻结 facade 的现有行为契约先保持稳定；下一阶段应围绕 seam 抽离，而不是先改 facade 行为。
- 不要把本轮 residue 清理扩散成 workflow、runtime-execution、dispense-packaging 的整体重构；邻接模块只做必要 consumer rewiring。

## 11. Next-Phase Input
- migration batches
  - 抽离 `InterpolationPlanningUseCase` 到真正的 app / execution owner。
  - 抽离 `InterpolationProgramFacade` 与 `ValidatedInterpolationPort` 的 execution seam。
  - 抽离 `MotionControlService*`、`MotionStatusService*`、`IAxisControlPort`、`IJogControlPort`、`IHomingPort`。
  - 抽离 `MotionTypes`、`HardwareTestTypes`、`TrajectoryAnalysisTypes` 到 runtime/diagnostics/shared 的真实 owner。
- delete batches
  - 删除无 consumer 的 `MotionPlanningRequest.h`。
  - 删除 `SemanticPath` overload（前提是 workflow compat consumer 清零）。
  - 删除或显式降级 `services/`、`adapters/`、`examples/` 占位壳。
- rename batches
  - 收口 `application/include/application/...` 与 `application/include/motion_planning/...` 的双命名 surface。
  - 对齐 README / module.yaml / domain README 中 execution owner 的表述。
- contract freeze items
  - authority input 是否唯一固定为 `ProcessPath::Contracts::ProcessPath`
  - `MotionPlan` 作为业务别名是否继续保留，但不得再派生第二套 payload
  - `InterpolationProgram` DTO 到底归 planning 还是 runtime-execution owner
  - `MotionTypes` / `HardwareTestTypes` / `TrajectoryAnalysisTypes` 的最终 owner
  - `InterpolationPlanningUseCase` 是 app seam 还是 module seam
- test realignment items
  - 哪些测试构成 M7 纯 unit gate
  - 哪些测试迁往 integration / consumer module
  - 哪些 repo-wide boundary tests 应归入 architecture-contract lane

## 12. Suggested Execution Order
1. Batch A：先冻结 public surface 与 dependency truth。
Acceptance: `module.yaml`、README、CMake 对 public surface 和 allowed deps 的说法一致；对外只剩 contracts + 明确批准的 facade 集。

2. Batch B：抽离 execution-specific export seam。
Acceptance: `InterpolationPlanningUseCase`、`InterpolationProgramFacade`、`ValidatedInterpolationPort` 不再让 `modules/motion-planning` 暴露或编译 execution port/DTO。

3. Batch C：清理 domain/motion 的 execution control residual。
Acceptance: `MotionControlService*`、`MotionStatusService*`、`IAxisControlPort`、`IJogControlPort`、`IHomingPort` 及相关 compat 头退出 M7 tree；`siligen_motion` 不再链接 `runtime_execution_runtime_contracts`。

4. Batch D：切分 value-object owner。
Acceptance: `MotionTypes`、`HardwareTestTypes`、`TrajectoryAnalysisTypes` 迁到真实 owner 后，M7 只保留规划语义直接需要的最小类型。

5. Batch E：收缩双 seam 与 placeholder。
Acceptance: `TimeTrajectoryPlanner` 仅保留 authority input；`SemanticPath`、`MotionPlanningRequest`、README-only placeholder 目录都获得明确去留。

6. Batch F：重排测试与文档 closeout。
Acceptance: `tests/unit/` 只保留 deterministic 的 M7 owner 证据；集成/诊断/架构治理测试迁位；README / module.yaml / domain README 与 live build/test 完全一致。
