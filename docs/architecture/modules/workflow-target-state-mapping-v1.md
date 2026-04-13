# workflow target-state mapping v1

本文把当前 `modules/workflow/` 的物理结构映射到 [workflow-target-state-v1](./workflow-target-state-v1.md)。

它只回答一件事：

- 当前目录、文件、target 在长期目标态下应该 **保留 / 重组 / 迁出 / 删除** 到哪里。

本文不提供迁移步骤，不等价于实施计划。

## 1. 映射规则

统一使用以下处置动作：

- `保留`：仍属于 `M0 workflow` owner
- `重组`：仍属于 `M0 workflow`，但目录与命名需要重构
- `迁出`：不再属于 `M0`，应进入其他模块 owner
- `删除`：属于 compat / residue / wrapper / dual-root，应退出长期结构

## 2. 顶层目录映射

| 当前路径 | 当前状态 | 目标动作 | 目标落点 |
|---|---|---|---|
| `modules/workflow/contracts/` | live owner contracts | `重组` | 保留为 `workflow/contracts`，从单头聚合拆成 `commands/queries/events/dto/failures` |
| `modules/workflow/domain/` | live + residue 混合 | `重组` | 只保留 `WorkflowRun / StageTransitionRecord / RollbackDecision / policies / ports` |
| `modules/workflow/application/` | M0、M8、M9、M11 语义混合 | `重组 + 迁出` | 只保留 M0 command/query/facade；planning/execution 相关实现迁出 |
| `modules/workflow/adapters/` | 最小 live adapter | `重组` | 保留为 `adapters/persistence|messaging|projection_store` |
| `modules/workflow/runtime/` | 当前缺失 | `新增` | 新增轻量 orchestrators/subscriptions |
| `modules/workflow/services/` | shell-only 预留位 | `保留并实化` | 作为 M0 lifecycle/rollback/archive/projection 服务层 |
| `modules/workflow/tests/` | M0 + foreign owner 测试混合 | `重组 + 迁出` | 只保留 M0 tests；foreign owner 测试迁回各自 owner |
| `modules/workflow/examples/` | shell-only | `保留` | 仅保留 M0 最小示例 |
| `modules/workflow/docs/` | 历史治理与迁移记录 | `保留` | 继续作为模块内部治理记录，不进 public surface |

## 3. 当前 public target 映射

| 当前 target / export | 当前问题 | 目标动作 | 目标结果 |
|---|---|---|---|
| `siligen_workflow_contracts_public` | 方向正确，但内容过粗 | `重组` | 继续保留，内容收敛为 M0 contracts only |
| `siligen_workflow_domain_public` | 当前仍承载 compat header residue | `重组` | 继续保留，但只暴露 M0 owner domain |
| `siligen_workflow_application_headers` | 透传 foreign owner contracts | `删除` | 外部统一改走 `workflow/contracts` |
| `siligen_workflow_adapters_public` | 不得再透传 non-M0 adapter residue | `重组` | 若保留，仅暴露 M0 自己的 adapter interfaces |
| `siligen_application_dispensing` | 实际承载 planning/execution 交互链 | `迁出` | 退出 `modules/workflow` |
| `siligen_application_redundancy` | 实际承载代码冗余治理 use case，不是 M0 rollback owner | `删除` | 从 `modules/workflow` 的 canonical build graph 与 public target 集合退出 |
| `siligen_module_workflow` | 当前仍夹带 application payload | `重组` | 仅表示 M0 canonical bundle |

## 4. `contracts/` 映射

### 4.1 目录级

| 当前路径 | 目标动作 | 目标落点 |
|---|---|---|
| `contracts/include/workflow/contracts/WorkflowContracts.h` | `重组` | 保留为聚合头，仅做 `#include` 聚合 |

### 4.2 类型级

| 当前类型 | 目标动作 | 目标落点 |
|---|---|---|
| `WorkflowCommand` | `删除总枚举` | 拆入各 command 头 |
| `WorkflowFailureCategory` | `保留并收口` | `contracts/failures/WorkflowFailureCategory.h` |
| `WorkflowRecoveryAction` | `保留并收口` | `contracts/failures/WorkflowRecoveryAction.h` |
| `WorkflowStageLifecycle` | `删除长期 owner 地位` | 由 `WorkflowRunState` / `RollbackDecisionStatus` 取代 |
| `WorkflowRecoveryDirective` | `重组` | 融入 `WorkflowFailurePayload` |
| `WorkflowStageState` | `拆分` | `WorkflowRunView` + `StageTransitionRecordView` |
| `WorkflowPlanningTriggerRequest` | `迁出 workflow canonical surface` | 不再作为 M0 稳定契约 |
| `WorkflowPlanningTriggerResponse` | `迁出 workflow canonical surface` | 改为 owner 事件 + M0 顶层投影 |

## 5. `application/` 映射

### 5.1 当前不属于 `M0` 的 recovery / governance residue

| 当前路径 | 当前语义 | 目标动作 | 目标落点 |
|---|---|---|---|
| `application/recovery-control/CandidateScoringService.*` | repo 级代码冗余评分 | `删除/迁出` | 退出 `modules/workflow`；如仍保留能力，应进入独立 repo-governance / tooling owner |
| `application/recovery-control/RedundancyGovernanceUseCases.*` | repo 级删除候选治理与决策流 | `删除/迁出` | 退出 `modules/workflow`；不得并入 M0 rollback command handler |
| `application/usecases/redundancy/RedundancyGovernanceUseCases.h` | wrapper residue | `删除` | 不进入 M0 终态 |

### 5.2 当前应退出 `workflow` 的面

| 当前路径 | 当前语义 | 目标动作 | 目标落点 |
|---|---|---|---|
| `application/planning-trigger/PlanningUseCase.*` | 规划链触发 + 组装协作 | `迁出` | 退出 `M0`；由 `M11 hmi-application` 的 live 交互面消费 peer contracts |
| `application/phase-control/DispensingWorkflowUseCase.*` | preview/confirm/start job/运行前后交互 | `迁出` | 拆到 `M11 hmi-application` + `M9 runtime-execution` |
| `application/ports/dispensing/PlanningPorts.h` | M8/M7/M6 规划协作 port | `迁出` | peer owner contracts / live facade |
| `application/ports/dispensing/PlanningPortAdapters.*` | foreign facade adapter | `删除` | 由外部应用层显式依赖 peer modules |
| `application/ports/dispensing/WorkflowExecutionPort.h` | 执行链 port | `迁出` | `runtime-execution/contracts` 或上层 app facade |
| `application/services/dispensing/PreviewSnapshotService.h` | 预览链语义 | `迁出` | `hmi-application` 或相关 preview owner |
| `application/services/dispensing/WorkflowPlanningAssemblyTypes.h` | planning assembly glue | `迁出` | live 交互链 owner，不留在 M0 |
| `application/usecases/dispensing/**` | 历史残留 | `删除/迁出` | 不进入 M0 终态 |
| `application/usecases/motion/**` | 历史残留 | `删除/迁出` | 不进入 M0 终态 |
| `application/usecases/system/**` | 历史残留 | `删除/迁出` | 不进入 M0 终态 |

### 5.3 include root 映射

| 当前路径 | 当前问题 | 目标动作 | 目标落点 |
|---|---|---|---|
| `application/include/application/**` | dual-root residue | `删除` | 不再作为 public root |
| `application/include/workflow/application/**` | 夹带 foreign semantics | `删除或内聚` | 外部统一转向 `workflow/contracts` |

## 6. `domain/` 映射

### 6.1 当前不属于 `M0` 的 recovery / governance residue

| 当前路径 | 当前语义 | 目标动作 | 目标落点 |
|---|---|---|---|
| `domain/include/domain/recovery/redundancy/RedundancyTypes.h` | 代码实体 / 证据 / 候选 / 决策日志治理模型 | `删除/迁出` | 退出 `modules/workflow`；不得映射成 `RollbackDecision*` |
| `domain/include/domain/recovery/redundancy/ports/IRedundancyRepositoryPort.h` | repo 级冗余治理仓储 port | `删除/迁出` | 退出 `modules/workflow`；不得映射成 `IRollbackDecisionStore` |

### 6.2 当前应整体退出 `M0` 的 residue family

| 当前路径族 | 当前语义 | 目标动作 | 目标 owner |
|---|---|---|---|
| `domain/domain/dispensing/**` | M8/M9 混合 concrete residue | `迁出` | 离线 planning/timing 部分进 `dispense-packaging`；runtime actuation 部分进 `runtime-execution` |
| `domain/domain/motion/**` | M7/M9 混合 concrete residue | `迁出` | 规划部分进 `motion-planning`；执行部分进 `runtime-execution` |
| `domain/domain/geometry/**` | 几何 residue | `迁出` | `dxf-geometry` / `process-path` owner |
| `domain/domain/_shared/**` | bridge residue | `删除` | 不进入 M0 终态 |
| `domain/motion-core/**` | compat shell | `删除` | 不进入 M0 终态 |

### 6.3 `domain/include/` compat header 映射

| 当前路径族 | 目标动作 | 目标 owner |
|---|---|---|
| `domain/include/domain/motion/**` | `删除` | `motion-planning` / `runtime-execution` |
| `domain/include/domain/dispensing/**` | `删除` | `dispense-packaging` / `runtime-execution` |
| `domain/include/domain/safety/**` | `删除` | `runtime-execution` |
| `domain/include/domain/machine/**` | `删除` | `runtime-execution` / 相关 machine owner |

### 6.4 关键文件级判断

| 当前文件/目录 | 判断 | 目标 owner |
|---|---|---|
| `domain/domain/motion/domain-services/HomingProcess.*` | 设备执行 concrete，不属于 M0 | `runtime-execution` |
| `domain/domain/motion/domain-services/JogController.*` | 设备执行 concrete，不属于 M0 | `runtime-execution` |
| `domain/domain/motion/domain-services/MotionBufferController.*` | 设备执行 concrete，不属于 M0 | `runtime-execution` |
| `domain/domain/motion/domain-services/ReadyZeroDecisionService.*` | 设备执行/运行时决策，不属于 M0 | `runtime-execution` |
| `domain/domain/motion/value-objects/MotionTrajectory.h` | M7 owner 对象 | `motion-planning` |
| `domain/domain/motion/value-objects/TimePlanningConfig.h` | M7 owner 配置 | `motion-planning` |
| `domain/domain/dispensing/planning/domain-services/ContourOptimizationService.*` | 离线 planning residue | `dispense-packaging` |
| `domain/domain/dispensing/planning/domain-services/DispensingPlannerService.*` | 离线 planning residue | `dispense-packaging` |
| `domain/domain/dispensing/domain-services/TriggerPlanner.*` | timing/planning residue | `dispense-packaging` |
| `domain/domain/dispensing/domain-services/ArcTriggerPointCalculator.*` | timing/planning residue | `dispense-packaging` |
| `domain/domain/dispensing/domain-services/CMPTriggerService.*` | timing/pattern 语义，非 M0 | `dispense-packaging` |
| `domain/domain/dispensing/domain-services/DispenseCompensationService.*` | timing/package 或 runtime mixed | `dispense-packaging` 优先，必要时拆分 |
| `domain/domain/dispensing/domain-services/DispensingController.*` | 运行时执行 concrete | `runtime-execution` |
| `domain/domain/dispensing/domain-services/PositionTriggerController.*` | 运行时执行 concrete | `runtime-execution` |
| `domain/domain/dispensing/domain-services/PurgeDispenserProcess.*` | 运行时执行 concrete | `runtime-execution` |
| `domain/domain/dispensing/domain-services/SupplyStabilizationPolicy.*` | 运行时执行 concrete | `runtime-execution` |
| `domain/domain/dispensing/domain-services/ValveCoordinationService.*` | 运行时执行 concrete | `runtime-execution` |
| `domain/domain/dispensing/ports/IValvePort.h` | 运行时/设备 port | `runtime-execution/contracts` 或 `shared/device contracts` |
| `domain/domain/dispensing/ports/ITriggerControllerPort.h` | 运行时/设备 port | `runtime-execution/contracts` |
| `domain/domain/dispensing/ports/ITaskSchedulerPort.h` | 运行时调度 port | `runtime-execution/contracts` |

## 7. `adapters/` 映射

| 当前路径 | 目标动作 | 目标落点 |
|---|---|---|
| `adapters/infrastructure/adapters/redundancy/JsonRedundancyRepositoryAdapter.cpp` | `删除/迁出` | 退出 `modules/workflow`；不得重命名成 rollback persistence |
| `adapters/include/infrastructure/adapters/redundancy/JsonRedundancyRepositoryAdapter.h` | `删除/迁出` | 退出 `modules/workflow`；不得作为 M0 adapter surface 保留 |
| `adapters/README.md` | `保留` | 口径改为 M0 persistence/messaging/projection adapters |

## 8. `tests/` 映射

### 8.1 应保留为 `M0` 测试的集合

| 当前测试 | 目标动作 | 目标落点 |
|---|---|---|
| `tests/contract/WorkflowContractsTest.cpp` | `重写并保留` | M0 contracts contract tests |
| `tests/canonical/unit/domain/workflow/WorkflowDomainModelTest.cpp` | `新增并保留` | M0 owner domain unit tests |
| `tests/canonical/unit/domain/workflow/WorkflowSkeletonSurfaceTest.cpp` | `新增并保留` | M0 application/runtime skeleton surface tests |

### 8.2 应退出 `workflow` 的测试

| 当前测试 | 当前语义 | 目标动作 | 目标 owner |
|---|---|---|---|
| `tests/canonical/unit/redundancy/CandidateScoringServiceTest.cpp` | repo 级代码冗余评分 | `删除/迁出` | 独立 repo-governance / tooling owner |
| `tests/canonical/unit/redundancy/RedundancyGovernanceUseCasesTest.cpp` | repo 级删除候选治理流 | `删除/迁出` | 独立 repo-governance / tooling owner |
| `tests/canonical/unit/infrastructure/adapters/redundancy/JsonRedundancyRepositoryAdapterTest.cpp` | repo 级治理仓储 | `删除/迁出` | 独立 repo-governance / tooling owner |
| `tests/canonical/unit/dispensing/PlanningRequestTest.cpp` | planning request 语义 | `迁出` | `hmi-application` / live planning interaction chain |
| `tests/integration/DispensingWorkflowUseCaseTest.cpp` | preview/start job 交互链 | `迁出` | `hmi-application` + `runtime-execution` |
| `tests/integration/PlanningFailureSurfaceTest.cpp` | planning 失败链 | `迁出` | peer planning owner / live interaction layer |
| `tests/integration/PlanningUseCaseExportPortTest.cpp` | planning export chain | `迁出` | `hmi-application` / export owner |
| `tests/regression/DispensingWorkflowModeResolutionTest.cpp` | dispensing execution mode | `迁出` | `runtime-execution` / `hmi-application` |

## 9. `services/` / `runtime/` 实化映射

| 当前路径 | 当前状态 | 目标动作 | 目标落点 |
|---|---|---|---|
| `services/README.md` | shell-only | `实化` | `services/lifecycle|rollback|archive|projection` |
| `examples/README.md` | shell-only | `保留` | 仅允许 M0 minimal examples |
| `runtime/` | 当前缺失 | `新增` | `runtime/orchestrators|subscriptions` |

## 10. `CMakeLists` 方向性映射

| 当前文件 | 当前问题 | 目标动作 |
|---|---|---|
| `modules/workflow/CMakeLists.txt` | `siligen_module_workflow` 仍打包 application payload | 只保留 M0 contracts/domain/application facade/runtime orchestration/adapters |
| `modules/workflow/application/CMakeLists.txt` | 定义 `siligen_application_dispensing` 等非 M0 目标 | 删除或迁出这些 target |
| `modules/workflow/domain/CMakeLists.txt` | public root 仍含 compat residue | 只保留 M0 owner domain export |
| `modules/workflow/tests/CMakeLists.txt` | canonical/integration/regression 仍带 foreign owner 链 | 拆分为 M0 tests only |

## 11. 终态判定标准

当且仅当以下条件同时满足，才可认为 `modules/workflow` 已达到 target-state：

- `workflow/contracts` 只表达 M0 commands/queries/events/dto/failures
- `workflow/domain` 内不再存在 motion/dispensing/safety/machine concrete 或 compat header
- `workflow/domain` / `application` / `adapters` / `tests` 内不再存在 code redundancy governance candidate/evidence/repository surface
- `workflow/application` 内不再存在 planning/execution live interaction usecase
- `workflow/services` 已实化为 M0 lifecycle/rollback/archive/projection
- `workflow/runtime` 已存在且只承载轻量 orchestration
- `workflow/tests` 只验证 M0 owner 行为
- `siligen_module_workflow` 不再静默透传 foreign owner payload
