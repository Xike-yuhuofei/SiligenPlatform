# hmi-application Execution Report

## 0. 2026-04-09 Startup/Build Addendum
- 本节为当前真值增补；若与下文历史记录冲突，以本节为准。
- 本轮新增 batch：
  - `MB2c startup split finish`
    - `hmi_application.services.launch_result_projection` 承接 `LaunchResult` / offline result / snapshot projection
    - `hmi_application.services.startup_sequence_service` 承接 startup/recovery sequence 装配与 `SupervisorSession` 委托
    - `hmi_application.adapters.startup_qt_workers` 承接 `StartupWorker` / `RecoveryWorker`
    - `hmi_application.services.startup_orchestration`、`hmi_application.adapters.qt_workers` 收缩为薄层 / re-export
  - `MB4b build topology shrink`
    - `apps/planner-cli/CMakeLists.txt` 已移除 `siligen_module_hmi_application` link
- 本轮额外更新：
  - `modules/hmi-application/CMakeLists.txt` owner assets 纳入新 startup files
  - `modules/hmi-application/tests/CMakeLists.txt` 纳入新的 startup owner tests
  - `modules/hmi-application/README.md`、`application/README.md`、`tests/README.md` 已同步 startup split 真值
- 本轮验证结果：
  - `python .\modules\hmi-application\tests\run_hmi_application_tests.py`：`56` 项通过
  - `python -m pytest .\apps\hmi-app\tests\unit\test_startup_sequence.py -q`：`11` 项通过
  - `python -m pytest .\apps\hmi-app\tests\unit\test_m11_owner_imports.py -q`：`11` 项通过
  - `python -m pytest .\apps\hmi-app\tests\unit\test_main_window.py -q`：`73` 项通过
  - `python -m pytest .\apps\hmi-app\tests\unit\test_smoke.py -q`：`1` 项通过
  - `python -m pytest .\apps\hmi-app\tests\unit\test_preview_session_worker.py -q`：`2` 项通过
  - `cmake -S . -B .\build` + `cmake --build .\build --target siligen_planner_cli --config Debug`：通过
- 当前剩余 blocker：
  - `preview_session.py` 仍是主 residual 容器
  - `siligen_module_hmi_application` 仍是 `INTERFACE` owner asset bucket，build topology 还未完全拆清
  - canonical `integration/regression` 测试面仍未补齐
- 因此，下文所有“`startup.py` 仍混装 QThread/runtime concrete”以及“`planner-cli` 仍链接 `siligen_module_hmi_application`”的表述，现已过时。

## 1. Execution Scope
- 本轮处理的模块：`modules/hmi-application/`
- 读取的台账路径：`docs/architecture-audits/modules/hmi-application/residue-ledger.md`
- 本轮 batch 范围：
  - `MB2a launch_supervision session split`
  - `MB2b startup truth reconciliation`
  - `MB3 contract cutover + compat cleanup`

## 2. Execution Oracle
- 本轮冻结的 batch 列表：
  - `MB2a launch_supervision session split`
    - acceptance criteria：`launch_supervision_session.py` 退化为 stable seam；protocol/type/helper 从单文件 residual 容器抽离到包内 `domain/services/adapters`。
  - `MB2b startup truth reconciliation`
    - acceptance criteria：`startup.py` 不再持有重复的 launch-mode authority；`README / module.yaml / CMake` 对 live in-package split 的叙述一致。
  - `MB3 contract cutover + compat cleanup`
    - acceptance criteria：live contract 物理落点切到 `hmi_application.contracts.launch_supervision_contract`；app 侧 `supervisor_*` compat 删除；`launch_supervision_*` 成为唯一 live naming。
- 本轮 no-change zone：
  - 不改 `SessionSnapshot`、`SessionStageEvent`、`FailureStage`、`FailureCode`、`RecoveryAction` 语义。
  - 不改 `preview_gate.py`、`preview_session.py`、`launch_state.py` 的既有行为。
  - 不改 `apps/hmi-app/src/hmi_client/ui/main_window.py` 的业务逻辑。
  - 不扩散到 `shared/`、`apps/planner-cli/` 实现。
- 本轮测试集合：
  - `python .\modules\hmi-application\tests\run_hmi_application_tests.py`
  - `python -m pytest .\apps\hmi-app\tests\unit\test_m11_owner_imports.py -q`
  - `python -m pytest .\apps\hmi-app\tests\unit\test_supervisor_contract.py -q`
  - `python -m pytest .\apps\hmi-app\tests\unit\test_supervisor_session.py -q`
  - `python -m pytest .\apps\hmi-app\tests\unit\test_startup_sequence.py -q`
  - `python -m pytest .\apps\hmi-app\tests\unit\test_main_window.py -q`
  - `python -m pytest .\apps\hmi-app\tests\unit\test_smoke.py -q`

## 3. Change Log
- `modules/hmi-application/application/hmi_application/contracts/__init__.py`
  - 新增包内 contract root 导出。
  - 这样改是为了给 `launch_supervision` contract 一个真实且可冻结的 live root。
  - 对应消除：`M11-SFC-002`。
- `modules/hmi-application/application/hmi_application/contracts/launch_supervision_contract.py`
  - 将原 `launch_supervision_contract.py` 的 live contract 全量迁入此处，语义保持不变。
  - 这样改是为了完成 contract relocation，而不是继续把 contract 留在 application root 伪装为 canonical。
  - 对应消除：`M11-SFC-002`、部分 `M11-DOC-001`。
- `modules/hmi-application/application/hmi_application/domain/__init__.py`
  - 新增包内 domain 导出。
  - 对应消除：部分 `M11-OWN-003`、部分 `M11-PLH-002`。
- `modules/hmi-application/application/hmi_application/domain/launch_supervision_types.py`
  - 新增 `SupervisorPolicy`、`StepResult`、stage 常量、timeout 默认值与 `normalize_launch_mode()`。
  - 这样改是为了把 launch/supervision 的纯类型与规则从 `launch_supervision_session.py` / `startup.py` 抽离。
  - 对应消除：部分 `M11-OWN-002`、部分 `M11-OWN-003`。
- `modules/hmi-application/application/hmi_application/adapters/launch_supervision_ports.py`
  - 新增 backend/TCP/hardware port protocol 与 `BackendController`。
  - 这样改是为了把 adapter port seam 从 `launch_supervision_session.py` 中剥离出来。
  - 对应消除：部分 `M11-OWN-003`、部分 `M11-PLH-004`。
- `modules/hmi-application/application/hmi_application/services/__init__.py`
  - 新增 services 子包导出。
  - 对应消除：部分 `M11-OWN-003`、部分 `M11-PLH-003`。
- `modules/hmi-application/application/hmi_application/services/launch_supervision_flow.py`
  - 新增 `LaunchSupervisionFlow`，承接启动/恢复状态机、事件发射、snapshot 发射与 backend/TCP/hardware concrete step。
  - 这样改是为了把 `launch_supervision_session.py` 从实现容器收缩为稳定 seam。
  - 对应消除：核心 `M11-OWN-003`。
- `modules/hmi-application/application/hmi_application/services/launch_supervision_runtime.py`
  - 新增运行态退化 helper：`build_runtime_failure_snapshot`、`detect_runtime_degradation`、`detect_runtime_degradation_with_event`。
  - 这样改是为了把 runtime degradation 逻辑从 `launch_supervision_session.py` 分离到包内 service helper。
  - 对应消除：部分 `M11-OWN-003`。
- `modules/hmi-application/application/hmi_application/launch_supervision_session.py`
  - 改写为 thin seam，只保留 `SupervisorSession` 稳定方法与静态 helper 委托。
  - 这样改是为了维持外部 API 不变，同时移除单文件 owner/service/adapter 混装。
  - 对应消除：核心 `M11-OWN-003`。
- `modules/hmi-application/application/hmi_application/startup.py`
  - 改为复用 `domain.launch_supervision_types` 的 `normalize_launch_mode`、默认 timeout 常量与 `SupervisorPolicy`，并直接消费 `adapters.launch_supervision_ports`。
  - 这样改是为了让 `startup.py` 只保留 launch API 与 `LaunchResult` authority，不再维护第二份 mode/timeout 真相。
  - 对应消除：部分 `M11-OWN-002`、部分 `M11-DOC-001`。
- `modules/hmi-application/application/hmi_application/launch_state.py`
  - contract import 切到 `hmi_application.contracts.launch_supervision_contract`。
  - 这样改是为了让 live code 不再依赖已删除的旧 contract 路径。
  - 对应消除：部分 `M11-SFC-002`。
- `modules/hmi-application/application/hmi_application/__init__.py`
  - package-root re-export 改为从 `contracts.launch_supervision_contract` 读取稳定 contract。
  - 这样改是为了保留 package root 稳定 seam，同时统一真实 contract root。
  - 对应消除：部分 `M11-SFC-001`、`M11-SFC-002`。
- `modules/hmi-application/CMakeLists.txt`
  - owner assets 纳入新的 in-package `contracts/domain/services/adapters` 文件，移除已删除的 `launch_supervision_contract.py`。
  - 这样改是为了让 build truth 跟实际 live Python payload 对齐。
  - 对应消除：部分 `M11-BLD-001`。
- `modules/hmi-application/README.md`
  - 更新为 stage-3 cleanup 口径，明确 live package split 已存在于 `application/hmi_application/contracts|domain|services|adapters/`。
  - 对应消除：部分 `M11-DOC-001`。
- `modules/hmi-application/application/README.md`
  - 更新 live Python owner 面，明确 `launch_supervision_session` 退化为 stable seam，`contracts/domain/services/adapters` 已成为 live package split。
  - 对应消除：部分 `M11-DOC-001`。
- `modules/hmi-application/tests/README.md`
  - contract test 描述更新到 `hmi_application.contracts.launch_supervision_contract`。
  - 对应消除：部分 `M11-DOC-001`。
- `modules/hmi-application/module.yaml`
  - notes 更新到 stage-3 cleanup 与包内 split 口径。
  - 对应消除：部分 `M11-DOC-001`。
- `modules/hmi-application/contracts/README.md`
  - 明确顶层 `contracts/` 仍是非 live 占位目录；当前 live contract 已在包内 `application/hmi_application/contracts/`。
  - 对应消除：部分 `M11-PLH-001`、部分 `M11-DOC-001`。
- `modules/hmi-application/tests/unit/test_launch_state.py`
  - owner test import 切到 `hmi_application.contracts.launch_supervision_contract`。
  - 对应消除：部分 `M11-TST-002`。
- `modules/hmi-application/tests/contract/test_launch_supervision_contract.py`
  - contract test import 切到新的 contract root。
  - 对应消除：部分 `M11-TST-002`。
- `apps/hmi-app/src/hmi_client/client/launch_supervision_contract.py`
  - thin wrapper 改为从 `hmi_application.contracts.launch_supervision_contract` re-export。
  - 这样改是为了在 app 侧保留 sanctioned import path，同时去掉对旧 owner 路径的依赖。
  - 对应消除：部分 `RB2 import-path convergence`。
- `apps/hmi-app/src/hmi_client/client/supervisor_contract.py`
  - 删除 app 侧旧命名 compat wrapper。
  - 对应消除：`RB1 naming convergence`、`DB1 compat cleanup`。
- `apps/hmi-app/src/hmi_client/client/supervisor_session.py`
  - 删除 app 侧旧命名 compat wrapper。
  - 对应消除：`RB1 naming convergence`、`DB1 compat cleanup`。
- `apps/hmi-app/tests/unit/test_m11_owner_imports.py`
  - 改为验证 `launch_supervision_session` wrapper 继续指向 owner，并断言 `supervisor_contract` / `supervisor_session` 已不存在。
  - 对应消除：部分 `RB1 naming convergence`。
- `apps/hmi-app/tests/unit/test_smoke.py`
  - 从 smoke candidate 中移除已删除的 `supervisor_contract.py` / `supervisor_session.py`。
  - 对应消除：部分 `RB1 naming convergence`。

## 4. Residual Disposition Table
| residue_id | file_or_target | disposition | resulting_location | reason |
| --- | --- | --- | --- | --- |
| M11-OWN-003 | `application/hmi_application/launch_supervision_session.py` | split | `application/hmi_application/launch_supervision_session.py` + `application/hmi_application/domain/launch_supervision_types.py` + `application/hmi_application/services/launch_supervision_flow.py` + `application/hmi_application/services/launch_supervision_runtime.py` + `application/hmi_application/adapters/launch_supervision_ports.py` | `launch_supervision_session.py` 已退化为 stable seam，状态机/helper/ports 分流到包内子包。 |
| M11-OWN-002 | `application/hmi_application/startup.py` | split | 原位置保留，authority 收敛到 domain/adapters | `startup.py` 保留 `LaunchResult` 和 launch API，但不再维护重复的 mode/timeout/port 真相。 |
| M11-SFC-002 | `application/hmi_application/launch_supervision_contract.py` | migrated | `application/hmi_application/contracts/launch_supervision_contract.py` | live contract 已迁到明确的 in-package contracts root；旧物理文件已删除。 |
| M11-SFC-003 | `application/hmi_application/supervisor_contract.py` | deleted | 已删除 | 模块内 compat 已在上一轮退场，本轮继续保持清理后状态。 |
| M11-SFC-004 | `application/hmi_application/supervisor_session.py` | deleted | 已删除 | 模块内 compat 已在上一轮退场，本轮继续保持清理后状态。 |
| RB1 | `apps/hmi-app/src/hmi_client/client/supervisor_contract.py` | deleted | 已删除 | app 侧旧命名 wrapper 不再需要，避免双命名并存。 |
| RB1 | `apps/hmi-app/src/hmi_client/client/supervisor_session.py` | deleted | 已删除 | app 侧旧命名 wrapper 不再需要，避免双命名并存。 |
| RB2 | `apps/hmi-app/src/hmi_client/client/launch_supervision_contract.py` | migrated | 原位置保留 | sanctioned wrapper 继续存在，但已切到新 contract root。 |
| M11-BLD-001 | `modules/hmi-application/CMakeLists.txt` | migrated | 原位置保留 | owner assets 已对齐到新的 in-package live split。 |
| M11-DOC-001 | `README.md` + `application/README.md` + `contracts/README.md` + `module.yaml` + `tests/README.md` | migrated | 原位置保留 | 文档与 manifest 已改写为 stage-3 live truth。 |

## 5. Dependency Reconciliation Result
- `module.yaml / CMake / target inventory` 已对齐到当前 live truth：
  - live build 仍只冻结 `application;tests`
  - owner assets 已纳入 `application/hmi_application/contracts/*.py`
  - owner assets 已纳入 `application/hmi_application/domain/*.py`
  - owner assets 已纳入 `application/hmi_application/services/*.py`
  - owner assets 已纳入 `application/hmi_application/adapters/launch_supervision_ports.py`
- `target_link_libraries` 本轮未变化；`siligen_module_hmi_application` 仍只显式链接 `siligen_application_contracts`。
- public headers：本模块无 C++ public headers，本轮无此项对齐动作。
- 还有哪些未清项：
  - `siligen_module_hmi_application` 仍是 `INTERFACE` asset bucket，而不是更细的 owner/contracts/tests topology。
  - `apps/planner-cli/CMakeLists.txt` 仍链接 `siligen_module_hmi_application`，该 consumer 在模块外，本轮未动。
- 为什么本轮不能继续推进：
  - 再往前会变成跨模块 build topology 清理，已越出本轮边界。

## 6. Surface Reconciliation Result
- stable seam 已统一：
  - contract canonical path 已冻结为 `hmi_application.contracts.launch_supervision_contract`
  - session canonical path 继续冻结为 `hmi_application.launch_supervision_session`
  - package root 继续 re-export 稳定 contract/session seam
- façade/provider/bridge 多套并存情况：
  - 模块内无旧命名 `supervisor_*`
  - app 侧仅保留 `launch_supervision_contract.py` 与 `launch_supervision_session.py` 两个 thin wrapper
  - app 侧 `supervisor_contract.py` / `supervisor_session.py` 已删除
- 内部 stage 类型泄漏：
  - `FailureStage`、`SessionStageEvent` 仍是 live contract 一部分，语义未改
  - 这不是本轮阻塞，但后续若要继续收面，需要先重新冻结“哪些 stage detail 仍属稳定 contract”

## 7. Test Result
- 本轮实际运行的测试：
  - `python .\modules\hmi-application\tests\run_hmi_application_tests.py`
  - `python -m pytest .\apps\hmi-app\tests\unit\test_m11_owner_imports.py -q`
  - `python -m pytest .\apps\hmi-app\tests\unit\test_supervisor_contract.py -q`
  - `python -m pytest .\apps\hmi-app\tests\unit\test_supervisor_session.py -q`
  - `python -m pytest .\apps\hmi-app\tests\unit\test_startup_sequence.py -q`
  - `python -m pytest .\apps\hmi-app\tests\unit\test_main_window.py -q`
  - `python -m pytest .\apps\hmi-app\tests\unit\test_smoke.py -q`
- 哪些通过：
  - 模块 canonical runner：`41` 项通过
  - `test_m11_owner_imports.py`：`7` 项通过
  - `test_supervisor_contract.py`：`7` 项通过
  - `test_supervisor_session.py`：`20` 项通过
  - `test_startup_sequence.py`：`11` 项通过
  - `test_main_window.py`：`73` 项通过
  - `test_smoke.py`：`1` 项通过
- 哪些失败：
  - 无
- 失败属于任务内问题还是已知任务外阻塞：
  - 无新增失败；本轮验证未暴露任务外阻塞。

## 8. Remaining Blockers
### 架构 blocker
- `preview_session.py` 仍同时承载 preview owner 与 local playback/presenter 语义，`MB1 preview split` 还没做。
- `startup.py` 虽已去重 authority，但 `LaunchResult` 与 orchestration 仍在同一文件。

### 构建 blocker
- `siligen_module_hmi_application` 仍未拆出更细的 owner/contracts/tests topology。
- 模块外 `planner-cli` 对 `siligen_module_hmi_application` 的 link 尚未清理。

### 测试 blocker
- 模块仍缺少真实 `integration` / `regression` closeout 证据。
- app 侧 thin-host 邻接回归还未重新分层归档到 canonical gate。

### 文档 / 命名 blocker
- 顶层 `modules/hmi-application/contracts|domain|services|adapters|examples/` 仍为物理占位目录，尚未 closeout 或删除。
- `test_supervisor_contract.py` / `test_supervisor_session.py` 这类历史测试名仍带旧命名，只是导入链已切到新路径。

## 9. Final Verdict
- `modules/hmi-application/` 本轮后明显更接近 canonical owner：`launch_supervision` contract 有了真实 live root，`launch_supervision_session.py` 不再是肥大 residual 容器，app 侧旧命名 compat 已退场。
- 本轮已达成 batch acceptance criteria：
  - `MB2a launch_supervision session split`：达成
  - `MB2b startup truth reconciliation`：达成
  - `MB3 contract cutover + compat cleanup`：达成
- 下一批次最应该处理：
  - `MB1 preview split`
  - 其次是 `siligen_module_hmi_application` 的 build topology 收缩与 module-external consumer 清理
