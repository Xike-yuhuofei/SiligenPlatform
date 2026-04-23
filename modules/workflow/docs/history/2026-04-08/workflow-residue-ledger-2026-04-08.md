# Historical note
- 本文件是 `2026-04-08` 的治理基线快照，不应直接替代当前代码真值。
- 当前 canonical truth：
  - recipe manager 已退役；`modules/recipe-lifecycle` 与 recipe management public surface 当前应保持删除
  - `IEventPublisherPort` owner = `shared/contracts/runtime/include/runtime/contracts/system/IEventPublisherPort.h`
- 若下文仍把 `modules/recipe-lifecycle`、workflow recipe targets、`runtime_process_bootstrap/recipes/RecipeJsonSerializer.h` 或 workflow-owned event seam 记为当前 owner，请按历史快照理解。

# 1. Audit scope
- 主目录：`modules/workflow`
- 模块声明与入口：`modules/workflow/CMakeLists.txt`, `modules/workflow/module.yaml`, `modules/workflow/README.md`
- workflow 内部 build graph / include graph：`modules/workflow/domain/**`, `modules/workflow/application/**`, `modules/workflow/adapters/**`, `modules/workflow/tests/canonical/**`
- root build / gate：repo root `CMakeLists.txt`, `legacy-exit-check.ps1`, `ci.ps1`
- migration / closeout 证据：`scripts/migration/legacy_fact_catalog.json`, `scripts/migration/validate_workspace_layout.py`
- 直接相关 canonical owner README：
  - `modules/process-planning/README.md`
  - `modules/dispense-packaging/README.md`
  - `modules/motion-planning/README.md`
  - `modules/runtime-execution/README.md`
  - `modules/dxf-geometry/README.md`
  - `modules/topology-feature/README.md`
  - `modules/coordinate-alignment/README.md`
- 直接相关消费者与反向 build graph：
  - `apps/runtime-service/**`
  - `apps/planner-cli/**`
  - `apps/runtime-gateway/transport-gateway/**`
  - `modules/runtime-execution/**`

# 2. Frozen audit assumptions
1. `modules/workflow` 是 `M0 workflow` 的 canonical owner 入口。
2. `workflow` owner 范围仅限：
   - 工作流编排与阶段推进语义
   - 规划链触发与编排边界
   - 流程状态与调度决策事实归属
3. `workflow` 不应长期承载已迁出模块的 concrete 实现。
4. `src/`、`process-runtime-core/`、各类 compat/bridge 壳层若继续存在，只允许是 shell-only / forward-only / deprecated compatibility，不允许继续承载真实 `.h/.cpp` payload。
5. `domain/`、`application/`、`adapters/`、`contracts/`、`tests/` 应当是可解释的长期结构面；若名实不符，也应入账。
6. 审计以真实 build graph / include graph / public surface 为准，不以目录名自证清洁。

# 3. Residue identification criteria
- `compat-target`：兼容 target 仍在 build graph 中承担真实编译职责。
- `bridge-payload`：bridge / compat / shell 路径仍持有真实 `.cpp` 或真实装配责任。
- `include-leak`：外部或内部消费者仍通过非 canonical include surface 获取 workflow 能力或 foreign owner 能力。
- `owner-drift`：目录名、模块名、target 名显示属于 workflow，但真实职责不属于 workflow。
- `reverse-dependency`：其它模块仍通过 workflow target/surface 反向注入、拼接或约束 workflow。
- `deprecated-header-with-real-semantics`：名义上 deprecated / compat 的头文件仍携带真实业务语义或 owner alias。
- `concrete-backflow`：workflow 内部直接落入已迁出 owner 的 concrete 实现。
- `shell-only-violation`：壳层目录中存在真实逻辑、真实状态、真实装配责任。
- `build-graph-residue`：构建图中仍保留迁移前路径、target、link 关系。
- `naming-misleading`：目录 / target / 文件名表达的分层意图与真实职责不一致。
- `test-boundary-drift`：tests 承载了实现层 / compat 层职责，或测试依赖暴露真实边界未迁净。
- `doc-contract-drift`：README / module.yaml / CMake 声明职责与真实目录骨架、依赖关系不一致。

# 4. Workflow residue ledger

## 4.1 Confirmed residues

### WF-R001
- `residue_id`: `WF-R001`
- `status`: `confirmed`
- `residue_type`: `build-graph-residue`
- `severity`: `critical`
- `current_path`: `CMakeLists.txt` (repo root)
- `current_symbol_or_target`: `siligen_domain`; global include roots `modules/workflow/domain/process-core/include`, `modules/workflow/domain/motion-core/include`
- `observed_fact`: root `CMakeLists.txt` 仍把 `modules/workflow/domain/process-core/include`、`modules/workflow/domain/motion-core/include` 注入全局实现 include roots，并在 configure 阶段要求 `workflow/process-runtime-core 必须提供 siligen_domain`。
- `why_it_is_residue`: 物理 `src/` / `process-runtime-core/` 根已不在，但 root build graph 仍把 workflow 当 legacy `process-runtime-core` 提供者。
- `expected_owner_or_surface`: root build graph 只依赖 canonical module public targets，不再依赖 `siligen_domain` 或 legacy process-core / motion-core include 根。
- `illegal_pattern`: legacy build graph still requires compat aggregate
- `suggested_disposition`: `retarget`
- `acceptance_check`: root `CMakeLists.txt` 不再引用 `modules/workflow/domain/process-core/include`、`modules/workflow/domain/motion-core/include`，也不再要求 `siligen_domain` 作为 workflow/process-runtime-core 替身。
- `blocking_risk`: clean-exit 可在目录层看似完成时继续被脏 build graph 掩盖，后续 owner closeout 会出现假完成。

### WF-R002
- `residue_id`: `WF-R002`
- `status`: `confirmed`
- `residue_type`: `compat-target`
- `severity`: `critical`
- `current_path`: `modules/workflow/domain/process-core/CMakeLists.txt`
- `current_symbol_or_target`: `siligen_process_core`
- `observed_fact`: `siligen_process_core` 直接编译 `src/recipes/services/recipe_validation_service.cpp`、`src/recipes/services/recipe_activation_service.cpp`。
- `why_it_is_residue`: `process-core` 以兼容 target 形式继续承担 recipe concrete 编译职责，不是 shell-only。
- `expected_owner_or_surface`: recipe owner surface 应位于 `modules/workflow` 之外的 canonical recipe owner，不应继续落在 compat target `workflow/domain/process-core`。
- `illegal_pattern`: compat target still compiles real source
- `suggested_disposition`: `move`
- `acceptance_check`: `siligen_process_core` 不再编译任何 real `.cpp`，`process-core` 仅剩空壳或彻底退出 build graph。
- `blocking_risk`: recipe concrete 继续固化在 workflow，owner 固化失败，后续依赖裁剪无法可信进行。

### WF-R003
- `residue_id`: `WF-R003`
- `status`: `confirmed`
- `residue_type`: `compat-target`
- `severity`: `critical`
- `current_path`: `modules/workflow/domain/motion-core/CMakeLists.txt`
- `current_symbol_or_target`: `siligen_motion_core`
- `observed_fact`: `siligen_motion_core` 直接编译 `src/safety/services/interlock_policy.cpp`。
- `why_it_is_residue`: `motion-core` 仍以 compat target 身份承载 safety/interlock concrete，不是 shell-only。
- `expected_owner_or_surface`: canonical safety / runtime owner surface，不应继续落在 `workflow/domain/motion-core` compat target。
- `illegal_pattern`: compat target still compiles real source
- `suggested_disposition`: `move`
- `acceptance_check`: `siligen_motion_core` 不再编译任何 real `.cpp`，`motion-core` 不再承载 live safety payload。
- `blocking_risk`: runtime safety 依赖继续经由 workflow 残留 target 传递，build graph 无法完成真实收口。

### WF-R004
- `residue_id`: `WF-R004`
- `status`: `confirmed`
- `residue_type`: `build-graph-residue`
- `severity`: `critical`
- `current_path`: `modules/workflow/domain/CMakeLists.txt`; `modules/runtime-execution/application/CMakeLists.txt`; `modules/runtime-execution/runtime/host/CMakeLists.txt`
- `current_symbol_or_target`: `siligen_domain`
- `observed_fact`: `siligen_domain` 仍聚合并导出 `siligen_process_core`、`siligen_motion_core`、`domain_machine`、`domain_recipes`、`siligen_dispense_packaging_domain_dispensing`、`siligen_process_path_contracts_public`、`siligen_process_planning_contracts_public`；`runtime-execution` application/host/tests 仍链接该 target。
- `why_it_is_residue`: `siligen_domain` 仍是 legacy super-aggregate，掩盖了 workflow 与 foreign owner 的真实边界。
- `expected_owner_or_surface`: `runtime-execution` 与 apps 应链接具体 canonical public targets，而不是 `siligen_domain` 这种 bridge aggregate。
- `illegal_pattern`: compat target still anchors foreign concrete
- `suggested_disposition`: `split`
- `acceptance_check`: 外部模块不再链接 `siligen_domain`，该 target 若保留也只做最小 forward，不再聚合 foreign concrete / foreign public。
- `blocking_risk`: build graph 污染持续，owner 迁移进度无法由依赖图真实体现。

### WF-R005
- `residue_id`: `WF-R005`
- `status`: `confirmed`
- `residue_type`: `shell-only-violation`
- `severity`: `high`
- `current_path`: `modules/workflow/domain/domain/CMakeLists.txt`
- `current_symbol_or_target`: `bridge-domain` load point; `siligen_dispensing_execution_services`, `siligen_recipe_domain_services`, `siligen_diagnostics_domain_services`, `siligen_safety_domain_services`
- `observed_fact`: `domain/domain/CMakeLists.txt` 注释声明当前目录是“迁移期 bridge-domain 聚合层”，但实际 `add_subdirectory(machine|recipes|diagnostics|safety|configuration|system|planning|recovery|planning-boundary|supervision)` 并定义多个真实 static target。
- `why_it_is_residue`: 名义上的 bridge-domain 仍承担真实 domain payload 编译与装配责任。
- `expected_owner_or_surface`: bridge-domain 只应保留 forward/deprecated shell，不应继续成为 live domain target 装配根。
- `illegal_pattern`: bridge carries real payload
- `suggested_disposition`: `deprecate-shell`
- `acceptance_check`: `domain/domain` 不再定义任何 foreign concrete target，只保留 forward-only / deprecated shell。
- `blocking_risk`: 目录看似在做 bridge 收口，实则继续作为 live payload 根，导致清理顺序与依赖切割都被误导。

### WF-R006
- `residue_id`: `WF-R006`
- `status`: `confirmed`
- `residue_type`: `concrete-backflow`
- `severity`: `high`
- `current_path`: `modules/workflow/domain/domain/CMakeLists.txt`; `modules/workflow/domain/domain/dispensing/domain-services/*`
- `current_symbol_or_target`: `siligen_dispensing_execution_services`
- `observed_fact`: `siligen_dispensing_execution_services` 编译 `PurgeDispenserProcess.cpp`、`SupplyStabilizationPolicy.cpp`、`ValveCoordinationService.cpp`。
- `why_it_is_residue`: purge / supply stabilization / valve coordination 是 dispensing concrete，不属于 `M0 workflow` 编排 owner。
- `expected_owner_or_surface`: `modules/dispense-packaging/domain/dispensing` 或 `modules/runtime-execution` 的执行 concrete，不应位于 workflow domain。
- `illegal_pattern`: workflow owns foreign concrete
- `suggested_disposition`: `move`
- `acceptance_check`: workflow domain 不再编译 purge / supply / valve coordination concrete。
- `blocking_risk`: dispensing owner 无法完成物理收口，workflow 继续成为 foreign concrete 的兜底容器。

### WF-R007
- `residue_id`: `WF-R007`
- `status`: `confirmed`
- `residue_type`: `owner-drift`
- `severity`: `high`
- `current_path`: `modules/workflow/domain/domain/machine/CMakeLists.txt`
- `current_symbol_or_target`: `domain_machine`
- `observed_fact`: `domain_machine` 编译 `aggregates/DispenserModel.cpp` 与 `domain-services/CalibrationProcess.cpp`。
- `why_it_is_residue`: `CalibrationProcess` 已在 `modules/coordinate-alignment/domain/machine/` 声明为 canonical owner 面；`DispenserModel` 也不属于 workflow 编排语义。
- `expected_owner_or_surface`: `modules/coordinate-alignment/domain/machine` 及非 workflow machine owner surface。
- `illegal_pattern`: workflow owns foreign concrete
- `suggested_disposition`: `split`
- `acceptance_check`: workflow domain 不再同时承载 `CalibrationProcess` 与 machine aggregate concrete。
- `blocking_risk`: machine / calibration owner 持续漂移，导致 runtime、alignment 与 workflow 三方边界长期不清。

### WF-R008
- `residue_id`: `WF-R008`
- `status`: `confirmed`
- `residue_type`: `concrete-backflow`
- `severity`: `high`
- `current_path`: `modules/workflow/domain/domain/CMakeLists.txt`; `modules/workflow/domain/domain/recipes/domain-services/*`
- `current_symbol_or_target`: `siligen_recipe_domain_services`
- `observed_fact`: `siligen_recipe_domain_services` 编译 `RecipeActivationService.cpp`、`RecipeValidationService.cpp`。
- `why_it_is_residue`: recipe activation / validation 是 recipe concrete，不属于 workflow 编排边界。
- `expected_owner_or_surface`: canonical recipe owner surface，至少不应继续位于 `modules/workflow/domain/domain/recipes`。
- `illegal_pattern`: workflow owns foreign concrete
- `suggested_disposition`: `move`
- `acceptance_check`: workflow 不再编译 `RecipeActivationService.cpp` / `RecipeValidationService.cpp`。
- `blocking_risk`: recipe 语义无法从 workflow 公共面切离，继续污染 public include 与 app wiring。

### WF-R009
- `residue_id`: `WF-R009`
- `status`: `confirmed`
- `residue_type`: `concrete-backflow`
- `severity`: `high`
- `current_path`: `modules/workflow/domain/domain/CMakeLists.txt`; `modules/workflow/domain/domain/safety/domain-services/*`
- `current_symbol_or_target`: `siligen_safety_domain_services`
- `observed_fact`: `siligen_safety_domain_services` 编译 `EmergencyStopService.cpp`、`InterlockPolicy.cpp`、`SafetyOutputGuard.cpp`、`SoftLimitValidator.cpp`，并链接 `siligen_motion_core`。
- `why_it_is_residue`: safety concrete 与其 motion compat 依赖仍附着在 workflow domain，说明 safety owner 未从 bridge-domain 迁出。
- `expected_owner_or_surface`: canonical safety / runtime owner surface，不应继续位于 workflow bridge-domain。
- `illegal_pattern`: workflow owns foreign concrete
- `suggested_disposition`: `split`
- `acceptance_check`: workflow bridge-domain 不再编译 safety concrete，也不再通过 `siligen_motion_core` 维持 safety 依赖。
- `blocking_risk`: runtime / safety 迁移无法完成，互锁与急停等关键语义继续经由脏 compat target 传播。

### WF-R010
- `residue_id`: `WF-R010`
- `status`: `confirmed`
- `residue_type`: `owner-drift`
- `severity`: `high`
- `current_path`: `modules/workflow/application/usecases/recipes/CMakeLists.txt`; `modules/workflow/application/usecases/recipes/*`
- `current_symbol_or_target`: `siligen_recipe_core`
- `observed_fact`: `siligen_recipe_core` 直接编译 `CreateRecipeUseCase.cpp`、`UpdateRecipeUseCase.cpp`、`CreateDraftVersionUseCase.cpp`、`UpdateDraftVersionUseCase.cpp`、`RecipeCommandUseCase.cpp`、`RecipeQueryUseCase.cpp`、`CreateVersionFromPublishedUseCase.cpp`、`CompareRecipeVersionsUseCase.cpp`、`ExportRecipeBundlePayloadUseCase.cpp`、`ImportRecipeBundlePayloadUseCase.cpp`。
- `why_it_is_residue`: recipe CRUD / bundle import-export 不是 workflow orchestration 语义；workflow 仍把 recipe application concrete 作为自身 public surface 输出。
- `expected_owner_or_surface`: recipe application owner surface，至少不应继续由 `modules/workflow/application` 承担。
- `illegal_pattern`: workflow owns foreign concrete
- `suggested_disposition`: `move`
- `acceptance_check`: workflow application 不再编译 recipe CRUD / bundle usecases。
- `blocking_risk`: apps 继续把 workflow 当 recipe facade，owner 边界无法清晰收敛。

### WF-R011
- `residue_id`: `WF-R011`
- `status`: `confirmed`
- `residue_type`: `owner-drift`
- `severity`: `high`
- `current_path`: `modules/workflow/application/usecases/dispensing/valve/CMakeLists.txt`; `modules/workflow/application/usecases/dispensing/valve/*`
- `current_symbol_or_target`: `siligen_valve_core`
- `observed_fact`: `siligen_valve_core` 编译 `ValveCommandUseCase.cpp`、`ValveQueryUseCase.cpp`，并链接 `siligen_dispensing_execution_services` 与 `siligen_process_planning_contracts_public`。
- `why_it_is_residue`: valve command/query 是 dispensing / device 级 concrete，不属于 workflow orchestration。
- `expected_owner_or_surface`: `modules/dispense-packaging/application` 或 runtime/device owner surface，而不是 workflow application。
- `illegal_pattern`: workflow owns foreign concrete
- `suggested_disposition`: `move`
- `acceptance_check`: workflow application 不再编译 valve command/query concrete。
- `blocking_risk`: workflow 对外继续暴露阀门级 usecase，外部依赖无法从 M0 编排层剥离。

### WF-R012
- `residue_id`: `WF-R012`
- `status`: `confirmed`
- `residue_type`: `owner-drift`
- `severity`: `high`
- `current_path`: `modules/workflow/application/CMakeLists.txt`; `modules/workflow/application/usecases/system/*`
- `current_symbol_or_target`: `siligen_application_system`
- `observed_fact`: `siligen_application_system` 编译 `InitializeSystemUseCase.cpp`、`EmergencyStopUseCase.cpp`；其公开头仍包含 `runtime_execution/application/usecases/system/IHardLimitMonitor.h` 与 `runtime_execution/contracts/system/IMachineExecutionStatePort.h`。
- `why_it_is_residue`: system initialization / emergency-stop concrete 继续落在 workflow application，并通过 runtime-execution 合同回接底层执行 owner。
- `expected_owner_or_surface`: `apps/runtime-service` app-local composition 与 `modules/runtime-execution` system/runtime public，不应继续由 workflow application 提供。
- `illegal_pattern`: workflow owns foreign concrete
- `suggested_disposition`: `move`
- `acceptance_check`: workflow application 不再编译 system concrete usecases，外部不再通过 workflow system 头获取 runtime/system 能力。
- `blocking_risk`: system owner 与 workflow orchestration 混层，后续 app/runtime 拆分将持续受阻。

### WF-R013
- `residue_id`: `WF-R013`
- `status`: `confirmed`
- `residue_type`: `owner-drift`
- `severity`: `critical`
- `current_path`: `modules/workflow/application/CMakeLists.txt`; `modules/workflow/application/usecases/motion/*`; `modules/workflow/application/execution-supervision/runtime-consumer/*`
- `current_symbol_or_target`: `siligen_application_motion`
- `observed_fact`: `siligen_application_motion` 编译 `ManualMotionControlUseCase.cpp`、`MotionInitializationUseCase.cpp`、`MotionSafetyUseCase.cpp`、`MotionMonitoringUseCase.cpp`、`MotionCoordinationUseCase.cpp`、`HomeAxesUseCase.cpp`、`EnsureAxesReadyZeroUseCase.cpp`、`MoveToPositionUseCase.cpp`、`DeterministicPathExecutionUseCase.cpp`、`ExecuteTrajectoryUseCase.cpp`、`InterpolationPlanningUseCase.cpp`，以及 `execution-supervision/runtime-consumer/MotionRuntimeAssemblyFactory.cpp`；该 factory 还直接 `make_unique` 多个 motion usecase 并内置 `DefaultMotionRuntimeValidationService`。
- `why_it_is_residue`: motion execution/control concrete 与 runtime assembly 仍由 workflow application 负责，不是 `M0 workflow` 编排边界。
- `expected_owner_or_surface`: `modules/runtime-execution/application` 的 execution/control public，与 `modules/motion-planning` 的 planning/contracts public。
- `illegal_pattern`: workflow owns foreign concrete
- `suggested_disposition`: `split`
- `acceptance_check`: workflow application 不再编译 motion execution/control concrete，也不再负责 runtime assembly factory。
- `blocking_risk`: motion owner 继续附着在 workflow，外部消费者无法按真实 owner 切依赖。

### WF-R014
- `residue_id`: `WF-R014`
- `status`: `confirmed`
- `residue_type`: `naming-misleading`
- `severity`: `high`
- `current_path`: `modules/workflow/application/CMakeLists.txt`
- `current_symbol_or_target`: `siligen_control_application`; alias `siligen_application`
- `observed_fact`: `workflow/application/CMakeLists.txt` 把 `siligen_control_application` 标注为 canonical target，并通过 `add_library(siligen_application ALIAS siligen_control_application)` 保留旧名。
- `why_it_is_residue`: target 名称表达的是一个“大一统 control application”，而实际内容混合 system / motion / dispensing / recipe / valve 多个 foreign owner concrete。
- `expected_owner_or_surface`: 只保留 workflow orchestration 语义的最小 public target，按真实 owner 切分其余 application target。
- `illegal_pattern`: naming hides owner drift
- `suggested_disposition`: `split`
- `acceptance_check`: 不再存在用单个 `siligen_control_application` / `siligen_application` 隐藏多 owner application concrete 的入口。
- `blocking_risk`: 上游持续误把 workflow 当“全能控制模块”，依赖裁剪和 owner closeout 都会失真。

### WF-R015
- `residue_id`: `WF-R015`
- `status`: `confirmed`
- `residue_type`: `include-leak`
- `severity`: `critical`
- `current_path`: `modules/workflow/application/CMakeLists.txt`
- `current_symbol_or_target`: `siligen_workflow_application_headers`
- `observed_fact`: `siligen_workflow_application_headers` 暴露 `modules/workflow/application/include`、`modules/dxf-geometry/application/include`、`shared compat include root`，并链接 `siligen_job_ingest_*`、`siligen_dispense_packaging_*`、`siligen_process_path_contracts_public`、`siligen_motion_planning_*`、`siligen_process_planning_contracts_public`、`siligen_device_contracts`、`siligen_runtime_execution_application_public`。
- `why_it_is_residue`: workflow application public header target 继续充当 foreign owner include / link 分发器，而不是只暴露 workflow canonical public surface。
- `expected_owner_or_surface`: foreign owner include/link 由消费者按 owner 直接声明，不应通过 `siligen_workflow_application_headers` 透传。
- `illegal_pattern`: non-canonical include surface still active
- `suggested_disposition`: `retarget`
- `acceptance_check`: `siligen_workflow_application_headers` 只暴露 workflow 自身 public include，不再透传 dxf/job/runtime/motion/dispense/process foreign owner surface。
- `blocking_risk`: include graph 与 link graph 持续脏化，任何消费者都可能通过 workflow 间接访问 foreign owner。

### WF-R016
- `residue_id`: `WF-R016`
- `status`: `confirmed`
- `residue_type`: `include-leak`
- `severity`: `high`
- `current_path`: `modules/workflow/domain/CMakeLists.txt`
- `current_symbol_or_target`: `siligen_workflow_domain_headers`
- `observed_fact`: `siligen_workflow_domain_headers` 暴露 `modules/workflow/domain/include`、`modules/motion-planning`、`modules/workflow/domain/process-core/include`、`modules/workflow/domain/motion-core/include`、`shared compat include root`。
- `why_it_is_residue`: workflow domain public header target 仍把 motion-planning 与 legacy process-core / motion-core surface 当作 workflow public surface 一部分向外透出。
- `expected_owner_or_surface`: workflow domain headers 只暴露 workflow canonical domain include；motion-planning 与 compat headers 应由消费者直接依赖对应 owner。
- `illegal_pattern`: non-canonical include surface still active
- `suggested_disposition`: `retarget`
- `acceptance_check`: `siligen_workflow_domain_headers` 不再向外暴露 motion-planning、process-core、motion-core 或 shared compat include 根。
- `blocking_risk`: domain include graph 无法切净，外部继续通过 workflow 获得 foreign owner 头文件。

### WF-R017
- `residue_id`: `WF-R017`
- `status`: `confirmed`
- `residue_type`: `build-graph-residue`
- `severity`: `high`
- `current_path`: `modules/workflow/CMakeLists.txt`
- `current_symbol_or_target`: `siligen_workflow_application_public`, `siligen_workflow_adapters_public`, `siligen_workflow_recipe_domain_public`, `siligen_workflow_recipe_application_public`, `siligen_workflow_recipe_serialization_public`, `siligen_workflow_runtime_consumer_public`, `siligen_module_workflow`
- `observed_fact`: workflow root 仍定义多个 interface bundle target，并把 recipe domain/application/serialization public、runtime consumer public、adapter public 挂到 `siligen_module_workflow` / `siligen_workflow_*_public` 之下。
- `why_it_is_residue`: workflow 模块根入口继续打包 recipe/runtime consumer/adapters 等 foreign owner public surface，形成表面干净、根入口仍脏的 build graph。
- `expected_owner_or_surface`: workflow 模块根只暴露 `M0` orchestration 所需的最小 public surface；recipe/runtime/adapter public 应由各自 owner 单独暴露。
- `illegal_pattern`: compat aggregate still exports foreign public surface
- `suggested_disposition`: `split`
- `acceptance_check`: `siligen_module_workflow` 不再静默透传 recipe/runtime consumer/adapter foreign surface。
- `blocking_risk`: 模块根 target 继续成为“万能入口”，clean-exit 时无法证明 root entry 已真实清洁。

### WF-R018
- `residue_id`: `WF-R018`
- `status`: `resolved`
- `residue_type`: `include-leak`
- `severity`: `medium`
- `current_path`: `modules/workflow/application/include/application/usecases/dispensing/PlanningUseCase.h`; `modules/workflow/application/include/workflow/application/usecases/dispensing/PlanningUseCase.h`; `modules/workflow/application/include/application/usecases/dispensing/DispensingWorkflowUseCase.h`; `modules/workflow/application/include/workflow/application/usecases/dispensing/DispensingWorkflowUseCase.h`
- `current_symbol_or_target`: public headers `PlanningUseCase.h`, `DispensingWorkflowUseCase.h`
- `observed_fact`: `planning-trigger/` 与 `phase-control/` canonical 公开头已经自持真实定义；本批次删除了仍会把消费者拉回旧 `usecases/dispensing` 路径的 legacy public wrapper。
- `why_it_is_residue`: 该 residue 已在当前批次完成收口；legacy public wrapper 一旦保留，就会继续让外部消费者停留在旧 include surface。
- `expected_owner_or_surface`: canonical public header 不再依赖旧 `usecases/dispensing/*` 路径。
- `illegal_pattern`: non-canonical include surface still active
- `suggested_disposition`: `done`
- `acceptance_check`: repo 中不再存在上述四个 `usecases/dispensing` public wrapper 文件。
- `blocking_risk`: 若旧 wrapper 回流，消费者会重新锚定到非 canonical include surface。

### WF-R019
- `residue_id`: `WF-R019`
- `status`: `resolved`
- `residue_type`: `deprecated-header-with-real-semantics`
- `severity`: `medium`
- `current_path`: `modules/workflow/application/include/workflow/application/services/dispensing/IPlanningArtifactExportPort.h`; `modules/workflow/application/include/workflow/application/services/dispensing/PlanningArtifactExportPort.h`; `modules/workflow/application/include/application/services/motion/runtime/IMotionRuntimeServicesProvider.h`; `modules/workflow/application/include/workflow/application/services/motion/runtime/IMotionRuntimeServicesProvider.h`
- `current_symbol_or_target`: `IPlanningArtifactExportPort`, `PlanningArtifactExportPort`, `IMotionRuntimeServicesProvider`
- `observed_fact`: `application/services/dispensing/IPlanningArtifactExportPort.h` 已在此前批次删除；当前批次继续删除 workflow namespace 下对 planning export contract 的 compat 头，consumer 统一改为直接使用 `runtime_execution/application/services/...` canonical contract。
- `why_it_is_residue`: 该 residue 已在当前批次完成收口；继续保留 workflow namespace compat 头会让 runtime-execution 语义继续被 workflow 复刻。
- `expected_owner_or_surface`: `modules/runtime-execution/application/include/runtime_execution/application/services/*`
- `illegal_pattern`: deprecated shell still exports business semantics
- `suggested_disposition`: `done`
- `acceptance_check`: repo 中不再存在 workflow namespace 下的 planning export compat 头，外部消费者只通过 runtime-execution canonical contract 获取该语义。
- `blocking_risk`: 若 compat 头重新出现，runtime-execution public surface 会再次被 workflow 复刻。

### WF-R020
- `residue_id`: `WF-R020`
- `status`: `confirmed`
- `residue_type`: `deprecated-header-with-real-semantics`
- `severity`: `high`
- `current_path`: `modules/workflow/domain/include/domain/dispensing/planning/domain-services/DispensingPlannerService.h`; `modules/workflow/domain/include/domain/dispensing/planning/domain-services/ContourOptimizationService.h`; `modules/workflow/domain/include/domain/dispensing/domain-services/ArcTriggerPointCalculator.h`; `modules/workflow/domain/include/domain/dispensing/domain-services/TriggerPlanner.h`
- `current_symbol_or_target`: M8 dispensing planning / trigger compat headers
- `observed_fact`: 这些 workflow public headers 都标注为 `Deprecated workflow compatibility header`，并直接包含 `modules/dispense-packaging/domain/dispensing/**` canonical owner 头。
- `why_it_is_residue`: workflow public surface 仍通过 deprecated compat 头暴露 `dispense-packaging` 的真实 planning / trigger 语义。
- `expected_owner_or_surface`: `modules/dispense-packaging/domain/dispensing/**`
- `illegal_pattern`: deprecated shell still exports business semantics
- `suggested_disposition`: `deprecate-shell`
- `acceptance_check`: 外部模块不再通过 workflow `domain/include/domain/dispensing/**` 获取 M8 planning / trigger 能力。
- `blocking_risk`: M8 canonical owner 虽已存在，外部依赖仍可绕回 workflow compat 面，导致迁移假完成。

### WF-R021
- `residue_id`: `WF-R021`
- `status`: `confirmed`
- `residue_type`: `deprecated-header-with-real-semantics`
- `severity`: `high`
- `current_path`: `modules/workflow/domain/include/domain/motion/domain-services/TrajectoryPlanner.h`; `modules/workflow/domain/include/domain/motion/domain-services/SpeedPlanner.h`; `modules/workflow/domain/include/domain/motion/domain-services/TriggerCalculator.h`; `modules/workflow/domain/include/domain/motion/value-objects/TimePlanningConfig.h`
- `current_symbol_or_target`: M7 motion planning compat headers
- `observed_fact`: 这些 workflow public headers 标注为 `Public compatibility shim` 或 `Deprecated workflow compatibility header`，并直接包含 `modules/motion-planning/domain/motion/**` 或 `motion_planning/contracts/**` canonical owner 头。
- `why_it_is_residue`: workflow public surface 仍通过 compat 头暴露 motion-planning 的真实算法/类型语义。
- `expected_owner_or_surface`: `modules/motion-planning/domain/motion/**`; `modules/motion-planning/contracts/include/motion_planning/contracts/**`
- `illegal_pattern`: deprecated shell still exports business semantics
- `suggested_disposition`: `deprecate-shell`
- `acceptance_check`: 外部模块不再通过 workflow `domain/include/domain/motion/**` 获取 M7 planning types / services。
- `blocking_risk`: M7 public surface 即使已 canonical 化，消费者仍可继续滞留在 workflow compat 根。

### WF-R022
- `residue_id`: `WF-R022`
- `status`: `confirmed`
- `residue_type`: `naming-misleading`
- `severity`: `medium`
- `current_path`: `modules/workflow/domain/include/domain/planning-boundary/ports/ISpatialIndexPort.h`
- `current_symbol_or_target`: `Siligen::Domain::PlanningBoundary::Ports::ISpatialIndexPort`
- `observed_fact`: 该“canonical” planning-boundary 头实际 `#include "../../../../domain/planning/ports/ISpatialIndexPort.h"`，并 `using` 旧 `Siligen::Domain::Planning::Ports::*` 符号。
- `why_it_is_residue`: 新目录名看似已切到 `planning-boundary`，但真实 public surface 仍依赖旧 `planning` compat 头。
- `expected_owner_or_surface`: `planning-boundary` canonical 头应独立于旧 `domain/planning/*` compat 路径。
- `illegal_pattern`: naming implies migration but surface still aliases legacy path
- `suggested_disposition`: `retarget`
- `acceptance_check`: `domain/planning-boundary/ports/ISpatialIndexPort.h` 不再通过旧 `domain/planning/*` 提供真实语义。
- `blocking_risk`: owner boundary 看似迁出、实则 public surface 未迁，后续 include 收口无法完成。

### WF-R023
- `residue_id`: `WF-R023`
- `status`: `confirmed`
- `residue_type`: `naming-misleading`
- `severity`: `medium`
- `current_path`: `modules/workflow/domain/include/domain/recovery/redundancy/ports/IRedundancyRepositoryPort.h`
- `current_symbol_or_target`: `Siligen::Domain::Recovery::Redundancy::Ports::IRedundancyRepositoryPort`
- `observed_fact`: 该“canonical” recovery 头实际 `#include "../../../../../domain/system/redundancy/ports/IRedundancyRepositoryPort.h"`，并 `using` 旧 `System::Redundancy` 命名空间符号。
- `why_it_is_residue`: 新目录名看似已经从 `system` 迁到 `recovery`，但真实语义仍锚定旧 `system/redundancy` compat 路径。
- `expected_owner_or_surface`: `domain/recovery/redundancy/**` 应独立承载 canonical public surface，不再依赖旧 `domain/system/**`。
- `illegal_pattern`: naming implies migration but surface still aliases legacy path
- `suggested_disposition`: `retarget`
- `acceptance_check`: `domain/recovery/redundancy/ports/IRedundancyRepositoryPort.h` 不再依赖旧 `domain/system/redundancy/*` compat 头。
- `blocking_risk`: recovery 目录可能被误判为已清洁，但实际 public surface 仍未脱离 legacy system path。

### WF-R024
- `residue_id`: `WF-R024`
- `status`: `resolved`
- `residue_type`: `include-leak`
- `severity`: `high`
- `current_path`: `apps/planner-cli/CommandHandlers.Motion.cpp`; `apps/planner-cli/CommandHandlers.Connection.cpp`; `apps/planner-cli/CommandHandlers.Dispensing.cpp`; `apps/planner-cli/CommandHandlers.Dxf.cpp`; `apps/runtime-service/container/ApplicationContainer.System.cpp`; `apps/runtime-service/container/ApplicationContainer.Motion.cpp`; `apps/runtime-service/container/ApplicationContainer.Dispensing.cpp`; `apps/runtime-gateway/transport-gateway/include/siligen/gateway/tcp/tcp_facade_builder.h`; `apps/runtime-gateway/transport-gateway/src/facades/tcp/*`
- `current_symbol_or_target`: old include roots `application/usecases/*`; `workflow/application/usecases/recipes/*`
- `observed_fact`: 本批次之前的 consumer cutover 已完成；`planner-cli`、`runtime-service`、`runtime-gateway` 不再依赖旧 `application/usecases/*`，而是切到 canonical `planning-trigger` / `phase-control` 与 foreign owner public surface。
- `why_it_is_residue`: 该 residue 已在前序 consumer cutover 批次收口，保留在 ledger 中仅用于记录已完成的外部切换事实。
- `expected_owner_or_surface`: `modules/workflow/application/include/application/planning-trigger/*`、`phase-control/*`，以及 foreign owner 的 canonical public headers。
- `illegal_pattern`: non-canonical include surface still active
- `suggested_disposition`: `done`
- `acceptance_check`: live code search 不再命中 apps 对 `application/usecases/*` 或 `workflow/application/usecases/recipes/*` 的依赖。
- `blocking_risk`: 若外部 consumer 回流到旧 include 根，public surface 会重新失去收口能力。

### WF-R025
- `residue_id`: `WF-R025`
- `status`: `confirmed`
- `residue_type`: `reverse-dependency`
- `severity`: `critical`
- `current_path`: `modules/runtime-execution/application/CMakeLists.txt`; `modules/runtime-execution/runtime/host/CMakeLists.txt`; `modules/runtime-execution/contracts/runtime/CMakeLists.txt`; `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/**`
- `current_symbol_or_target`: `siligen_domain`; `siligen_motion_core`; workflow `domain/*` include surface
- `observed_fact`: `runtime-execution` application/host/tests 仍链接 `siligen_domain`、`siligen_motion_core`；其 runtime contracts 头仍包含 workflow `domain/safety/*`、`domain/motion/*`、`domain/system/*` 头。
- `why_it_is_residue`: `runtime-execution` 作为下游 owner，仍反向把 workflow domain 当作 live public / concrete 依赖根。
- `expected_owner_or_surface`: `runtime_execution/contracts/*` 与 `runtime_execution/application/*` 仅依赖 canonical owner public，不再依赖 workflow `siligen_domain` / `siligen_motion_core` / `domain/*`。
- `illegal_pattern`: downstream module still depends on workflow compat surface
- `suggested_disposition`: `retarget`
- `acceptance_check`: `runtime-execution` 不再链接 `siligen_domain` / `siligen_motion_core`，其 contracts 不再包含 workflow `domain/*` 头。
- `blocking_risk`: workflow clean-exit 会被 `runtime-execution` 反向锁死，形成 public surface 回流。

### WF-R026
- `residue_id`: `WF-R026`
- `status`: `confirmed`
- `residue_type`: `concrete-backflow`
- `severity`: `high`
- `current_path`: `modules/workflow/adapters/infrastructure/adapters/planning/dxf/CMakeLists.txt`; `modules/workflow/adapters/infrastructure/adapters/planning/dxf/*`
- `current_symbol_or_target`: `siligen_parsing_adapter`
- `observed_fact`: `siligen_parsing_adapter` 直接编译 `PbPathSourceAdapter.cpp`、`AutoPathSourceAdapter.cpp`、`DXFAdapterFactory.cpp`，并生成 / 链接 DXF protobuf 代码。
- `why_it_is_residue`: DXF/PB parsing concrete 仍落在 workflow/adapters，而 `dxf-geometry` 已声明为 canonical owner。
- `expected_owner_or_surface`: `modules/dxf-geometry` 或相邻 engineering owner 的 canonical adapter/application surface，不应继续位于 workflow/adapters。
- `illegal_pattern`: workflow owns foreign concrete
- `suggested_disposition`: `move`
- `acceptance_check`: workflow/adapters 不再编译 DXF/PB parsing concrete。
- `blocking_risk`: engineering-data / DXF owner 无法真正切离 workflow，adapter build graph 继续回流。

### WF-R027
- `residue_id`: `WF-R027`
- `status`: `confirmed`
- `residue_type`: `test-boundary-drift`
- `severity`: `high`
- `current_path`: `modules/workflow/tests/canonical/CMakeLists.txt`
- `current_symbol_or_target`: `siligen_unit_tests`, `siligen_pr1_tests`, `siligen_dispensing_semantics_tests`, `workflow_motion_runtime_assembly_test`, `workflow_deterministic_path_execution_test`
- `observed_fact`: `tests/canonical/CMakeLists.txt` 仍把 `domain/process-core/include`、`domain/motion-core/include`、`modules/runtime-execution/application/include`、`modules/runtime-execution/contracts/runtime/include`、`modules/motion-planning`、`modules/job-ingest/contracts/include` 加入 include roots，并链接 `siligen_dispense_packaging_application_public`、`siligen_runtime_execution_application_public`、`siligen_runtime_execution_motion_execution_services`、`siligen_job_ingest_application_public`、`siligen_dxf_geometry_application_public`。
- `why_it_is_residue`: workflow canonical tests 继续以 foreign owner public / runtime concrete 做单测与语义测，暴露出真实边界尚未切净。
- `expected_owner_or_surface`: workflow tests 只依赖 workflow canonical public / contract；foreign owner 行为由各 owner 自测或仓库级跨 owner smoke / 端到端证明。
- `illegal_pattern`: tests rely on foreign owner concrete to validate workflow
- `suggested_disposition`: `split`
- `acceptance_check`: workflow canonical tests 不再引入 process-core/motion-core include 根，也不再把 runtime/dxf/job-ingest public 当作 workflow 单测依赖。
- `blocking_risk`: 测试层会持续掩盖 owner 漂移，并在后续重构时产生大面积伪回归。

### WF-R028
- `residue_id`: `WF-R028`
- `status`: `confirmed`
- `residue_type`: `doc-contract-drift`
- `severity`: `high`
- `current_path`: `modules/workflow/README.md`; `modules/workflow/module.yaml`; `modules/workflow/domain/CMakeLists.txt`; `modules/workflow/application/CMakeLists.txt`
- `current_symbol_or_target`: README / module notes claiming shell-only and direct-dependency removal
- `observed_fact`: `README.md` 声称 `src/` 与 `process-runtime-core/` 已完成 shell-only bridge 收敛，`domain/CMakeLists.txt` 与 `application/CMakeLists.txt` 仅允许 bridge-only 聚合；`module.yaml` 也写明 workflow 已移除对 `M6/M7/M8 application facade` 的直接依赖。实际 `domain/process-core`、`domain/motion-core` 仍编译 real source，`application/CMakeLists.txt` 仍 `add_subdirectory` / `target_link_libraries` 到 `motion-planning/application`、`dispense-packaging/application`、`runtime-execution/application` 等 foreign public。
- `why_it_is_residue`: 文档和模块声明口径比真实 build graph 更“干净”，属于明确的 contract drift。
- `expected_owner_or_surface`: README / module.yaml / CMake 注释应与真实 build graph 一致，不得提前宣告 bridge-only closeout。
- `illegal_pattern`: doc statement conflicts with live build graph
- `suggested_disposition`: `needs-further-trace`
- `acceptance_check`: README、module.yaml、CMake 注释与真实 target / include / link 关系一致，不再宣告未完成的 bridge-only closeout。
- `blocking_risk`: 团队会误判 residue 已清空，后续 clean-exit 与 owner closeout 容易被错误放行。

## 4.2 Suspected residues

### WF-R029
- `residue_id`: `WF-R029`
- `status`: `suspected`
- `residue_type`: `reverse-dependency`
- `severity`: `medium`
- `current_path`: `scripts/migration/validate_workspace_layout.py`
- `current_symbol_or_target`: `workflow application owner wiring missing`; `forbidden_reverse_mutations`
- `observed_fact`: 校验脚本仍要求 `modules/workflow/application/CMakeLists.txt` 出现 `siligen_job_ingest_application_public`、`siligen_dxf_geometry_application_public`、`siligen_runtime_execution_application_public`，同时保留对 `job-ingest`、`dxf-geometry`、`runtime-execution/application` 反向修改 workflow target 的专项检查。
- `why_it_is_residue`: 脚本口径显示 workflow 仍被当作 foreign owner wiring hub；但本轮未发现当前 live CMake 中存在明确的反向 target mutation 语句，因此只记为 suspected。
- `expected_owner_or_surface`: closeout 校验不再要求 workflow 维持 foreign owner wiring，也不再为历史 reverse mutation 通道保留结构性前提。
- `illegal_pattern`: other modules still expected to mutate or inject workflow target
- `suggested_disposition`: `needs-further-trace`
- `acceptance_check`: 校验脚本不再把 foreign owner wiring 视作 workflow/application 的必需条件，也不再为 reverse mutation 保留历史豁免。
- `blocking_risk`: 反向依赖通道可能被重新引入，或 closeout 工具持续固化错误结构。

### WF-R030
- `residue_id`: `WF-R030`
- `status`: `suspected`
- `residue_type`: `include-leak`
- `severity`: `medium`
- `current_path`: `modules/workflow/domain/domain/system/**`; `modules/workflow/domain/domain/planning/**`
- `current_symbol_or_target`: legacy compat directories `system`, `planning`
- `observed_fact`: `domain/domain/system/CMakeLists.txt` 与 `domain/domain/planning/CMakeLists.txt` 都声明自己是 migration-phase legacy/compat 聚合目录；同时 canonical `recovery` 与 `planning-boundary` 头仍回指这些旧路径。
- `why_it_is_residue`: 说明旧 `system` / `planning` 物理轴仍可能存在未完全追到的 include consumers；但本轮只确认了 `IRedundancyRepositoryPort` 与 `ISpatialIndexPort` 两个直接别名点，未全量证实所有外部消费点。
- `expected_owner_or_surface`: `recovery` / `planning-boundary` / `supervision` canonical public surface，不再依赖旧 `domain/system/*` / `domain/planning/*`。
- `illegal_pattern`: non-canonical include surface still active
- `suggested_disposition`: `needs-further-trace`
- `acceptance_check`: repo 内不再存在通过旧 `domain/system/*` / `domain/planning/*` 获取 live 语义的消费者。
- `blocking_risk`: 目录看似已切到新骨架，但残存 include 可能在后续重排时集中爆炸。

### WF-R031
- `residue_id`: `WF-R031`
- `status`: `suspected`
- `residue_type`: `owner-drift`
- `severity`: `medium`
- `current_path`: `modules/workflow/application/engineering_preview_tools/**`; `modules/workflow/application/engineering_trajectory_tools/**`; `modules/workflow/application/engineering_simulation_tools/**`
- `current_symbol_or_target`: offline engineering Python tools under `workflow/application`
- `observed_fact`: 这些 Python 文件生成 preview artifact、trajectory artifact、simulation payload，并直接处理 engineering-data protobuf / path bundle / preview 产物。
- `why_it_is_residue`: 从文件内容看，它们更像 `dxf-geometry` / `topology-feature` / `motion-planning` 的离线 engineering 工具，而非 `M0 workflow` orchestration；但本轮未追完整调用链与入口归属，因此记为 suspected。
- `expected_owner_or_surface`: 对应 engineering canonical owner surface，而不是 `modules/workflow/application`。
- `illegal_pattern`: workflow owns foreign concrete
- `suggested_disposition`: `needs-further-trace`
- `acceptance_check`: 这些 offline engineering 工具的入口 owner 被明确冻结，且不再误落到 workflow/application。
- `blocking_risk`: workflow 目录继续吸收离线工程语义，长期维护会再次把 online/offline 边界混回一起。

### WF-R032
- `residue_id`: `WF-R032`
- `status`: `suspected`
- `residue_type`: `build-graph-residue`
- `severity`: `medium`
- `current_path`: `legacy-exit-check.ps1`; `ci.ps1`; `scripts/migration/legacy_fact_catalog.json`
- `current_symbol_or_target`: legacy exit path rules for two deleted workflow historical roots
- `observed_fact`: legacy exit 入口与 fact catalog 当前聚焦的是 bridge 根路径存在性、旧根路径引用和 bridge metadata；catalog 里仍能看到两组已删除 workflow 历史根的路径规则，但未出现针对 `siligen_domain`、`siligen_process_core`、`siligen_motion_core`、`siligen_workflow_*_public` 的 target 级 matcher。
- `why_it_is_residue`: 这说明 exit gate 对“目录干净但 build graph 仍脏”的 residue 覆盖不足；但本轮未执行 gate 并比对实际报告，因此只记为 suspected。
- `expected_owner_or_surface`: clean-exit gate 应覆盖 compat target、public bundle、include leak 等 target-level residue，而不只检查物理桥接根。
- `illegal_pattern`: exit gate does not cover target-level build residue
- `suggested_disposition`: `needs-further-trace`
- `acceptance_check`: exit gate 能直接阻断 `siligen_domain`、`siligen_process_core`、`siligen_motion_core`、`siligen_workflow_*_public` 这类 live compat residue。
- `blocking_risk`: CI 可能在 bridge 根物理删除后仍放行脏 build graph，形成错误的 clean-exit 结论。

# 5. Residue clustering view

## 5.1 By residue type
- `build-graph-residue`: `WF-R001`, `WF-R004`, `WF-R017`, `WF-R032`
- `compat-target`: `WF-R002`, `WF-R003`
- `shell-only-violation`: `WF-R005`
- `concrete-backflow`: `WF-R006`, `WF-R008`, `WF-R009`, `WF-R026`
- `owner-drift`: `WF-R007`, `WF-R010`, `WF-R011`, `WF-R012`, `WF-R013`, `WF-R031`
- `naming-misleading`: `WF-R014`, `WF-R022`, `WF-R023`
- `include-leak`: `WF-R015`, `WF-R016`, `WF-R030`
- `deprecated-header-with-real-semantics`: `WF-R020`, `WF-R021`
- `reverse-dependency`: `WF-R025`, `WF-R029`
- `test-boundary-drift`: `WF-R027`
- `doc-contract-drift`: `WF-R028`

## 5.2 By directory
- `repo root build / gate`: `WF-R001`, `WF-R032`
- `modules/workflow/domain/process-core`: `WF-R002`
- `modules/workflow/domain/motion-core`: `WF-R003`
- `modules/workflow/domain/CMakeLists.txt` and `modules/workflow/domain/domain/**`: `WF-R004`, `WF-R005`, `WF-R006`, `WF-R007`, `WF-R008`, `WF-R009`, `WF-R020`, `WF-R021`, `WF-R022`, `WF-R023`, `WF-R030`
- `modules/workflow/application/**`: `WF-R010`, `WF-R011`, `WF-R012`, `WF-R013`, `WF-R014`, `WF-R015`, `WF-R031`
- `modules/workflow/CMakeLists.txt`: `WF-R017`
- `modules/workflow/adapters/**`: `WF-R026`
- `modules/workflow/tests/canonical/**`: `WF-R027`
- `modules/workflow/README.md` + `module.yaml`: `WF-R028`
- `apps/*` consumers: none in current active residue set (`WF-R024` 已收口)
- `modules/runtime-execution/**`: `WF-R025`
- `scripts/migration/**`: `WF-R029`, `WF-R032`

## 5.3 By priority
- `P0`: `WF-R001`, `WF-R002`, `WF-R003`, `WF-R004`, `WF-R005`, `WF-R006`, `WF-R013`, `WF-R015`, `WF-R017`, `WF-R025`
  - 阻断 clean-exit：root / downstream 仍依赖 compat aggregate，workflow 仍编译 foreign concrete，公共 bundle 仍脏。
- `P1`: `WF-R007`, `WF-R008`, `WF-R009`, `WF-R010`, `WF-R011`, `WF-R012`, `WF-R016`, `WF-R020`, `WF-R021`, `WF-R026`, `WF-R029`, `WF-R031`
  - 导致 owner 不清：machine / recipe / safety / valve / system / motion / DXF concrete 仍在 workflow，外部消费者仍粘在旧 surface。
- `P2`: `WF-R022`, `WF-R023`, `WF-R027`, `WF-R030`, `WF-R032`
  - 导致长期维护混乱：新骨架仍依赖旧 public path，测试与 gate 都会继续掩盖结构残留。
- `P3`: `WF-R014`, `WF-R028`
  - 文档 / 命名层残留：名称和说明比真实 build graph 更干净，容易制造假完成感。

# 6. Key findings
- 两组已删除的 workflow 历史根已经不在，但 build graph 没有同步干净。root CMake、`siligen_domain`、`siligen_process_core`、`siligen_motion_core` 仍把 workflow 当 legacy `process-runtime-core` 提供者。
- workflow 仍实际承载大量 foreign concrete：dispensing execution、recipe domain/application、safety、machine、system、motion、valve、DXF parsing concrete 都还在 workflow 内编译。
- public surface 仍未完全迁走，但当前残留已收敛到 `recovery` / `planning-boundary` 等仍回指旧 `domain/system` / `domain/planning` 路径的区域；`planning-trigger` / `phase-control` 的 legacy `usecases/dispensing` wrapper 与 workflow planning export compat 头已在本批次删除。
- 外部消费者的 app 侧旧 surface 已完成 cutover；当前仍需治理的是 `runtime-execution` 对 `siligen_domain` / `siligen_motion_core` 与 workflow `domain/*` 的反向依赖。
- tests 与 docs 都比真实状态更干净。`tests/canonical` 继续依赖 foreign owner public/runtime concrete；README / module.yaml / bridge-only 注释提前宣告 closeout。

# 7. Clean-exit blockers
- `siligen_process_core`、`siligen_motion_core`、`siligen_domain` 仍是 live build graph 关键节点；只删物理旧根不足以宣告 clean-exit。
- `siligen_application_motion`、`siligen_application_system`、`siligen_recipe_core`、`siligen_valve_core` 等 workflow application target 仍承载 foreign concrete，owner 无法冻结。
- `siligen_workflow_application_headers`、`siligen_workflow_domain_headers`、`siligen_module_workflow` 仍把 foreign owner include / link surface 伪装成 workflow public surface。
- `runtime-execution` 与 apps 仍反向依赖 workflow compat surface；workflow 无法单边完成 clean-exit。
- `tests/canonical` 与 current gate 口径都不足以证明“目录看似干净但 build graph 仍脏”的 residue 已被消除。

# 8. Unknowns requiring further trace
- `scripts/migration/validate_workspace_layout.py` 是否仍在为历史 reverse mutation / foreign wiring 保留强制结构要求，需要结合实际校验报告进一步确认。
- `domain/domain/system/**` 与 `domain/domain/planning/**` 是否还有本轮未命中的旧 include consumers，需要扩大 repo 级 include graph 扫描。
- `workflow/application/engineering_*_tools/**` 的真实入口 owner 和调用链尚未冻结；当前只从文件语义看更像 offline engineering owner residue。
- legacy exit / CI gate 是否已经出现“桥接根删除了，但 target-level residue 仍放行”的 false green，需要实际执行 gate 并比对报告。

# 9. Completion verdict
`ledger-partial-with-gaps`
