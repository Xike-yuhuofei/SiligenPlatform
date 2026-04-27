# hmi-application Residue Ledger

## 0. Current Truth Update (2026-04-09 startup/build batch)
- 以下增补是当前真值；若与下文历史台账冲突，以本节为准。
- `M11-OWN-002` 已从 monolithic startup 容器降为薄 seam：
  - `hmi_application.startup` 继续作为稳定入口
  - `hmi_application.services.launch_result_projection` 承接 `LaunchResult` / offline result / snapshot projection
  - `hmi_application.services.startup_sequence_service` 承接 startup/recovery sequence 装配与 `SupervisorSession` 委托
  - `hmi_application.adapters.startup_qt_workers` 承接 `StartupWorker` / `RecoveryWorker`，`hmi_application.adapters.qt_workers` 仅保留 preview worker 实现并 re-export startup worker
- `M11-BLD-001` 已部分收口：
  - `apps/planner-cli/CMakeLists.txt` 已移除 `siligen_module_hmi_application` link
  - 剩余 build residue 是 `siligen_module_hmi_application` 仍为 `INTERFACE` owner asset bucket，而不是 consumer 异常 link
- 2026-04-09 现场验证已更新为：
  - `python .\modules\hmi-application\tests\run_hmi_application_tests.py`：`56` 项通过
  - `python -m pytest .\apps\hmi-app\tests\unit\test_startup_sequence.py -q`：`11` 项通过
  - `python -m pytest .\apps\hmi-app\tests\unit\test_m11_owner_imports.py -q`：`11` 项通过
  - `python -m pytest .\apps\hmi-app\tests\unit\test_main_window.py -q`：`73` 项通过
  - `python -m pytest .\apps\hmi-app\tests\unit\test_smoke.py -q`：`1` 项通过
  - `python -m pytest .\apps\hmi-app\tests\unit\test_preview_session_worker.py -q`：`2` 项通过
  - `cmake -S . -B .\build` + `cmake --build .\build --target siligen_planner_cli --config Debug`：通过
- 当前下一阶段入口已收敛为：
  - `preview_session.py` 剩余 split
  - `siligen_module_hmi_application` 的 target topology 继续收缩
  - canonical `integration/regression` 测试面补齐
- 历史台账中所有“`startup.py` 仍混装 QThread/runtime concrete”以及“`planner-cli` 仍链接 `siligen_module_hmi_application`”的表述现已过时。

## 1. Executive Verdict
- 一句话判定：canonical 化进行中但 closeout 未完成；`M11 hmi-application` 的 owner 语义已经真实落到模块内，但 runtime-concrete、compat surface、placeholder 骨架和测试对齐仍未收口。

## 2. Scope
- 本轮只审查目录范围：`modules/hmi-application/`
- 为判断边界引用的少量关联对象范围：
  - `apps/hmi-app/src/hmi_client/ui/main_window.py`
  - `apps/hmi-app/src/hmi_client/client/*`
  - `apps/hmi-app/src/hmi_client/features/dispense_preview_gate/*`
  - `apps/hmi-app/tests/unit/*`
  - `apps/planner-cli/CMakeLists.txt`
  - `tests/CMakeLists.txt`
  - `modules/workflow/S2A_OWNER_MIGRATION_BASELINE.md`

## 3. Baseline Freeze
- owner 基线：
  - `module.yaml:1-11` 冻结 `module_id: M11`、`module_name: hmi-application`、`owner_artifact: UIViewModel`，并把允许依赖收窄到 `workflow/contracts`、`runtime-execution/contracts`、`shared`。
  - `README.md:7-9` 冻结该模块应拥有 HMI 启动恢复、运行态退化、preview 会话与生产前置校验、人机展示对象与 host-to-owner 边界，以及 HMI 域 owner 专属 contracts。
- 职责边界基线：
  - `README.md:19-23` 明确 `apps/hmi-app/` 仅承担宿主；`main_window.py` 只应保留 Qt 宿主、signal/slot、消息框、status bar、协议调用和 HTML 渲染；模块不应长期承担设备实现或运行时执行 owner。
  - `application/README.md:3-10` 把 live owner 面收敛到 `application/hmi_application/`，并声明 `supervisor_*` 仅为 compat shim。
- public surface 基线：
  - `README.md:25-31` 与 `contracts/README.md:3-15` 把模块 owner surface 指向 `contracts/`、`launch_state.py`、`preview_session.py`，且跨模块稳定 contract 应在 `shared/contracts/application/`。
  - `modules/workflow/S2A_OWNER_MIGRATION_BASELINE.md:29-32` 进一步冻结：`apps/hmi-app` 只保留宿主与 compat re-export，M11 tests 应成为 canonical owner tests。
- build 基线：
  - `CMakeLists.txt:1-27` 定义 `siligen_module_hmi_application`，当前是 `INTERFACE` target，owner 资产来自 `application/hmi_application/*.py`，并只显式链接 `siligen_application_contracts`。
  - `tests/CMakeLists.txt:41-70` 把 `modules/hmi-application/tests` 纳入 root canonical test roots，说明该模块 test root 处于 live build graph。
- test 基线：
  - `tests/README.md:5-17` 定义正式测试面为 `tests/unit` 与 `tests/contract`，并明确 host-to-owner 邻接回归与 fault/recovery 矩阵尚未补齐。
  - `tests/run_hmi_application_tests.py:17-23` 当前 runner 只发现 `unit` 与 `contract`。
  - 2026-04-09 现场证据：执行 `python .\modules\hmi-application\tests\run_hmi_application_tests.py`，56 项测试全部通过，说明 `modules/hmi-application/tests` 是 live test evidence，不只是目录占位。

## 4. Top-Level Findings
- `M11` owner 迁移并非纯文档声明：`apps/hmi-app/src/hmi_client/ui/main_window.py:41-68` 已消费 M11 surface，模块自带 runner 在 2026-04-09 通过 56 项测试，说明 owner 事实已经真实落在模块内。
- closeout 未完成同样不是推测：`main_window.py:41-68` 同时导入 `client` compat façade 和 `hmi_application.preview_session` 直连 surface，表明 app-facing seam 与 module-facing seam 并存。
- `application/hmi_application/preview_session.py` 是当前最大的 residual 容器：同一文件同时承载 preview authority 校验、Qt worker、TCP/RPC、worker profiling、本地 playback 和 preflight 决策。
- `launch_supervision_session.py` 已退化为 stable seam，`startup.py` 也已收缩为薄 startup seam；当前 startup residue 已转移到包内 split 完成后的 surface/测试/拓扑收口，而不再是单文件 runtime-concrete 混装。
- `contracts/README.md` 把 `contracts/` 宣称为模块专属 contract root，但 live contract 实体实际位于 `application/hmi_application/launch_supervision_contract.py`，contracts surface 与实际 owner 产物不一致。
- `supervisor_contract.py` / `supervisor_session.py` 在模块内部仍保留旧命名 compat shim；app 侧也保留平行 re-export，旧名和新名并存。
- `siligen_module_hmi_application` 目前仍更像 metadata / asset bucket，而不是清晰的 owner/contracts/build seam；`planner-cli` 的异常 CMake consumer 已清理，当前 blocker 聚焦在 target topology 本身。
- 模块级正式 test root 是 live 的，但 app 侧仍残留 owner/compat tests；与此同时 `apps/hmi-app/tests` 不在 root `tests/CMakeLists.txt:41-58` 的 canonical roots 中，这些测试更像历史噪音而非正式 closeout gate。

## 5. Residue Ledger Summary
- owner residue：3
- build residue：1
- surface residue：4
- naming residue：1
- test residue：2
- placeholder residue：5

## 6. Residue Ledger Table
| residue_id | severity | file_or_target | current_role | why_residue | violated_baseline | evidence | action_class | preferred_destination | confidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| M11-OWN-001 | P0 | `modules/hmi-application/application/hmi_application/preview_session.py` | Preview owner + snapshot worker + local playback controller + preflight gate | 同一文件同时承载 authority 校验、Qt 线程、TCP/RPC 调用、worker profiling 和本地播放状态，已经把 owner 语义、runtime concrete 与 host 呈现揉成一个 residual 容器。 | `README.md:19-23`；`module.yaml:6-11`；`adapters/README.md:3`；`services/README.md:3` | `preview_session.py:10,21-25,182-310,313-315,539-582,762-1087`；`main_window.py:63-68,431,3634` | split | 纯 preview/preflight owner 保留在 `application/` 或下沉 `domain/`；`PreviewSnapshotWorker` 迁到 `adapters/runtime_gateway/`；local playback 降到 host/presenter service | high |
| M11-OWN-002 | P1 | `modules/hmi-application/application/hmi_application/startup.py` | Launch result + startup/recovery orchestration + Qt worker + log sink | `LaunchResult`、policy 读取、启动/恢复 orchestration、`QThread` worker、文件日志落盘混装，模块继续承载 runtime concrete 与 host wiring。 | `README.md:19-23`；`application/README.md:5`；`adapters/README.md:3`；`services/README.md:3` | `startup.py:4-10,27-35,44-53,168-255,312-388` | split | `LaunchResult` / mode normalization 保留 owner 层；orchestration 迁到 `services/`；`StartupWorker` / `RecoveryWorker` 与日志落到 `adapters/qt` 或 app host | high |
| M11-OWN-003 | P1 | `modules/hmi-application/application/hmi_application/launch_supervision_session.py` | Supervisor state machine + recovery coordinator + runtime degradation helper | 830 行文件同时容纳 protocol 抽象、启动状态机、恢复编排、runtime failure detection；`domain/`、`services/`、`adapters/` 全空时，它成了事实上的肥大 residual 容器。 | `domain/README.md:3`；`services/README.md:3`；`adapters/README.md:3` | `launch_supervision_session.py:22-30,33-64,104-239,744-829`；文件长度 830 行 | split | 状态机与纯不变量下沉 `domain/`；恢复/启动编排迁到 `services/`；外设接口与 concrete 适配迁到 `adapters/` | high |
| M11-SFC-001 | P1 | `modules/hmi-application/application/hmi_application/__init__.py` | Package façade / omnibus public surface | 单一 `__init__` 同时 re-export gate、DTO、worker、session、contract、compat-facing types，导致 workflow-facing seam、app-facing seam、module-facing seam 过厚且难以冻结。 | `README.md:25-31`；`contracts/README.md:3-15` | `__init__.py:1-104`；`main_window.py:41-68`；`apps/hmi-app/src/hmi_client/client/__init__.py:30-68` | shrink | 只保留稳定 owner seam；worker、compat、state-machine internals 改为显式子模块导入，不再默认进 package root | high |
| M11-SFC-002 | P1 | `modules/hmi-application/application/hmi_application/launch_supervision_contract.py` | Live startup supervision contract | 真正 live 的模块 contract 放在 `application/` 下，而 `contracts/` 只有 README；同时 `FailureStage` / `SessionStageEvent` 等 stage 类型直接作为对外 surface 暴露，surface 冻结线不清。 | `README.md:14,29-31`；`contracts/README.md:3-15` | `launch_supervision_contract.py:1-129`；`contracts/README.md:1-21`；`application/README.md:9` | migrate | 迁到 `modules/hmi-application/contracts/`，并在迁移时划清哪些类型是稳定 contract、哪些只是内部 stage detail | high |
| M11-SFC-003 | P2 | `modules/hmi-application/application/hmi_application/supervisor_contract.py` | Deprecated compat shim | 旧命名 `supervisor_*` 仍留在模块 owner 包内，旧名与新名并存，bridge/compat closeout 没完成。 | `application/README.md:9`；`README.md:37` | `supervisor_contract.py:1-4`；`apps/hmi-app/src/hmi_client/client/supervisor_contract.py:1-10`；`apps/hmi-app/tests/unit/test_m11_owner_imports.py:22-33` | demote | 兼容层若必须保留，应只留在 app compat 包；模块 owner 包内应最终删除 | high |
| M11-SFC-004 | P2 | `modules/hmi-application/application/hmi_application/supervisor_session.py` | Deprecated compat shim | 与 `supervisor_contract.py` 同类；旧命名 session shim 留在模块内，继续制造双 surface。 | `application/README.md:9`；`README.md:37` | `supervisor_session.py:1-4`；`apps/hmi-app/src/hmi_client/client/supervisor_session.py:1-10`；`apps/hmi-app/tests/unit/test_m11_owner_imports.py:26-27` | demote | 兼容 shim 降级到 app compat 或完成删除 | high |
| M11-BLD-001 | P1 | `modules/hmi-application/CMakeLists.txt` / `siligen_module_hmi_application` | Module root target / owner asset bucket | 当前 target 只是 `INTERFACE + owner_assets`，却把 `domain/services/application/adapters/tests/examples` 都登记为 implementation roots；没有表达 Python runtime concrete 依赖，反而被 `planner-cli` 链接，构建拓扑和实际 owner seam 不一致。 | `README.md:13-15`；`module.yaml:6-11` | `CMakeLists.txt:1-27`；`apps/planner-cli/CMakeLists.txt:21-33`；`tests/CMakeLists.txt:41-70` | split | 拆出更清晰的 owner/contracts/tests metadata；若 `planner-cli` 无真实消费则移除该 link；把 placeholder roots 从 live implementation roots 中清退 | medium |
| M11-PLH-001 | P1 | `modules/hmi-application/contracts/README.md` + `modules/hmi-application/contracts/` | Declared contract root, 实际为空壳 | `contracts/` 在仓库中存在，但本轮未发现任何 live contract artifact；它现在只是文档占位壳层。 | `README.md:14,29`；`contracts/README.md:3-15` | `contracts/README.md:1-21`；目录清单仅有 `README.md` | demote | 若 contract 继续放 `application/`，应先把 `contracts/` 降级为说明性目录；若坚持 baseline，则应承接真实 contract 迁入 | high |
| M11-PLH-002 | P2 | `modules/hmi-application/domain/` | Declared home for owner facts / value objects | `domain/` 只剩 README，纯不变量和事实并未真正进入该层。 | `domain/README.md:3` | `domain/README.md:1-4`；目录清单仅有 `README.md`；`launch_supervision_contract.py:69-129`；`preview_gate.py:31-151` | migrate | 承接纯 contract / gate / invariant 类对象 | high |
| M11-PLH-003 | P1 | `modules/hmi-application/services/` | Declared home for use-case orchestration | `services/` 为空，但启动、恢复、preview orchestration 仍堆在 `startup.py`、`launch_supervision_session.py`、`preview_session.py`。 | `services/README.md:3` | `services/README.md:1-4`；目录清单仅有 `README.md`；`startup.py:168-255`；`launch_supervision_session.py:136-239`；`preview_session.py:762-1087` | migrate | 承接启动、恢复、preview orchestration 与结果组装 | high |
| M11-PLH-004 | P1 | `modules/hmi-application/adapters/` | Declared home for external dependency adapters | `adapters/` 为空，但 `PyQt5`、`TcpClient`、`CommandProtocol`、`FileHandler` 等外部 concrete 仍位于 application 文件内。 | `adapters/README.md:3`；`module.yaml:6-11` | `adapters/README.md:1-4`；目录清单仅有 `README.md`；`preview_session.py:10,21-25,182-310`；`startup.py:10,44-53,312-388` | migrate | 承接 Qt worker、runtime-gateway client/protocol 适配与日志 sink | high |
| M11-TST-001 | P1 | `modules/hmi-application/tests/run_hmi_application_tests.py` + `tests/integration/` + `tests/regression/` | Canonical module test entry + placeholder suite dirs | runner 只发现 `unit` 和 `contract`；`integration/`、`regression/` 仅 `.gitkeep`，而 `tests/README.md` 又明确 host-to-owner 邻接和 recovery matrix 尚未补齐，closeout gate 不完整。 | `tests/README.md:13-17` | `run_hmi_application_tests.py:17-23`；`tests/integration/.gitkeep`；`tests/regression/.gitkeep`；`tests/CMakeLists.txt:16-22`；root `tests/CMakeLists.txt:41-70` | migrate | 真实补齐 integration/regression 后接入 runner；若短期不做，应先从 closeout 叙述中降级这些壳层 | high |
| M11-TST-002 | P2 | `modules/hmi-application/tests/unit/*.py` + `modules/hmi-application/tests/contract/*.py` | Canonical owner tests | 这些测试都靠 `sys.path.insert(...)` 直插 `modules/hmi-application/application`，绕过模块根 package / target，说明 test seam 仍在走路径黑魔法。 | `README.md:13-15`；`CMakeLists.txt:1-27` | `tests/unit/test_launch_state.py:6-10`；`tests/unit/test_preview_session.py:9-13`；`tests/contract/test_launch_supervision_contract.py:6-10` | migrate | 使用一个 canonical test bootstrap 或模块根 package 入口，不再每个测试文件单独改 `sys.path` | high |
| M11-DOC-001 | P1 | `modules/hmi-application/README.md` + `module.yaml` + `application/README.md` + `tests/README.md` | Frozen baseline documentation | 文档一边声明 app 侧只保留 thin-host / compat、`contracts/` 是模块 contract 入口、模块不承载 runtime concrete；另一边实际代码仍有 app compat tests、双 import 路径、Qt worker、日志落盘和空壳目录。 | 冻结基线自身互相打架 | `README.md:13-15,19-23,35-37`；`application/README.md:5-10`；`tests/README.md:5-17`；`module.yaml:6-11`；`apps/hmi-app/tests/unit/test_supervisor_session.py:9-11`；`apps/hmi-app/tests/unit/test_startup_sequence.py:10-19`；`apps/hmi-app/tests/unit/test_preview_session_worker.py:10-12` | demote | 在结构 closeout 前，把 README/module.yaml 显式改成“迁移进行中”；closeout 后再恢复为最终 frozen narrative | high |
| M11-PLH-005 | P2 | `modules/hmi-application/application/hmi_application/__pycache__/` + `modules/hmi-application/tests/unit/__pycache__/` | Generated bytecode inside module tree | 非源码产物进入 owner 模块与测试树，只会污染审计视图和 closeout 结果；不属于 live owner/seam。 | `README.md:37` 的“终态实现只落在 canonical 骨架内” | 模块文件清单中出现 `application/hmi_application/__pycache__/*.pyc` 与 `tests/unit/__pycache__/*.pyc` | delete | 无；从仓库树中移除并交由本地缓存/忽略规则管理 | high |

## 7. Hotspots
- `modules/hmi-application/application/hmi_application/preview_session.py`
  - 当前最重的 cross-layer 容器；同时是 owner 语义、worker 线程、runtime RPC 和本地 playback 的交汇点。
- `modules/hmi-application/application/hmi_application/startup.py`
  - 把 launch result、启动编排、恢复动作、Qt worker 与日志落盘装到一起，是第二个 closeout blocker。
- `modules/hmi-application/application/hmi_application/launch_supervision_session.py`
  - 实际承担了 state machine + service + adapter protocol 三层职责。
- `modules/hmi-application/application/hmi_application/__init__.py`
  - 过厚 surface 是 app 直连、多命名并存和 compat 未收口的放大器。
- `modules/hmi-application/CMakeLists.txt` / `siligen_module_hmi_application`
  - target 定义与 live Python owner reality 不匹配，还被 `planner-cli` 异常链接。
- `modules/hmi-application/application/hmi_application/launch_supervision_contract.py`
  - 这是 live contract，却不在 `contracts/`，造成 surface/documentation freeze 断裂。
- `modules/hmi-application/tests/run_hmi_application_tests.py`
  - 当前正式 gate 只覆盖 unit/contract，两类 placeholder suite 没有进入 runner。
- `modules/hmi-application/README.md` + `module.yaml`
  - 基线叙述已经先行冻结，但代码 closeout 仍未跟上，形成反复误导。

## 8. False Friends
- `modules/hmi-application/contracts/`
  - 看起来像 canonical contract root，实际只有 README。
- `modules/hmi-application/domain/`、`services/`、`adapters/`
  - 目录名已经 canonical，实际尚未承接 live owner/service/adapter 对象。
- `modules/hmi-application/application/hmi_application/__init__.py`
  - 看起来像整洁 façade，实际是把稳定 seam 和 concrete worker 一起打包的 residual surface。
- `modules/hmi-application/application/hmi_application/supervisor_contract.py` / `supervisor_session.py`
  - 看起来像合法 surface，实际只是待退场 compat shim。
- `siligen_module_hmi_application`
  - 看起来像 canonical module target，实际更像 metadata / assets bucket。
- `modules/hmi-application/tests/integration/`、`tests/regression/`
  - 看起来像正式 test suite，实际只是 `.gitkeep` 壳层。
- `modules/hmi-application/examples/`
  - 看起来像调试/回归样例根，实际本轮未发现任何 live artifact。

## 9. Closeout Blockers
### 架构阻塞
- `preview_session.py`、`startup.py`、`launch_supervision_session.py` 都还在同时承载 owner、service、adapter、host-facing 行为，导致 `domain/services/adapters` 无法真正接管职责。
- app-facing compat seam 与 module-facing direct seam 并存；`main_window.py` 同时走 `client` façade 和 `hmi_application.preview_session` 直连，surface 不能冻结。
- `launch_supervision_contract.py` 的 contract 归属未冻结，导致 `contracts/` 与 `application/` 谁是 authority 入口一直悬空。

### 构建阻塞
- `siligen_module_hmi_application` 仍是 `INTERFACE` 资产桶，没有把 owner/contracts/tests 的 build topology 清晰拆开。
- `planner-cli` 仍链接 `siligen_module_hmi_application`，说明 build graph 里还有异常 consumer。
- `CMakeLists.txt` 的 implementation roots 把 placeholder 目录也登记进 live root，构建叙述放大了空壳。

### 测试阻塞
- canonical module test runner 只覆盖 `unit` / `contract`，不能支撑 owner closeout 所需的 host-to-owner adjacency 与 recovery matrix。
- app 侧 owner/compat tests 仍存在，但又不在 root canonical test roots 内，正式 gate 与历史噪音混杂。
- 模块测试仍靠逐文件 `sys.path` 注入，不利于后续 surface 收缩和路径冻结。

### 文档 / 命名阻塞
- `README.md` / `application/README.md` / `tests/README.md` / `module.yaml` 已经写成 closeout 口径，但代码现实仍是迁移中。
- `launch_supervision_*` 与 `supervisor_*` 双命名共存，容易把 compat 误判为 canonical。
- `contracts/` 被声明为 canonical contract root，但 live contract 不在那里。

## 10. No-Change Zone
- 不要在下一阶段未经 contract freeze 就改 `SessionSnapshot`、`SessionStageEvent`、`FailureStage`、`RecoveryAction` 的语义；这些不变量当前由 `launch_supervision_contract.py` 与 `tests/contract/test_launch_supervision_contract.py` 固定。
- 不要在没有 replacement test 的情况下删改 `tests/unit/test_launch_state.py`、`tests/unit/test_preview_session.py`、`tests/contract/test_launch_supervision_contract.py`；它们是本轮唯一已验证通过的模块 closeout 证据。
- `preview_gate.py` 当前相对接近纯 gate/state object，不应在下一阶段被顺手扩成新的 residual 容器。
- `launch_state.py` 的可观察输出字段与状态投影结果应先保持语义稳定；后续可以拆文件，但不应先改行为。
- 本轮不应扩散处理 `apps/hmi-app`、`apps/planner-cli` 或 `shared/` 的实现，只把这些对象作为 consumer/build 证据使用。

## 11. Next-Phase Input
### migration batches
- `MB1 preview split`：把 `preview_session.py` 拆成 preview authority/preflight owner、runtime snapshot adapter、local playback/presenter 三块。
- `MB2 startup-supervision split`：把 `startup.py` 的 `LaunchResult` 与 owner 语义保留，把 `QThread` worker、日志 sink、concrete orchestration 迁出；同时把 `launch_supervision_session.py` 切成 domain state machine、service orchestration、adapter protocol。
- `MB3 contract relocation`：把 `launch_supervision_contract.py` 迁入 `contracts/`，或明确裁定其仍属 application owner contract 并同步收缩 `contracts/README.md`。
- `MB4 surface shrink`：瘦身 `__init__.py`，把 worker、compat shim、internal stage helpers 从默认 public surface 清退。

### delete batches
- `DB1 compat cleanup`：在 app consumer 全部切换后删除模块内 `supervisor_contract.py` / `supervisor_session.py`。
- `DB2 source tree cleanup`：删除模块内 `__pycache__` 和迁移后仍为空的 placeholder 壳层。

### rename batches
- `RB1 naming convergence`：统一 `launch_supervision_*` 与 `supervisor_*` 命名，只保留一套 canonical 名称。
- `RB2 import-path convergence`：统一 app 到 M11 的导入路径，避免 `client` façade 与 `hmi_application.*` 直连长期并存。

### contract freeze items
- `CF1`：冻结 `SessionSnapshot` / `SessionStageEvent` / `FailureStage` 哪些是稳定对外 contract，哪些只是内部启动监督态细节。
- `CF2`：冻结 `PreviewSnapshotWorker`、`StartupWorker`、`RecoveryWorker` 到底属于模块 owner、adapter 层还是宿主层。
- `CF3`：冻结 local playback 是否属于 HMI owner 语义，还是纯 host/presenter 责任。

### test realignment items
- `TR1`：把模块 `unit/contract` 明确为 primary evidence，把 app 侧 owner/compat tests 降级为 thin-host 验证或删除。
- `TR2`：补齐 `integration/` / `regression/` 的 host-to-owner 邻接回归与 startup fault/recovery 矩阵，并纳入 runner。
- `TR3`：用一个 canonical bootstrap 取代各测试文件的 `sys.path.insert(...)`。

## 12. Suggested Execution Order
- `Step 1: Freeze contract and surface`
  - 最小 acceptance criteria：明确 `contracts/` 是否承接 live contract；明确 `launch_supervision_*` 与 `supervisor_*` 的退场名单；明确 app 到模块只保留一条 sanctioned import path。
- `Step 2: Extract runtime concrete adapters`
  - 最小 acceptance criteria：`PreviewSnapshotWorker`、`StartupWorker`、`RecoveryWorker`、日志落盘不再作为 live owner surface 留在 `application/hmi_application/` 根包；模块现有 41 项测试继续通过。
- `Step 3: Refill domain/services/adapters with live objects`
  - 最小 acceptance criteria：`domain/`、`services/`、`adapters/` 都至少承接一类 live 对象；`preview_session.py`、`startup.py`、`launch_supervision_session.py` 显著瘦身，不再是单文件 residual container。
- `Step 4: Shrink public surface and retire compat`
  - 最小 acceptance criteria：`__init__.py` 只保留稳定 seam；模块内 `supervisor_*` 删除或明确 demote；app 侧 compat 只保留必要最小面。
- `Step 5: Realign tests/docs/placeholders`
  - 最小 acceptance criteria：`run_hmi_application_tests.py` 与文档描述一致；`integration/`/`regression/` 要么有真实测试，要么不再伪装正式 suite；README/module.yaml 不再把迁移中状态写成已 closeout。
