# Historical note
- 本文件保留一份早期 residue baseline，供治理追溯使用，不应再被当作当前唯一真值。
- 当前 canonical truth：
  - recipe family owner = `modules/recipe-lifecycle`
  - recipe JSON serializer surface = `modules/recipe-lifecycle/adapters/include/recipe_lifecycle/adapters/serialization/RecipeJsonSerializer.h`
  - `IEventPublisherPort` owner = `shared/contracts/runtime/include/runtime/contracts/system/IEventPublisherPort.h`
  - generic diagnostics sink owner = `modules/trace-diagnostics/contracts/include/trace_diagnostics/contracts/{IDiagnosticsPort,DiagnosticTypes}.h`
  - hardware-test diagnostics live landing = `apps/runtime-service/include/runtime_process_bootstrap/diagnostics/**` (app-local quarantine surface)
- 若下文出现 `runtime_process_bootstrap/recipes/RecipeJsonSerializer.h`、`siligen_workflow_recipe_*` 或 workflow-owned event seam 作为“当前 owner”的表述，应按历史证据理解。

# 1. Audit scope
- **主目录**：`modules/workflow`
- **构建图边界**：`modules/workflow/CMakeLists.txt` 及内部各层的 CMakeLists (domain, application, adapters, tests)
- **文档契约声明**：`modules/workflow/README.md`, `S2A_OWNER_MIGRATION_BASELINE.md`, 各子目录 README
- **核心审查焦点**：识别本应属于已迁出模块（motion, dispensing, process-path, dxf 等）却依然以静态库、头文件依赖、测试边界、或 facade 壳层形式附着在 workflow 内的真实 payload 残留。

# 2. Frozen audit assumptions
1. `modules/workflow` (M0) 仅负责工作流编排与阶段推进语义、规划链触发、以及流程状态事实归属。
2. workflow 不应承载已迁出模块（M4-M8）的模块内事实实现或 concrete 业务代码。
3. `process-runtime-core/` 与 `src/` 等历史目录如继续存在，只能作为 shell-only 或 deprecated bridge，严禁编译真实 `.h/.cpp` 载荷。
4. `domain/`、`application/`、`adapters/`、`tests/` 若包含其他模块领域或应用层逻辑，则属于直接违规。

# 3. Residue identification criteria
- **`shell-only-violation` / `bridge-payload`**：声明仅起桥接作用的层，内部仍存在参与编译的有效 `.cpp` 源文件。
- **`concrete-backflow`**：本应归属下层具体领域的逻辑实现，被放置并编译在 workflow 领域空间内。
- **`owner-drift`**：应用层用例（UseCases）、基础适配器（Adapters）被放置在了错误的主从模块归属关系下。
- **`build-graph-residue`**：为了兼容旧代码调用，CMake 提供了一个巨型 alias/compat target 打包所有分离逻辑，掩盖了重构。
- **`include-leak`**：Workflow 自身的 public interface 向外部无差别暴露其内部依赖的其他领域头文件根路径。
- **`test-boundary-drift`**：测试目标测试了非自身边界的行为。
- **`doc-contract-drift`**：文档已宣告代码重构完成或已作 shell 化收敛，但构建图事实与之矛盾。

# 3.1 Current rebaseline (`2026-04-09`)
- `siligen_workflow_domain_headers` 当前只导出 workflow 自己的 public include root，不再暴露 motion-planning / shared compat include roots。
- `domain/supervision/ports/IEventPublisherPort.h` 已是独立 canonical surface；旧 `domain/system/ports/IEventPublisherPort.h` live consumer 已归零，compat shim 已删除。
- `siligen_domain` 已于 round `18` 从 `modules/workflow/domain/CMakeLists.txt` 删除；workflow 对外当前只保留 canonical `siligen_workflow_domain_headers` / `siligen_workflow_domain_public`，因此 `WF-R009` / `WF-B08` 已完成当前 family-close。
- repo-wide 非文档源码检索下，`modules/runtime-execution/**` 当前不再直接链接 `siligen_domain` / `siligen_motion_core`，也未再命中 live `#include "domain/*"`；round `18` 已将 `WF-B02` 的 formal family-close 写回，后续主线不再把 runtime-execution reverse-dep 视为 active blocker。
- `recipe serialization` app-cross-boundary 批次已完成：authority artifact 当前收口到 `modules/recipe-lifecycle/adapters/include/recipe_lifecycle/adapters/serialization/RecipeJsonSerializer.h` + `modules/recipe-lifecycle/adapters/serialization/RecipeJsonSerializer.cpp`，`apps/runtime-service` / `apps/runtime-gateway` live consumer 已全部 retarget，workflow 旧 producer 与 `siligen_workflow_recipe_serialization_public` 已删除。
- `diagnostics` app-cross-boundary 批次已完成：generic diagnostics sink 当前收口到 `modules/trace-diagnostics/contracts/include/trace_diagnostics/contracts/{IDiagnosticsPort,DiagnosticTypes}.h`；hardware-test diagnostics contracts 当前仍停留在 `apps/runtime-service/include/runtime_process_bootstrap/diagnostics/**` 这组 app-local quarantine surface。
- `recipe serialization` 当前已从 workflow residue ledger 中退出：`modules/recipe-lifecycle` 承担 `RecipeJsonSerializer` surface，workflow 不再保留该 producer/compat target；recipe family 的后续 concrete closeout 已在 round `20` 完成。
- recipe family closeout 已于 round `20` 完成：`modules/workflow/domain/process-core/**`、`modules/workflow/domain/domain/recipes/**` 与 `modules/workflow/application/usecases/recipes/**` 已退出 live build graph；`planner-cli`、`runtime-service`、`runtime-gateway` live consumer 与 focused tests 当前均已收口到 `modules/recipe-lifecycle`。
- dispensing concrete closeout 已于 round `21` 完成当前最小切片：`modules/workflow/domain/domain/CMakeLists.txt` 已不再定义 `siligen_triggering` 或编译 `dispensing/domain-services/PositionTriggerController.cpp`；triggering concrete 的 canonical live owner 当前固定为 `modules/dispense-packaging/domain/dispensing`。
- round `21` revalidation 已确认 `siligen_runtime_execution_unit_tests`、`siligen_runtime_host_unit_tests`、`siligen_motion_planning_unit_tests`、contract test 与 boundary gate 通过；`siligen_runtime_service_unit_tests` 当前仍被 `apps/runtime-service` 既有 motion port duplicate-definition blocker 卡住，该问题位于 `ApplicationContainerFwd.h` / `InfrastructureBindings.h` 所在 app 级链路，不属于本轮 `WF-B10` scope。
- `modules/workflow/domain/domain/**` 当前物理目录只剩 `_shared/`、`motion/`、`geometry/`、`dispensing/`；旧 `WF-B14` 口径中的 `domain/domain/machine/**` 已不再存在，因此该 scope 只能继续作为历史证据保留。
- fresh configure graph `D:/Projects/SiligenSuite/build-wf-machine-freeze/workflow-target-graph.dot` 已确认：`domain_machine`、`siligen_trajectory` 与 `siligen_configuration` 当前都无 incoming edge；round `23` 已从 workflow live build graph 删除这三组 dead shell。
- machine/calibration 的 workflow-side public-surface residue 已完成当前 closeout：`domain/include/domain/machine/**` tracked payload 已清零；`DispenserModel` / `CalibrationProcess` / `IHardwareConnectionPort` / `IHardwareTestPort` / `MachineMode` / `MachineState` 均已退出 workflow live/public surface，machine 相关剩余命中只保留在 contract guard 与历史文档中。
- `diagnostics` 当前的 canonical split 为：
  - `generic diagnostics sink`：`modules/trace-diagnostics/contracts` owner，当前被 workflow planning、`runtime-execution` 与 `apps/runtime-service` bootstrap 共用。
  - `hardware-test diagnostics contracts`：`apps/runtime-service/include/runtime_process_bootstrap/diagnostics/**` app-local quarantine surface；当前 live include 已收敛到 bootstrap/container 注册链与 app-local headers 自身；workflow 旧 diagnostics public headers 已删除，且本轮未把该组 contracts 并入 `trace-diagnostics`。
- `build-wf-diagnostics` 串行验证已确认 generic diagnostics sink cutover 当前不是 active blocker：`siligen_runtime_execution_unit_tests`、`siligen_runtime_host_unit_tests`、`siligen_runtime_service_unit_tests` 与 `siligen_dispensing_semantics_tests` 均已完成构建，相关定向测试通过。
- safety family closeout 已于 round `19` 完成：`modules/workflow/domain/motion-core/CMakeLists.txt` 与 `modules/workflow/domain/domain/safety/**` 已退出 live build graph；`siligen_runtime_service_unit_tests` 当前真实 blocker 已收敛为 `ITaskSchedulerPort` contract 头自包含缺口并已修复；targeted safety tests 与 boundary gate 已重新转绿。
- repo 级非文档源码检索下，`siligen_control_application` / `siligen_application` 当前已无 live 命中；workflow application live target 已收敛到 `siligen_application_dispensing` 与 `siligen_application_redundancy`。
- `siligen_application_motion` 的当前 live owner target 位于 `modules/runtime-execution/application/CMakeLists.txt`；workflow 侧 `application/execution-supervision/runtime-consumer/MotionRuntimeAssemblyFactory.cpp` 与 `application/usecases/motion/trajectory/DeterministicPathExecutionUseCase.cpp` 当前不存在。
- round `25` 已完成 `WF-B03` 当前最小 producer/export slice：`siligen_workflow_application_headers` 已删除 `${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}`，`PlanningPortAdapters.h` 的 foreign application facade re-export 也已在后续轮次收口；`siligen_application_dispensing` 已显式 `PUBLIC` 链接 `siligen_process_path_contracts_public`、`siligen_motion_planning_contracts_public`、`siligen_shared_runtime_contracts` 与 `siligen_runtime_execution_runtime_contracts`。
- round `30` 已完成 `WF-B03` 当前最小 keep-set tighten slice：`siligen_application_dispensing` 的 `siligen_utils` / `siligen_geometry` 与 `siligen_application_redundancy` 的 `siligen_utils` 已从 `PUBLIC` 降为 `PRIVATE`；`siligen_types` 继续作为稳定 `PUBLIC` surface 保留。
- round `31` 已完成 `WF-B03` 当前最小 redundancy public-surface tighten slice：`siligen_application_redundancy` 已不再把 `siligen_workflow_application_headers` 暴露为 `PUBLIC` 依赖；其 `PUBLIC` surface 当前只保留 `siligen_types` 与 `siligen_workflow_domain_public`。
- round `32` 已完成 `WF-B03` 当前最小 keep-set necessity freeze slice：`tests/contracts/test_bridge_exit_contract.py` 当前已精确冻结 `siligen_workflow_application_headers` 的 4 条 `INTERFACE` keep-set 与 `siligen_application_dispensing` 的 11 条 `PUBLIC` keep-set；workflow README / residue ledger / execution status 已同步回写为“public-surface 必需项冻结”真值。后续若要继续下收，必须单开 public header surface refactor 批次。
- workflow canonical tests 当前已完成最小 consumer tighten：`siligen_unit_tests` / `siligen_pr1_tests` 已不再依赖 `siligen_workflow_application_public` aggregate，而改为 direct link 最小 planning/redundancy owner targets；`ctest -R "siligen_(dispensing_semantics_tests|unit_tests|pr1_tests)"` 已通过。
- `build-wf-b04-04a-tests-serial` 下的串行 `/FS` 验证已确认 `siligen_application_dispensing`、`siligen_application_redundancy`、`siligen_dispensing_semantics_tests`、`siligen_unit_tests`、`siligen_pr1_tests`、`siligen_runtime_process_bootstrap`、`siligen_transport_gateway` 与 `siligen_planner_cli` 当前均构建通过，且未恢复 workflow application header leak。
- `build-wf-b04-04a-tests-serial` 下的串行 `/FS` 重新验证已确认 `siligen_motion_planning_unit_tests` 当前构建通过，且 `ctest -R siligen_motion_planning_unit_tests` 通过；早期 round `10` owner-boundary 失败已不再构成当前 blocker 证据。
- round `26` 已完成 `WF-B17` 当前最小 live-app consumer retarget：`apps/runtime-service`、`apps/runtime-gateway` 与 `apps/planner-cli` 当前都已把 `siligen_workflow_application_public` 替换为 `siligen_application_dispensing`，且 [modules/workflow/application/CMakeLists.txt](/D:/Projects/SiligenSuite/modules/workflow/application/CMakeLists.txt) 已把 `siligen_workflow_contracts_public` / `siligen_workflow_domain_public` 补入 `siligen_application_dispensing` 的 `PUBLIC` deps。
- `build-wf-b04-04a-tests-serial` 下的 round `26` 串行 `/FS` 重新验证已确认 `siligen_application_dispensing`、`siligen_transport_gateway`、`siligen_planner_cli`、`siligen_dispensing_semantics_tests`、`siligen_unit_tests` 与 `siligen_pr1_tests` 继续通过；`rg -n "siligen_workflow_application_public" apps/runtime-service/CMakeLists.txt apps/runtime-gateway/transport-gateway/CMakeLists.txt apps/planner-cli/CMakeLists.txt` 当前 0 命中。
- 同一轮中，`siligen_runtime_process_bootstrap` 当前失败于 `modules/runtime-execution/adapters/device/src/adapters/dispensing/dispenser/triggering/TriggerControllerAdapter.cpp(3,10)` 对缺失路径 `adapters/diagnostics/health/testing/HardwareTestAdapter.h` 的 include；仓库内实际 header 位于 `modules/runtime-execution/adapters/device/include/siligen/device/adapters/hardware/HardwareTestAdapter.h`。该阻塞属于 `modules/runtime-execution/**` / device-adapters 现有 dirty-worktree 问题，不并入本轮 workflow retarget scope。
- round `27` 已完成 `WF-B18` 当前最小 workflow test-consumer cleanup：`modules/workflow/tests/canonical/CMakeLists.txt` 与 `modules/workflow/tests/integration/CMakeLists.txt` 当前都已不再依赖 `siligen_workflow_application_public` aggregate，且 `rg -n "siligen_workflow_application_public" modules/workflow/tests/canonical/CMakeLists.txt modules/workflow/tests/integration/CMakeLists.txt` 当前 0 命中。
- `build-wf-b04-04a-tests-serial` 下的 round `27` 串行 `/FS` 重新验证已确认 `siligen_dispensing_semantics_tests`、`siligen_unit_tests` 与 `siligen_pr1_tests` 继续通过；`ctest -R "siligen_(dispensing_semantics_tests|unit_tests|pr1_tests)"`、`python -m pytest tests/contracts/test_bridge_exit_contract.py -q` 与 `scripts/validation/assert-module-boundary-bridges.ps1` 当前都已转绿。
- round `28` 已完成 `WF-B05` 当前最小 module-root aggregate / naming slice：`modules/workflow/CMakeLists.txt` 当前已删除 `siligen_workflow_application_public`，并把 `siligen_module_workflow` 重冻为显式 M0 canonical bundle（`workflow contracts/domain/application/adapters` + `LINK_ONLY` application payload）。
- round `29` 已完成 application/domain public-surface 前置收紧：`modules/workflow/application/include/application/ports/dispensing/PlanningPortAdapters.h` 当前已改为 declaration-only public header，不再 re-export `ProcessPathFacade.h`、`MotionPlanningFacade.h` 或 `DxfPbPreparationService.h`；`apps/runtime-service/container/ApplicationContainer.Dispensing.cpp` 当前已显式包含 `ProcessPathFacade.h`，`siligen_runtime_process_bootstrap` 也已显式链接 `siligen_process_path_application_public`。
- round `29` 已完成 `WF-B04` 的 `planning-boundary` first slice：`modules/workflow/domain/include/domain/planning-boundary/ports/ISpatialIndexPort.h` 已删除，canonical header 当前位于 `modules/dispense-packaging/domain/dispensing/planning/ports/ISpatialIndexPort.h`；`modules/dispense-packaging/domain/dispensing/domain-services/PathOptimizationStrategy.h` 与 workflow dormant residue 对应 header 已全部 retarget 到新路径。
- `D:/Projects/SiligenSuite/build` 下的 round `29` 串行 `/FS` 重新验证已确认 `siligen_application_dispensing`、`siligen_runtime_process_bootstrap`、`siligen_transport_gateway`、`siligen_planner_cli`、`siligen_dispensing_semantics_tests`、`siligen_unit_tests` 与 `siligen_pr1_tests` 构建通过；`ctest --test-dir D:/Projects/SiligenSuite/build -C Debug -R "siligen_(dispensing_semantics_tests|unit_tests|pr1_tests)" --output-on-failure` 与 `assert-module-boundary-bridges.ps1` 通过。完整 `tests/contracts/test_bridge_exit_contract.py -q` 当前仍有 1 个既有失败，落在 `modules/dxf-geometry/tests/README.md` 文案断言；本轮新增的 2 个 workflow 断言已独立通过。
- 当前 `siligen_workflow_application_public` 在 `modules/**` / `apps/**` / `tests/**` 的 live 非文档命中已归零；剩余字符串仅保留在 validation / contract guard 与历史文档中，作为 reintroduction 证据。
- 本 ledger 下文的 residue family 与 close-exit blocker 仍保留为长期治理真值；若与本节冲突，以本节的当前事实为准。

# 4. Workflow residue ledger

## 4.1 Confirmed residues

- **`residue_id`**：WF-R001
- **`status`**：resolved
- **`residue_type`**：shell-only-violation
- **`severity`**：critical
- **`current_path`**：`deleted modules/workflow/domain/process-core`
- **`current_symbol_or_target`**：`deleted siligen_process_core`
- **`observed_fact`**：round `20` 已删除 `modules/workflow/domain/process-core/CMakeLists.txt` 及其配套 recipe concrete 文件；repo-wide 非文档源码检索下，当前未再发现 live code 命中 `siligen_process_core`。
- **`why_it_is_residue`**：该 residue 已在 round `20` 收口；workflow 不再保留 process-core compat bridge，也不再承担 recipe concrete 的物理承载。
- **`expected_owner_or_surface`**：recipe owner = `modules/recipe-lifecycle`；其他非 recipe 语义按各自 canonical owner 承接
- **`illegal_pattern`**：bridge carries real payload
- **`suggested_disposition`**：keep-deleted / guard-against-reintroduction
- **`acceptance_check`**：`modules/workflow/domain/process-core/CMakeLists.txt` 保持删除；`siligen_process_core` 不得回流；recipe family targeted build/test 与 boundary gate 持续通过。
- **`blocking_risk`**：若 process-core bridge 回流，会重新制造 workflow 对 recipe owner 的 false-clean 污染。

- **`residue_id`**：WF-R002
- **`status`**：resolved
- **`residue_type`**：shell-only-violation
- **`severity`**：critical
- **`current_path`**：`modules/workflow/domain/motion-core`
- **`current_symbol_or_target`**：`deleted siligen_motion_core`
- **`observed_fact`**：round `19` 已删除 `modules/workflow/domain/motion-core/CMakeLists.txt` 与 `src/safety/services/interlock_policy.cpp`；boundary gate 当前同时要求 `motion-core` CMake 维持删除状态，并对 `siligen_motion_core` 施加 exact-word 回流拦截。
- **`why_it_is_residue`**：该 residue 已在 round `19` 收口；workflow motion-core 不再持有 live safety payload，也不再保留 compat target。
- **`expected_owner_or_surface`**：`modules/runtime-execution/application` safety services + `modules/motion-planning` public surface；workflow `motion-core` 仅允许 archive/seed-only README 痕迹
- **`illegal_pattern`**：bridge carries real payload
- **`suggested_disposition`**：keep-deleted / archive-only
- **`acceptance_check`**：`modules/workflow/domain/motion-core/CMakeLists.txt` 保持删除；repo-wide 非文档源码检索不再命中 `siligen_motion_core`；safety family boundary gate 持续通过。
- **`blocking_risk`**：若 `motion-core` target 或 safety payload 回流，会重新打开 workflow 对 runtime-owned safety 的 false-clean 污染。

- **`residue_id`**：WF-R003
- **`status`**：resolved
- **`residue_type`**：concrete-backflow
- **`severity`**：high
- **`current_path`**：`modules/workflow/domain/domain/CMakeLists.txt` (Dispensing 领域)
- **`current_symbol_or_target`**：`deleted siligen_triggering`
- **`observed_fact`**：round `21` 已从 `modules/workflow/domain/domain/CMakeLists.txt` 删除 `siligen_triggering`，并移除 `dispensing/domain-services/PositionTriggerController.cpp` 的 live 编译入口；boundary gate 当前同时阻断 `siligen_triggering` 与 `PositionTriggerController.cpp` 回流。
- **`why_it_is_residue`**：该 residue 已在 round `21` 收口；workflow bridge-domain 不再编译 dispensing triggering concrete，live owner 已回收到 M8 canonical target。
- **`expected_owner_or_surface`**：`modules/dispense-packaging/domain/dispensing`
- **`illegal_pattern`**：workflow owns foreign concrete
- **`suggested_disposition`**：keep-dormant / guard-against-reintroduction
- **`acceptance_check`**：`modules/workflow/domain/domain/CMakeLists.txt` 不再命中 `siligen_triggering` 或 `PositionTriggerController.cpp`；repo-wide 非文档源码检索不再命中 live build files 精确词 `siligen_triggering`；targeted build/test 与 boundary gate 持续通过。
- **`blocking_risk`**：若 `siligen_triggering` 或 `PositionTriggerController.cpp` 回流到 workflow build graph，会重新制造 dispensing triggering 的双 owner。

- **`residue_id`**：WF-R004
- **`status`**：resolved
- **`residue_type`**：concrete-backflow
- **`severity`**：high
- **`current_path`**：`deleted modules/workflow/domain/domain/recipes`
- **`current_symbol_or_target`**：`deleted siligen_recipe_domain_services`
- **`observed_fact`**：round `20` 已删除 `modules/workflow/domain/domain/recipes/**`，且 `modules/workflow/domain/domain/CMakeLists.txt` 不再命中 `add_subdirectory(recipes)` 或 `siligen_recipe_domain_services`；recipe live domain services 当前收口到 `modules/recipe-lifecycle/domain`。
- **`why_it_is_residue`**：该 residue 已在 round `20` 收口；workflow bridge-domain 不再编译 recipe concrete。
- **`expected_owner_or_surface`**：`modules/recipe-lifecycle/domain`
- **`illegal_pattern`**：workflow owns foreign concrete
- **`suggested_disposition`**：keep-deleted / guard-against-reintroduction
- **`acceptance_check`**：`modules/workflow/domain/domain/recipes/**` 保持删除；repo-wide 非文档源码检索不再命中 `siligen_recipe_domain_services`；recipe family targeted build/test 持续通过。
- **`blocking_risk`**：若 workflow recipe concrete 回流，会重新形成 recipe family 双 owner。

- **`residue_id`**：WF-R005
- **`status`**：resolved
- **`residue_type`**：concrete-backflow
- **`severity`**：medium
- **`current_path`**：`modules/workflow/domain/domain/CMakeLists.txt` (Diagnostics / Safety 领域)
- **`current_symbol_or_target`**：deleted workflow diagnostics/safety concrete slice
- **`observed_fact`**：generic diagnostics sink 已于 round `15` 收口到 `modules/trace-diagnostics/contracts`，workflow 旧 diagnostics public headers 已删除；round `19` 又确认 `modules/workflow/domain/domain/CMakeLists.txt` 不再命中 `siligen_safety_domain_services`，且 `modules/workflow/domain/domain/safety/**` 已物理删除。
- **`why_it_is_residue`**：该 residue 已跨 round `15` 与 round `19` 收口；workflow bridge-domain 不再编译 diagnostics / safety concrete，剩余 workflow `domain/safety/**` 仅保留 compat surface。
- **`expected_owner_or_surface`**：diagnostics = `modules/trace-diagnostics/contracts`；safety = `modules/runtime-execution/application`；workflow 仅保留必要 compat headers
- **`illegal_pattern`**：workflow owns foreign concrete
- **`suggested_disposition`**：keep-deleted / compat-only
- **`acceptance_check`**：workflow bridge CMake 不再代理编译 diagnostics / safety concrete；`siligen_runtime_execution_unit_tests` safety filter、`siligen_runtime_host_unit_tests` limit-monitor filter、`siligen_runtime_service_unit_tests` 与 safety family gate 持续通过。
- **`blocking_risk`**：若 workflow 重新编译 diagnostics / safety concrete，会再次穿透 owner 边界并使 runtime-execution safety closeout 失真。

- **`residue_id`**：WF-R006
- **`status`**：confirmed
- **`residue_type`**：owner-drift
- **`severity`**：high
- **`current_path`**：`modules/workflow/application/usecases/motion`
- **`current_symbol_or_target`**：`siligen_application_motion`
- **`observed_fact`**：包含 `ManualMotionControlUseCase`, `HomeAxesUseCase`, `InterpolationPlanningUseCase` 等运动控制和插补的直接业务交互实现。
- **`why_it_is_residue`**：README 已明确说明“motion / interpolation owner 已冻结到 M7”，这类具体用例的归属者应当是 M7 的 Application facade 层，而不是继续赖在 M0 里。
- **`expected_owner_or_surface`**：`modules/motion-planning/application`
- **`illegal_pattern`**：application facade owns foreign usecases
- **`suggested_disposition`**：move
- **`acceptance_check`**：将 M7 特有用例移除至 M7 模块中，M0 application 仅余下对 motion 服务的高阶组装编排。
- **`blocking_risk`**：导致外部消费者想调用一个基础运动控制 API 也需要被迫依赖整个 workflow。

- **`residue_id`**：WF-R007
- **`status`**：confirmed
- **`residue_type`**：owner-drift
- **`severity`**：high
- **`current_path`**：`modules/workflow/application/usecases/dispensing`
- **`current_symbol_or_target`**：`siligen_application_dispensing`
- **`observed_fact`**：包含 `usecases/dispensing/valve/coordination/ValveCoordinationUseCase.cpp`。
- **`why_it_is_residue`**：阀门级别协调属于设备级或点胶域（M8）核心控制流。
- **`expected_owner_or_surface`**：`modules/dispense-packaging/application`
- **`illegal_pattern`**：application facade owns foreign usecases
- **`suggested_disposition`**：move
- **`acceptance_check`**：阀门级别用例代码不再存放在 workflow。
- **`blocking_risk`**：同 WF-R006。

- **`residue_id`**：WF-R008
- **`status`**：confirmed
- **`residue_type`**：owner-drift
- **`severity`**：medium
- **`current_path`**：`modules/workflow/adapters/infrastructure/adapters/planning`
- **`current_symbol_or_target`**：`siligen_parsing_adapter`, `siligen_contour_augment_adapter`
- **`observed_fact`**：在工作流中持有对 DXF 文件解析和几何轮廓增广的具体适配器实现。
- **`why_it_is_residue`**：DXF 读取与处理通常完全归属于 dxf-geometry 或 process-path，M0 工作流无权处理具体解析算法的适配层。
- **`expected_owner_or_surface`**：`modules/dxf-geometry` 或 `modules/process-path`
- **`illegal_pattern`**：workflow owns foreign infrastructure adapters
- **`suggested_disposition`**：move
- **`acceptance_check`**：DXF解析与几何增强适配器相关代码从 workflow 树中被抽离。
- **`blocking_risk`**：几何解析器耦合在编排层，造成依赖包膨胀（比如无故引入图形库或CAD解析库）。

- **`residue_id`**：WF-R009
- **`status`**：resolved
- **`residue_type`**：build-graph-residue
- **`severity`**：high
- **`current_path`**：`modules/workflow/domain/CMakeLists.txt`
- **`current_symbol_or_target`**：`siligen_domain`
- **`observed_fact`**：round `18` 已从 `modules/workflow/domain/CMakeLists.txt` 删除 `siligen_domain` compat target；repo-wide 精确词检索下，当前未再发现 `modules/**` / `apps/**` / `tests/**` 的 live build files 命中 `siligen_domain`。
- **`why_it_is_residue`**：该 residue 在 round `18` 已收口；后续若 `siligen_domain` 回流，将直接重新构成 target-level clean-exit 污染。
- **`expected_owner_or_surface`**：workflow 对外仅保留 canonical `siligen_workflow_domain_public`；不得重新引入 `siligen_domain` compat target。
- **`illegal_pattern`**：legacy compat target remains live after consumer unlink
- **`suggested_disposition`**：keep-deleted / guard-against-reintroduction
- **`acceptance_check`**：目标保持删除状态；boundary gate 能机械阻断 `siligen_domain` 回流。
- **`blocking_risk`**：若该 target 回流，会重新模糊 workflow 的真实 clean-exit 完成度，并削弱 validator 对 compat 回流的阻断能力。

- **`residue_id`**：WF-R010
- **`status`**：confirmed
- **`residue_type`**：build-graph-residue
- **`severity`**：high
- **`current_path`**：`modules/workflow/application/CMakeLists.txt`
- **`current_symbol_or_target`**：`siligen_control_application` (alias `siligen_application`)
- **`observed_fact`**：将所有的运动用例、系统用例、点胶用例和阀门核心统一打包为一个目标并允许外部使用别名链接。
- **`why_it_is_residue`**：同 WF-R009，掩盖了应用层早已按领域切分的现实情况，阻止了上层消费者的架构治理。
- **`expected_owner_or_surface`**：拆分解耦的各独立 public target
- **`illegal_pattern`**：compat target hides application boundary drift
- **`suggested_disposition`**：deprecate-shell
- **`acceptance_check`**：不再存在 `siligen_application` 统一目标，强制业务消费者链接最小 facade。
- **`blocking_risk`**：业务线依赖退化为“引用大全”，难以摘除任何特定领域。

- **`residue_id`**：WF-R011
- **`status`**：partially-resolved
- **`residue_type`**：include-leak
- **`severity`**：medium
- **`current_path`**：`modules/workflow/CMakeLists.txt` 及 `application/CMakeLists.txt`
- **`current_symbol_or_target`**：`siligen_workflow_domain_headers`, `siligen_workflow_application_headers`
- **`observed_fact`**：`siligen_workflow_domain_headers` 当前已只暴露 workflow 自身 include root；`siligen_workflow_application_headers` 已删除 `${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}`，且 workflow public headers 不再隐式 re-export `ProcessPathFacade.h`、`MotionPlanningFacade.h` 或 `DxfPbPreparationService.h`，但当前仍保留少量 foreign contract `INTERFACE` links。round `26/27` 又确认 live app consumer 与 workflow canonical/integration tests 已不再依赖 `siligen_workflow_application_public`；round `28` 进一步删除了 [modules/workflow/CMakeLists.txt](/D:/Projects/SiligenSuite/modules/workflow/CMakeLists.txt) 中的 `siligen_workflow_application_public`，并把 `siligen_module_workflow` 重冻为显式 canonical bundle；round `30` 又把 `siligen_application_dispensing` / `siligen_application_redundancy` 的 shared-kernel helper keep-set 从 `PUBLIC` 收紧为 `PRIVATE`；round `31` 再把 `siligen_application_redundancy` 从 `siligen_workflow_application_headers` 的 `PUBLIC` 暴露链上解开；round `32` 则把 header bundle 剩余 4 条 `INTERFACE` links 与 `siligen_application_dispensing` 剩余 11 条 `PUBLIC` keep-set 精确冻结到 contract guard，默认不再继续做无 public-header 重构前提的削减。
- **`why_it_is_residue`**：模块自身作为 `INTERFACE` 提供时将毫不相干的外部目录泄漏出去，引发隐式包含成功的问题。
- **`expected_owner_or_surface`**：按需包含，杜绝隐式传递。
- **`illegal_pattern`**：non-canonical include surface still active
- **`suggested_disposition`**：freeze / defer-public-header-refactor
- **`acceptance_check`**：工作流头文件目标只暴露自身的 public include；`tests/contracts/test_bridge_exit_contract.py` 必须继续精确冻结 `siligen_workflow_application_headers` 的 4 条 `INTERFACE` keep-set 与 `siligen_application_dispensing` 的 11 条 `PUBLIC` keep-set；`siligen_application_dispensing` / `siligen_application_redundancy` 不得再把 `siligen_utils` 或 `siligen_geometry` 作为 `PUBLIC` keep-set 泄漏，且 `siligen_application_redundancy` 不得再把 `siligen_workflow_application_headers` 暴露为 `PUBLIC` 依赖；外部 app / test consumer 不再依赖 `siligen_workflow_application_public` 获取 foreign owner surface，且 `modules/workflow/CMakeLists.txt` 不再定义该 aggregate。若要继续减少 remaining keep-set，必须进入独立 public header surface refactor 批次。
- **`blocking_risk`**：若 aggregate surface 继续承载隐式依赖，修改无关 owner 模块头路径仍可能意外打破并未显式声明依赖的 consumer 编译。

- **`residue_id`**：WF-R012
- **`status`**：confirmed
- **`residue_type`**：test-boundary-drift
- **`severity`**：medium
- **`current_path`**：`modules/workflow/tests/unit/CMakeLists.txt`
- **`current_symbol_or_target`**：`workflow_owner_unit_tests`
- **`observed_fact`**：单元测试明确包含 `MotionOwnerBehaviorTest.cpp` 并强依赖所有外部 application facade。
- **`why_it_is_residue`**：运动的 owner behavior 应属于 motion 模块；且如果需要启动近乎全部模块的 facade，则这不是轻量级的 M0 unit test 而是边界模糊的混合测试。
- **`expected_owner_or_surface`**：`modules/motion-planning/tests` 或集成测试层。
- **`illegal_pattern`**：tests test foreign module behaviors
- **`suggested_disposition`**：move
- **`acceptance_check`**：该单元测试剥离所有外围强依赖，并迁走与 motion 本质相关的测试。
- **`blocking_risk`**：测试维护成本飙升，后续重构引发大面积虚假阻断。

- **`residue_id`**：WF-R013
- **`status`**：confirmed
- **`residue_type`**：doc-contract-drift
- **`severity`**：low
- **`current_path`**：`modules/workflow/README.md`
- **`current_symbol_or_target`**：文本内容 “已完成 shell-only bridge 收敛”
- **`observed_fact`**：声明与真实的 process-core/motion-core 存在源文件并参与编译的事实不符。
- **`why_it_is_residue`**：给出了误导性的“代码已清理”的虚假承诺。
- **`expected_owner_or_surface`**：无（修正文档事实或尽快清空代码）
- **`illegal_pattern`**：doc statement conflicts with codebase facts
- **`suggested_disposition`**：update
- **`acceptance_check`**：架构宣告与真实残留台账完全一致。
- **`blocking_risk`**：团队产生虚假安全感而放弃后续清理。

## 4.2 Suspected residues

- **`residue_id`**：WF-R014
- **`status`**：suspected
- **`residue_type`**：concrete-backflow
- **`severity`**：medium
- **`current_path`**：`modules/workflow/application/execution-supervision/runtime-consumer/MotionRuntimeAssemblyFactory.cpp`
- **`current_symbol_or_target`**：`siligen_application_motion`
- **`observed_fact`**：该装配工厂存在于 workflow 的 consumer 内。
- **`why_it_is_residue`**：规范要求 runtime concrete 的实例化仅存在于 runtime-execution。如果该文件存在对具体 class （而非接口）的 `new/make_shared`，则构成侵权；若是存粹的接口转接与下发，则可合法存在。
- **`expected_owner_or_surface`**：`modules/runtime-execution`
- **`illegal_pattern`**：workflow owns foreign concrete (suspected)
- **`suggested_disposition`**：needs-further-trace
- **`acceptance_check`**：审查该工厂文件内是否具有依赖物理实现的注入逻辑。
- **`blocking_risk`**：破坏基于服务的反转控制链。

# 5. Residue clustering view

说明：`WF-R001`、`WF-R002`、`WF-R003`、`WF-R004`、`WF-R005`、`WF-R009` 已 resolved，编号继续保留仅用于追溯执行历史。

## 5.1 By residue type
- **`shell-only-violation` / `bridge-payload`**：WF-R001, WF-R002
- **`concrete-backflow`**：WF-R003, WF-R004, WF-R005, WF-R014
- **`owner-drift`**：WF-R006, WF-R007, WF-R008
- **`build-graph-residue` / `compat-target`**：WF-R009, WF-R010
- **`include-leak`**：WF-R011
- **`test-boundary-drift`**：WF-R012
- **`doc-contract-drift`**：WF-R013

## 5.2 By directory
- **`domain/`**：WF-R001, WF-R002, WF-R003, WF-R004, WF-R005, WF-R009
- **`application/`**：WF-R006, WF-R007, WF-R010, WF-R014
- **`adapters/`**：WF-R008
- **`tests/`**：WF-R012
- **根目录构建/文档**：WF-R011, WF-R013

## 5.3 By priority
- **P0（阻断 clean-exit 的核心 payload）**：WF-R006, WF-R007
- **P1（导致 owner 边界不清的设施遗留）**：WF-R008, WF-R014 (周边基础设施与算法混居)
- **P2（导致长期维护混乱的图谱污染）**：WF-R010, WF-R011, WF-R012 (大包依赖、隐式包含传递和脆弱单元测试)
- **P3（文档命名层残留）**：WF-R013

# 6. Key findings
1. **workflow child concrete 已继续收缩**：`siligen_domain` 已于 round `18` 退场，safety family 已于 round `19` 收口，recipe family 于 round `20` 退出 workflow bridge-domain / application / process-core 三层，dispensing concrete 又于 round `21` 完成 closeout。
2. **bridge-domain root dead shell 已退出 live build graph**：fresh configure graph 已确认 `domain_machine`、`siligen_trajectory`、`siligen_configuration` 无 incoming edge；round `23` 已删除这些 workflow wrapper target，`domain/domain/CMakeLists.txt` 当前仅保留 dormant placeholder。
3. **machine residue 已从 workflow public surface 退出**：`domain/domain/machine/**` 与 `domain/include/domain/machine/**` 当前都不再承载 tracked live payload；machine 相关剩余命中集中在 contract guard 与历史文档，用于阻断回流。
4. **Application/Test 侧真值已继续前移**：`siligen_application_motion` 当前已落到 `modules/runtime-execution/application`，`siligen_control_application` / `siligen_application` 已无 live 命中；round `26` 完成外部 live app consumer retarget，round `27` 又完成 workflow canonical/integration tests 脱离 aggregate，round `28` 进一步删除了 `siligen_workflow_application_public` 并把 `siligen_module_workflow` 收敛为 canonical bundle，round `29` 又把 `PlanningPortAdapters.h` 从 inline payload 头收敛为 declaration-only surface，round `30` 再把 shared-kernel helper 的 `PUBLIC` keep-set 收紧为 `PRIVATE`，round `31` 又把 `siligen_application_redundancy` 从 header bundle 的 `PUBLIC` 暴露链上解开，round `32` 则把剩余 4 条 `INTERFACE` 与 11 条 `PUBLIC` keep-set 冻结为当前 public-surface 必需项；进一步收紧已不再属于当前低成本 slice。

# 7. Clean-exit blockers
1. application 侧当前真正活跃的阻塞已从 module-root public-surface / naming drift 进一步收敛到 `WF-B03` 残余 trace：live app consumer retarget 与 workflow test-consumer cleanup 已在 round `26/27` 收口，round `28` 已删除 `siligen_workflow_application_public` 并把 `siligen_module_workflow` 收敛为显式 canonical bundle，round `29` 已清掉 `PlanningPortAdapters.h` 的外部 application header re-export，round `30` 已完成 shared-kernel `PUBLIC` keep-set 收紧，round `31` 又已完成 redundancy public-surface 去耦，round `32` 则把 remaining keep-set 冻结为 contract-guarded truth。
2. `siligen_application` 宏 target 已退出 live code；后续 application 批次不应再把 alias、shared-kernel helper keep-set 或 `siligen_application_redundancy` header-bundle 去耦当主战场。若要继续减少 `siligen_workflow_application_headers` 剩余 foreign contract `INTERFACE` links 或 `siligen_application_dispensing` 其余 `PUBLIC` deps，必须单开 public header surface refactor 批次。

# 8. Unknowns requiring further trace
1. `BUILD_SECURITY_MODULE` 这条默认关闭的 root optional 分支，后续是否仍应继续消费 `siligen_module_workflow`，仍需等 security landing owner 与源码路径一并重冻；该问题当前不属于默认 workflow build graph slice。
2. `siligen_application_redundancy` 当前在 round `31` 已完成最小 public-surface 去耦；`siligen_application_dispensing` 与 `siligen_workflow_application_headers` 的 remaining keep-set 已在 round `32` 冻结为当前 public-surface 必需项。若后续仍要区分“编排层必需”与“历史 carry-over”，必须以前置 public header surface refactor 为准入条件，而不再作为当前低成本收紧问题继续推进。
3. apps 中仍然存活的 `workflow/application/planning-trigger/*` / `phase-control/*` canonical include，是否需要按 orchestration lane 再细化成文档级 owner 账本，仍待在后续批次补 trace；当前它们不再作为 `WF-R024` blocker。

# 9. Completion verdict
`ledger-partial-with-gaps`
*(注：本轮审查通过分析架构根节点和 CMake 树已深挖大量确切的物理文件和构图违规行为。但受限于静态构建代码推导，不排除某些被 `INTERFACE` 暴露出去的深层头文件、以及特定 `AssemblyFactory` 内的代码是否包含真正的 concrete instantiation 需进一步通过追溯编译中间产物才能完全定论，因此保守认定存在部分微观排查差距。)*
