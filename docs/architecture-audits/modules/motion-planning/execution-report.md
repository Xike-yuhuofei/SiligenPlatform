# motion-planning Execution Report

## 1. Execution Scope
- 本轮处理模块：`modules/motion-planning/`
- 读取的台账路径：`docs/architecture-audits/modules/motion-planning/residue-ledger.md`
- 本轮 batch 范围：`MP-R003` closeout，以及为 consumer rewiring 所需的最小文档、测试、build truth 对齐

## 2. Execution Oracle
- 本轮冻结的 batch 列表
  - 删除 `InterpolationProgramFacade` public residual
  - 把 3 个 live consumer 直接改接 `InterpolationProgramPlanner`
  - 收口 `README`、`module.yaml`、public-surface 测试与 owner-boundary 断言
  - 执行受影响构建与 closeout 主证据测试
- 每个 batch 的 acceptance criteria
  - `modules/motion-planning/application/` 不再存在 `InterpolationProgramFacade.h/.cpp`
  - `siligen_motion_planning_application_public` 不再因 facade 暴露 `runtime_execution_runtime_contracts`
  - `runtime-execution` 与 `dispense-packaging` 的 live consumer 直接调用 `InterpolationProgramPlanner`
  - README / `module.yaml` / 测试断言与 live truth 一致
- 本轮 no-change zone
  - 不改 `InterpolationProgramPlanner.cpp`、`InterpolationCommandValidator.cpp` 与任何规划算法实现
  - 不重开 `InterpolationData` owner 设计，不新增 façade / bridge / compat
  - 不扩到 `MotionControlService*`、execution control ports、`SemanticPath`
- 本轮测试集合
  - `cmake --build build --config Debug --target siligen_motion_planning_unit_tests siligen_motion_planning_architecture_tests siligen_motion_planning_integration_tests runtime_execution_deterministic_path_execution_test siligen_dispense_packaging_unit_tests siligen_dispense_packaging_boundary_tests siligen_dispense_packaging_workflow_seam_tests`
  - `ctest --test-dir build -C Debug -R "^siligen_motion_planning_(unit|architecture|integration)_tests$" --output-on-failure`
  - `ctest --test-dir build -C Debug -R "^runtime_execution_deterministic_path_execution_test$" --output-on-failure`
  - `ctest --test-dir build -C Debug -R "^siligen_dispense_packaging_(unit|boundary|workflow_seam)_tests$" --output-on-failure`

## 3. Change Log
- 修改了 `modules/motion-planning/application/CMakeLists.txt`
  - 删除 `InterpolationProgramFacade.cpp` 源项，并移除 application headers / application target 对 `siligen_runtime_execution_runtime_contracts` 的公开依赖
  - 这样改是为了让 facade 删除后，application public layer 回到只暴露 3 个 facade 的状态
  - 对应消除 residual：`MP-R003`、`MP-R001`
- 修改了 `modules/motion-planning/application/include/application/services/motion_planning/InterpolationProgramFacade.h`
  - 删除文件
  - 这样改是因为该 header 直接暴露 execution DTO，不再允许继续充当 M7 public surface
  - 对应消除 residual：`MP-R003`
- 修改了 `modules/motion-planning/application/services/motion_planning/InterpolationProgramFacade.cpp`
  - 删除文件
  - 这样改是因为该实现只是透传 `InterpolationProgramPlanner`，继续保留只会伪装成 canonical facade
  - 对应消除 residual：`MP-R003`
- 修改了 `modules/runtime-execution/application/usecases/motion/trajectory/DeterministicPathExecutionUseCase.cpp`
  - 移除对 `InterpolationProgramFacade` 的 include/using，直接实例化 `InterpolationProgramPlanner`
  - 这样改是因为 runtime-execution 是 live consumer，不应再经过 M7 public facade 绕行
  - 对应消除 residual：`MP-R003`
- 修改了 `modules/dispense-packaging/application/services/dispensing/PlanningAssemblyServices.cpp`
  - 移除 `InterpolationProgramFacade`，改为直接调用 `InterpolationProgramPlanner`
  - 这样改是因为本轮只做最小 consumer rewiring，不新增 wrapper
  - 对应消除 residual：`MP-R003`
- 修改了 `modules/dispense-packaging/domain/dispensing/planning/domain-services/DispensingPlannerService.cpp`
  - 移除 `InterpolationProgramFacade`，改为直接调用 `InterpolationProgramPlanner`
  - 这样改是为了关闭 M7 public facade 的最后一处 live domain consumer
  - 对应消除 residual：`MP-R003`
- 修改了 `modules/dispense-packaging/domain/dispensing/planning/domain-services/DispensingPlannerService.h`、`modules/dispense-packaging/domain/dispensing/planning/domain-services/ContourOptimizationService.h`
  - 把 `ProcessPath::Contracts::*` 的歧义限定名改成完整的 `Siligen::ProcessPath::Contracts::*`
  - 这样改是因为本轮 consumer rewiring 触发构建时暴露出同文件内 `using ProcessPath` 与 `ProcessPath::Contracts::*` 的名称冲突；这是完成本轮验证所需的最小构建阻塞修复
  - 对应消除 residual：本轮任务内构建阻塞，不新增新的架构 surface
- 修改了 `modules/motion-planning/README.md`、`modules/motion-planning/module.yaml`、`modules/motion-planning/domain/motion/README.md`
  - 删除对 `InterpolationProgramFacade` 的冻结 public surface 描述，补充 `InterpolationProgramPlanner` 仅是 internal residual seam
  - 这样改是为了对齐真相源，不再让 README / `module.yaml` 与 live build 打架
  - 对应消除 residual：`MP-R003`、`MP-R014`
- 修改了 `modules/motion-planning/tests/unit/application/services/motion_planning/MotionPlanningPublicSurfaceContractTest.cpp`
  - 删除 facade public-surface 测试和无用 helper
  - 这样改是因为 M7 public surface 已不再导出该对象
  - 对应消除 residual：`MP-R003`
- 修改了 `modules/motion-planning/tests/unit/domain/motion/MotionPlanningOwnerBoundaryTest.cpp`
  - 新增 facade 已删除断言，并把 `PlanningAssemblyServices.cpp` 纳入 `InterpolationProgramPlanner` consumer 断言
  - 这样改是为了把 closeout 证据收敛到“facade 已退场、consumer 已直连 internal seam”的当前事实
  - 对应消除 residual：`MP-R003`

## 4. Residual Disposition Table
| residue_id | file_or_target | disposition | resulting_location | reason |
| --- | --- | --- | --- | --- |
| MP-R003 | `InterpolationProgramFacade.h/.cpp` | deleted | 无 | facade 只剩透传价值，删除后由 live consumer 直接调用 `InterpolationProgramPlanner` |
| MP-R003 | `DeterministicPathExecutionUseCase.cpp` | migrated | 直接消费 `InterpolationProgramPlanner` | runtime-execution 不再经过 M7 public facade |
| MP-R003 | `PlanningAssemblyServices.cpp` | migrated | 直接消费 `InterpolationProgramPlanner` | dispense-packaging application consumer 直接接 internal seam |
| MP-R003 | `DispensingPlannerService.cpp` | migrated | 直接消费 `InterpolationProgramPlanner` | dispense-packaging domain consumer 直接接 internal seam |
| MP-R001 | `modules/motion-planning/application/CMakeLists.txt` | demoted | 仅保留 contracts + 3 facades 的 application public target | facade 删除后，application layer 不再公开携带 runtime contracts |
| MP-R014 | `README.md` / `module.yaml` / `domain/motion/README.md` | renamed | 与 live build 一致的 frozen wording | 文档删除旧 facade 叙述，说明 internal seam 的真实状态 |

## 5. Dependency Reconciliation Result
- `module.yaml` / `CMake` / `target_link_libraries` / public headers 是否已对齐
  - `modules/motion-planning/application/CMakeLists.txt` 不再编译 `InterpolationProgramFacade.cpp`
  - `siligen_motion_planning_application_headers` 与 `siligen_motion_planning_application` 不再公开链接 `siligen_runtime_execution_runtime_contracts`
  - README / `module.yaml` 不再把 `InterpolationProgramFacade` 认作 frozen public surface
- 还有哪些未清项
  - `siligen_motion` 仍私有依赖 `siligen_runtime_execution_runtime_contracts`，因为 `InterpolationProgramPlanner` / `InterpolationCommandValidator` 还直接使用 execution DTO
  - `dispense-packaging` 仍通过 target linkage 获取 `siligen_motion_planning_application_public`，因为同模块还在消费 `MotionPlanningFacade`
- 为什么本轮不能继续推进
  - 再往前将进入 `MotionControlService*`、execution control ports 与 `InterpolationData` owner 迁移，超出本轮冻结边界

## 6. Surface Reconciliation Result
- stable seam 是否统一
  - `motion-planning` public surface 现只剩 `MotionPlanningFacade`、`CmpInterpolationFacade`、`TrajectoryInterpolationFacade` 与 contracts
- 是否还存在 façade/provider/bridge 多套并存
  - 本轮关闭了 `InterpolationProgramFacade` 这条并行 surface
  - `InterpolationProgramPlanner` 仍存在，但被明确标注为 `domain/motion` internal residual seam，而非新的 public facade
- 是否仍有内部 stage 类型泄漏
  - application public layer 不再直接暴露 `InterpolationData`
  - domain internal seam 仍直接返回 `InterpolationData`，这属于下一阶段 residual，不在本轮展开

## 7. Test Result
- 本轮实际运行的测试
  - `cmake --build build --config Debug --target siligen_motion_planning_unit_tests siligen_motion_planning_architecture_tests siligen_motion_planning_integration_tests runtime_execution_deterministic_path_execution_test siligen_dispense_packaging_unit_tests siligen_dispense_packaging_boundary_tests siligen_dispense_packaging_workflow_seam_tests`
  - `ctest --test-dir build -C Debug -R "^siligen_motion_planning_(unit|architecture|integration)_tests$" --output-on-failure`
  - `ctest --test-dir build -C Debug -R "^runtime_execution_deterministic_path_execution_test$" --output-on-failure`
  - `ctest --test-dir build -C Debug -R "^siligen_dispense_packaging_(unit|boundary|workflow_seam)_tests$" --output-on-failure`
- 哪些通过
  - 上述构建目标全部通过
  - `siligen_motion_planning_unit_tests`、`siligen_motion_planning_architecture_tests`、`siligen_motion_planning_integration_tests` 通过
  - `runtime_execution_deterministic_path_execution_test` 通过
  - `siligen_dispense_packaging_unit_tests`、`siligen_dispense_packaging_boundary_tests`、`siligen_dispense_packaging_workflow_seam_tests` 通过
- 哪些失败
  - 无
- 失败属于任务内问题还是已知任务外阻塞
  - 初次整体验证前存在一个任务内构建阻塞：`DispensingPlannerService.h` / `ContourOptimizationService.h` 的 `ProcessPath::Contracts::*` 限定名与本地 `using ProcessPath` 冲突；已在本轮最小修复并复测通过

## 8. Remaining Blockers
- 架构 blocker
  - `InterpolationProgramPlanner` 仍以 execution DTO 作为 domain internal seam 输出
  - `MotionControlService*`、`MotionStatusService*`、`IAxisControlPort`、`IJogControlPort`、`IHomingPort` 仍停留在 M7 tree
  - `MotionTypes`、`HardwareTestTypes`、`TrajectoryAnalysisTypes` 仍是跨模块共享 owner 桶
  - `SemanticPath` 双 seam 尚未收口
- 构建 blocker
  - `siligen_motion` 仍私有依赖 runtime contracts
- 测试 blocker
  - repo-wide boundary tests 物理目录仍位于 `modules/motion-planning/tests/unit/domain/motion/`
- 文档 / 命名 blocker
  - `application/include/application/...` 与 `application/include/motion_planning/...` 的双命名 surface 仍未统一

## 9. Final Verdict
- `modules/motion-planning/` 本轮后是否更接近 canonical owner
  - 是。`InterpolationProgramFacade` 已退出 M7 public surface 与 live build，execution DTO 不再经由 application facade 外泄。
- 本轮是否达成 batch acceptance criteria
  - 是。facade 已删除，3 个 live consumer 已改接 `InterpolationProgramPlanner`，README / `module.yaml` / 测试断言与 live truth 对齐，受影响构建与 closeout 主证据测试全部通过。
- 下一批次最应该处理什么
  - 在不重开大范围重构的前提下，继续清 `MotionControlService*` / execution control ports，或重新冻结 `InterpolationData` 的最终 owner。
