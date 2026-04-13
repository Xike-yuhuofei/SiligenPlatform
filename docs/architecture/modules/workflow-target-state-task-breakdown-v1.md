# workflow target-state task breakdown v1

本文基于：

- [workflow-target-state-v1](./workflow-target-state-v1.md)
- [workflow-target-state-mapping-v1](./workflow-target-state-mapping-v1.md)

把 `modules/workflow/` 的目标态重构拆成可执行、可验证、依赖有序的任务波次。

本文只负责任务拆分，不直接给实现细节。

## 任务分解清单

### Task 1
- 名称：M0 contracts split and export freeze
- 目标：把 `workflow/contracts` 从单头聚合口径收敛为 `commands / queries / events / dto / failures`，并切断 `siligen_workflow_application_headers` 作为外部主入口的角色。
- 输入：
  - `dsp-e2e-spec` 的 `S05/S06/S07`
  - [workflow-target-state-v1](./workflow-target-state-v1.md)
  - 当前 `modules/workflow/contracts/include/workflow/contracts/WorkflowContracts.h`
- 输出：
  - 新的 `workflow/contracts` 文件骨架
  - `WorkflowContracts.h` 聚合头
  - `siligen_workflow_contracts_public` 的目标导出规则
- 依赖：无
- 验证方式：
  - `python -m pytest tests/contracts/test_bridge_exit_contract.py -q`
  - `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\validation\assert-module-boundary-bridges.ps1`
  - `.\build.ps1 -Suite contracts`
- 停止条件：
  - 外部不再需要依赖 `siligen_workflow_application_headers` 才能获取 M0 稳定契约
  - `workflow/contracts` 不再承载 planning/execution specific request/response

### Task 2
- 名称：M0 domain extraction
- 目标：从 `domain/` 中显式建立 `WorkflowRun`、`StageTransitionRecord`、`RollbackDecision` 及其 policies/ports，同时冻结 `rollback_target`、`WorkflowRunState` 等核心枚举。
- 输入：
  - `S03/S04/S07`
  - [workflow-target-state-v1](./workflow-target-state-v1.md)
  - 当前 `domain/include/domain/recovery/**`
- 输出：
  - `domain/workflow_run/**`
  - `domain/stage_transition/**`
  - `domain/rollback/**`
  - `domain/policies/**`
  - `domain/ports/**`
- 依赖：
  - Task 1
- 验证方式：
  - `ctest --test-dir .\build -C Debug -R "WorkflowContracts|workflow" --output-on-failure`
  - 针对新 domain model 的单元测试
- 停止条件：
  - `workflow/domain` 中已经存在完整的 M0 owner 模型
  - `WorkflowRun` 与 `RollbackDecision` 的枚举不再借用旧 `WorkflowStageLifecycle`

### Task 3
- 名称：M0 services/application/runtime skeleton implementation
- 目标：把 `services/` 从 shell-only 实化为 `lifecycle / rollback / archive / projection`，并建立 `application/commands|queries|facade` 与 `runtime/orchestrators|subscriptions` 的空骨架。
- 输入：
  - Task 1 输出
  - Task 2 输出
  - 当前 `services/README.md`
- 输出：
  - `services/**`
  - `application/commands/**`
  - `application/queries/**`
  - `application/facade/**`
  - `runtime/**`
- 依赖：
  - Task 1
  - Task 2
- 验证方式：
  - `.\build.ps1 -Suite contracts,apps`
  - `ctest --test-dir .\build -C Debug -R "workflow" --output-on-failure`
- 停止条件：
  - `services/` 不再是空壳
  - `runtime/` 只承载 M0 orchestration，不包含 M9 execution state machine

### Task 4
- 名称：foreign redundancy governance exit
- 目标：确认当前 `recovery-control`、`domain/include/domain/recovery/redundancy` 与 redundancy adapter 属于代码冗余治理 residue，而不是 M0 rollback owner；将其从 `workflow` 的 canonical build/test/module bundle 中退出。
- 输入：
  - 当前 `application/recovery-control/**`
  - 当前 `application/usecases/redundancy/**`
  - 当前 `domain/include/domain/recovery/redundancy/**`
  - 当前 `adapters/infrastructure/adapters/redundancy/**`
  - Task 2 与 Task 3 输出
- 输出：
  - `siligen_application_redundancy` / `siligen_redundancy_storage_adapter` 退出 `workflow` canonical graph
  - 删除旧 `usecases/redundancy` 包装与 recovery/redundancy residue surface
- 依赖：
  - Task 2
  - Task 3
- 验证方式：
  - `python -m pytest tests/contracts/test_bridge_exit_contract.py -q`
  - repo 搜索确认 `modules/workflow` 不再包含 `CandidateScoringService` / `RedundancyGovernanceUseCases` / `JsonRedundancyRepositoryAdapter`
- 停止条件：
  - `CandidateScoringService`、`RedundancyGovernanceUseCases`、`JsonRedundancyRepositoryAdapter` 不再属于 `modules/workflow` live build/test surface
  - `workflow` rollback 轴不再复用 code redundancy governance 语义

### Task 5
- 名称：workflow application foreign-surface exit
- 目标：把 `application/planning-trigger`、`phase-control`、`ports/dispensing`、`services/dispensing` 这些非 M0 语义面从 `workflow` 清出。
- 输入：
  - 当前 `application/planning-trigger/**`
  - 当前 `application/phase-control/**`
  - 当前 `application/ports/dispensing/**`
  - 当前 `application/services/dispensing/**`
  - [workflow-target-state-mapping-v1](./workflow-target-state-mapping-v1.md)
- 输出：
  - `workflow/application` 只剩 command/query/facade
  - planning / preview / execution chain 相关 surface 退出 M0
- 依赖：
  - Task 1
  - Task 3
- 验证方式：
  - repo 搜索确认 `modules/workflow/application` 不再暴露 planning/execution live usecase
  - `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\validation\assert-module-boundary-bridges.ps1`
  - 受影响 consumer 的定向 build
- 停止条件：
  - `siligen_application_dispensing` 不再由 `modules/workflow/application/CMakeLists.txt` 定义
  - `application/include/application/**` 和 `application/include/workflow/application/**` 的 dual-root residue 不再是 public surface

### Task 6
- 名称：workflow domain foreign-concrete exit
- 目标：把 `domain/domain/dispensing`、`domain/domain/motion`、`domain/include/domain/{motion,dispensing,safety,machine}` 全部从 M0 终态中退出。
- 输入：
  - 当前 `domain/domain/**`
  - 当前 `domain/include/domain/**`
  - [workflow-target-state-mapping-v1](./workflow-target-state-mapping-v1.md)
- 输出：
  - `workflow/domain` 中只剩 M0 owner model
  - foreign concrete 与 compat headers 全部迁出或删除
- 依赖：
  - Task 2
  - Task 5
- 验证方式：
  - `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\validation\assert-module-boundary-bridges.ps1`
  - `python -m pytest tests/contracts/test_bridge_exit_contract.py -q`
  - 相关 peer module 的 targeted build / tests
- 停止条件：
  - `workflow/domain` 下不再存在 motion/dispensing/safety/machine concrete
  - `domain/domain` 彻底失去长期 owner 语义

### Task 7
- 名称：tests realignment to M0-only verification
- 目标：把 `workflow/tests` 收敛为只验证 M0 owner 行为，迁出所有 planning/preview/execution 语义测试。
- 输入：
  - 当前 `tests/canonical/**`
  - 当前 `tests/integration/**`
  - 当前 `tests/regression/**`
  - [workflow-target-state-mapping-v1](./workflow-target-state-mapping-v1.md)
- 输出：
  - M0 contract/unit/integration/regression 四层最小测试集
  - foreign owner tests 迁回各自 owner
- 依赖：
  - Task 4
  - Task 5
  - Task 6
- 验证方式：
  - `ctest --test-dir .\build -C Debug -R "workflow" --output-on-failure`
  - `.\test.ps1 -Profile Local -Suite contracts`
- 停止条件：
  - `tests/` 中不再存在 planning request、preview/start job、dispensing mode 等 foreign owner 断言
  - `workflow/tests` 的失败面只指向 M0 own behavior

### Task 8
- 名称：module-root target and CMake convergence
- 目标：把 `siligen_module_workflow`、`siligen_workflow_domain_public`、`siligen_workflow_contracts_public`、`siligen_workflow_adapters_public` 收敛成真正的 M0 canonical bundle。
- 输入：
  - 当前 `modules/workflow/CMakeLists.txt`
  - 当前 `application/domain/adapters/tests` 的新结构
- 输出：
  - 新的 module-root target graph
  - 移除 `siligen_workflow_application_headers` 与 workflow 内非 M0 payload target
- 依赖：
  - Task 1 至 Task 7
- 验证方式：
  - `.\build.ps1 -Suite all`
  - `.\test.ps1 -Profile Local -Suite all`
  - `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\validation\run-local-validation-gate.ps1`
- 停止条件：
  - `siligen_module_workflow` 不再静默透传 foreign payload
  - `modules/workflow` 的 build graph 与 target-state 目录结构一致

## 串并行关系

### 串行

- `Task 1 -> Task 2 -> Task 3`
- `Task 3 -> Task 5 -> Task 6 -> Task 7 -> Task 8`

### 并行

- `Task 4` 可以在 `Task 2` 与 `Task 3` 完成后，与 `Task 5` 并行推进
- `Task 7` 可在 `Task 6` 后半程并行清理测试，但不得早于 `Task 5`

## 高风险任务

- `Task 5`
  - 原因：当前 `application/planning-trigger` 与 `phase-control` 承接 live 交互链，错误切分最容易把 M0/M9/M11 边界重新搅混。
- `Task 6`
  - 原因：`domain/domain/motion` 与 `domain/domain/dispensing` 里混有 M7/M8/M9 residue，最容易出现多 owner 和回流。
- `Task 8`
  - 原因：CMake target 收口是最终真值；前面目录改对了但 target graph 没收口，结构仍然是假 clean。

## 建议下一步

- 推荐进入：`task-execute`
- 如果在真正动代码前还要继续收边界，建议先补一份 `change-scope-guard`，把每个 Task 的允许编辑路径和禁止触碰区再冻结一层
