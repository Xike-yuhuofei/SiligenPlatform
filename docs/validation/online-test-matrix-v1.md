# 联机测试矩阵 v1

更新时间：`2026-04-21`

## 1. 目标与非目标

### 1.1 目标

- 把联机验证从“零散脚本”收敛为可执行、可回链、可追溯的测试矩阵。
- 统一 HIL、DXF dry-run、在线预览、HMI online smoke、故障恢复和 controlled gate 的入口口径。
- 明确哪些场景已具备正式入口，哪些场景仍处于 `partial` 或 `planned` 状态。

### 1.2 非目标

- 本文档不替代真实设备现场验收，也不替代工艺质量签收。
- 本文档不新增新的联机总入口，优先复用现有 `tests/e2e/hardware-in-loop/` 与 `apps/hmi-app/scripts/`。
- 本文档不把 UI 截图或人工观察当作唯一验收依据；截图只能作为辅助证据。

## 2. authority artifact 与 owner

| 场景簇 | authority artifact | owner 层 | consumer-only 层 |
| --- | --- | --- | --- |
| HIL 状态机闭环 | `hil-closed-loop-summary.json` | `tests/e2e/hardware-in-loop/` | `docs/runtime/`、发布收尾文档 |
| HIL controlled gate | `hil-controlled-gate-summary.json` | `tests/e2e/hardware-in-loop/verify_hil_controlled_gate.py` | `docs/runtime/field-acceptance.md`、发布流程 |
| DXF 真机 dry-run 主链 | `real-dxf-machine-dryrun.json` 及同批次观测工件 | `tests/e2e/hardware-in-loop/` | 诊断/回归说明文档 |
| DXF 生产模式真机专项 | `real-dxf-production-validation.json` 及同批次观测工件 | `tests/e2e/hardware-in-loop/` | 验证汇总、incident 材料 |
| 在线预览证据 | `preview-verdict.json` + `plan-prepare.json` + `snapshot.json` | `tests/e2e/hardware-in-loop/` + `tests/contracts/` | HMI 展示、验证汇总 |
| HMI online 成功/失败/恢复 | `online-smoke.log` 中的 `SUPERVISOR_EVENT` / `SUPERVISOR_DIAG` | `apps/hmi-app/scripts/` + `apps/hmi-app/src/hmi_client/tools/` | `docs/validation/`、`docs/runtime/` |
| HMI operator production 专项 | `hmi-production-operator-test.json` 及同批次观测工件 | `tests/e2e/hardware-in-loop/run_hmi_operator_production_test.py` | 操作问题清单、traceability 边界说明 |

约束：

- `online_ready` 的唯一真值仍来自 Supervisor 快照，不允许由 HMI 本地 UI 状态替代。
- 在线预览的唯一真值仍是 `planned_glue_snapshot` 系列工件，不允许把 `mock_synthetic` 或未知来源当作正式通过证据。
- controlled gate 只消费已冻结的 HIL 输出，不直接重解释设备行为语义。

## 3. 当前正式入口总览

| 场景簇 | 当前入口 | 最小输出 | 当前定位 |
| --- | --- | --- | --- |
| TCP 最小联机 smoke | `tests/e2e/hardware-in-loop/run_hardware_smoke.py` | `hardware-smoke` 报告、退出码 | P0 正式入口 |
| HIL 闭环 | `tests/e2e/hardware-in-loop/run_hil_closed_loop.py` | `hil-closed-loop-summary.json/md` | P0 正式入口 |
| HIL controlled gate | `tests/e2e/hardware-in-loop/run_hil_controlled_test.ps1` + `verify_hil_controlled_gate.py` | offline-prereq / hardware-smoke / HIL / gate / release summary | P0 正式入口 |
| DXF canonical dry-run | `tests/e2e/hardware-in-loop/run_real_dxf_machine_dryrun.py` | dry-run 报告目录、状态观测工件 | P0 正式入口 |
| DXF 生产模式真机专项 | `tests/e2e/hardware-in-loop/run_real_dxf_production_validation.py` | `real-dxf-production-validation.json/md`、状态观测工件、gateway 日志切片 | owner-local 正式专项入口，不替代 controlled gate |
| 在线预览证据 | `tests/e2e/hardware-in-loop/run_real_dxf_preview_snapshot.py` | `plan-prepare.json`、`snapshot.json`、`preview-verdict.json` 等 | P0 正式入口 |
| HMI online smoke | `apps/hmi-app/scripts/online-smoke.ps1` | `online-smoke.log`、qtest stdout/stderr、截图 | P0 正式入口 |
| HMI operator production 专项 | `tests/e2e/hardware-in-loop/run_hmi_operator_production_test.py` | `hmi-production-operator-test.json/md`、job/machine/coord 历史、gateway 日志切片、staged screenshots | owner-local 正式专项入口，不替代 `P1-04` runtime action matrix |
| HMI failure stage 注入 | `apps/hmi-app/scripts/verify-online-ready-timeout.ps1` | `SUPERVISOR_DIAG` / `SUPERVISOR_EVENT` 映射证据 | P0 正式入口 |
| HMI recovery | `apps/hmi-app/scripts/verify-online-recovery-loop.ps1` | 恢复前后事件与最终 `online_ready=true` | P0 正式入口 |
| 多轮 home/closed-loop matrix | `tests/e2e/hardware-in-loop/run_case_matrix.py`，或根级 `test.ps1 -IncludeHilCaseMatrix` | `case-matrix-summary.json/md` | P1 补充入口，已接 root validation opt-in，已接 controlled gate / release-check 默认门禁 |
| DXF 批量在线预览 | `tests/e2e/hardware-in-loop/run_real_dxf_preview_suite.py` | suite summary + per-case `preview-verdict.json` + evidence bundle | owner-local 聚合入口，用于 full-online blocker 集合 |
| 多 DXF dry-run 回归 | `tests/e2e/hardware-in-loop/run_real_dxf_machine_dryrun_suite.py` | suite summary + per-case dry-run 报告 + evidence bundle | owner-local 聚合入口，用于 full-online blocker 集合 |
| HMI full-online 汇总 | `apps/hmi-app/scripts/run_full_online_hmi_suite.py` | suite summary + per-scenario logs + evidence bundle | owner-local 聚合入口，用于 full-online blocker 集合 |
| soak profiles 汇总 | `tests/performance/run_online_soak_profiles.py` | suite summary + per-profile `latest.json/md` + evidence bundle | owner-local 聚合入口，用于 full-online blocker 集合 |

### 3.1 full-online blocker 集合与 controlled quick gate 关系

- 当前没有 repo-root 单一“全量联机测试总入口”；full-online 仍由多个 owner-local 入口组成。
- `run_real_dxf_preview_suite.py`、`run_real_dxf_machine_dryrun_suite.py`、`run_full_online_hmi_suite.py`、`run_online_soak_profiles.py` 只负责补齐 blocker 集合与 `G8` 补充证据。
- `tests/e2e/hardware-in-loop/run_hil_controlled_test.ps1` 仍是唯一 canonical controlled HIL 入口，但现在只保留 `60s` quick gate，且默认强制 attach 现有 gateway，不再允许双轨自启第二个 gateway。
- `signed publish`、`latest authority` 刷新和 `1800s formal gate` 均已禁用；后续只保留时间戳 evidence 批次。

## 4. 联机测试矩阵

### 4.1 P0：先做，直接决定联机门禁质量

| ID | 层级 | 场景 | 当前入口 | 必须证据 | 当前状态 |
| --- | --- | --- | --- | --- | --- |
| `P0-01` | L1 | TCP 最小联机 smoke | `run_hardware_smoke.py` | `hardware-smoke` 报告、退出码、stdout/stderr | `existing` |
| `P0-02` | L2 | 单轮 `start/pause/resume/stop` 闭环 | `run_hil_closed_loop.py` | `hil-closed-loop-summary.json/md`、`state_transition_checks`、`failure_context` | `existing` |
| `P0-03` | L2 | 多轮 `home + closed_loop` 稳定性 | `run_case_matrix.py`、`test.ps1 -IncludeHilCaseMatrix`、`run_hil_controlled_test.ps1` | `case-matrix-summary.json/md`、round 级 snapshots | `existing`：已接 root validation opt-in，已接 controlled gate / release-check 默认门禁 |
| `P0-04` | L3 | canonical DXF dry-run 主链 | `run_real_dxf_machine_dryrun.py` | dry-run 报告、`phase_timeline`、`verdict`、`evidence_contract` | `existing` |
| `P0-05` | L3 | 设备前提条件阻断 | `tests/e2e/hardware-in-loop/run_real_dxf_machine_dryrun_negative_matrix.py` + `run_real_dxf_machine_dryrun.py` + `tests/integration/scenarios/first-layer/run_tcp_precondition_matrix.py` | `real-dxf-machine-dryrun-negative-matrix.json/md`、per-case dry-run 报告、阻断原因、状态快照、失败上下文 | `existing`：canonical dry-run negative matrix 已收敛到 `estop`、`door_open`、`home_boundary_x_active`、`home_boundary_y_active` |
| `P0-06` | L4 | 在线预览证据主干 | `run_real_dxf_preview_snapshot.py` + `tests/contracts/test_online_preview_evidence_contract.py` | `plan-prepare.json`、`snapshot.json`、`glue_points.json`、`motion_preview.json`、`preview-verdict.json`、`preview-evidence.md`、`hmi-preview.png`、`online-smoke.log` | `existing` |
| `P0-07` | L5 | HMI online 启动成功 | `apps/hmi-app/scripts/online-smoke.ps1` | `online-smoke.log`、截图、`SUPERVISOR_EVENT` 最小阶段序列 | `existing` |
| `P0-08` | L5 | HMI failure stage/code 识别 | `apps/hmi-app/scripts/verify-online-ready-timeout.ps1` | `failure_code`、`failure_stage`、阶段失败事件 | `existing` |
| `P0-09` | L5 | HMI recovery 闭环 | `apps/hmi-app/scripts/verify-online-recovery-loop.ps1` | `stage_failed`、`recovery_invoked`、`stage_succeeded@online_ready`、最终 `online_ready=true` | `existing` |
| `P0-10` | L6 | controlled gate 基本门禁 | `run_hil_controlled_test.ps1` + `verify_hil_controlled_gate.py` | `hil-controlled-gate-summary.json/md` | `existing` |

### 4.2 P1：随后补深度

| ID | 层级 | 场景 | 建议落点 | 当前状态 |
| --- | --- | --- | --- | --- |
| `P1-02` | L3 | 门/急停/限位阻断专项 | 扩 `run_real_dxf_machine_dryrun.py` 的负例参数矩阵 | `planned` |
| `P1-03` | L4 | DXF 基线集批量在线预览 | `tests/e2e/hardware-in-loop/run_real_dxf_preview_suite.py` | `existing`：聚合 `rect_diag` / `bra` / `arc_circle_quadrants`，产出 suite summary 与 evidence bundle，不替代 controlled publish |
| `P1-04` | L5 | HMI runtime actions 批量在线回归 | `apps/hmi-app/scripts/run_online_runtime_action_matrix.py` + `online-smoke.ps1` / `ui_qtest.py` | `existing`：正式在线预览 authority 仅允许 `operator_preview`；`snapshot_render` 仅供 HIL 截图链 render-only 使用，不进入 matrix |
| `P1-05` | L6 | 30 分钟以上 soak | `tests/performance/run_online_soak_profiles.py --profile-id soak-30m-matrix` | `existing`：已纳入 full-online blocker 集合；`>30m` 扩展 profile 需要显式 `--allow-long-profiles` |
| `P1-06` | L5 | HMI operator production 专项 | `tests/e2e/hardware-in-loop/run_hmi_operator_production_test.py` | `existing`：通过 `operator_production` 正式入口走 HMI browse/preview/start/completed/next-job-ready；报告必须分离 `operator_execution` 与 `traceability_correspondence` |

### 4.3 P2：发布前与容量边界

| ID | 层级 | 场景 | 建议落点 | 当前状态 |
| --- | --- | --- | --- | --- |
| `P2-01` | L3/L4 | 多 DXF 回归套件 | `tests/e2e/hardware-in-loop/run_real_dxf_machine_dryrun_suite.py` + `run_real_dxf_preview_suite.py` | `existing`：multi-DXF dry-run / preview 已聚合，但仍只作为 blocker summary，不发布 latest |
| `P2-02` | L5/L6 | HMI 故障恢复夜间回归 | nightly suite 脚本 + suite summary | `planned` |
| `P2-03` | 真机 E2E | 发布前最小真机主链 | 现场记录文档 + 受限执行 SOP | `planned` |

## 5. 必须证据标准

### 5.1 HIL 状态机类

至少保留：

- `hil-closed-loop-summary.json`
- `hil-closed-loop-summary.md`
- `failure_context`
- `recent_status_snapshot`
- `state_transition_checks`（`hil-closed-loop`）

### 5.2 在线预览类

至少保留：

- `plan-prepare.json`
- `snapshot.json`
- `glue_points.json`
- `motion_preview.json`
- `preview-verdict.json`
- `preview-evidence.md`
- `hmi-preview.png`
- `online-smoke.log`
- `real-dxf-preview-suite-summary.json`
- `real-dxf-preview-suite-summary.md`

### 5.3 HMI 故障/恢复类

至少保留：

- qtest stdout/stderr
- screenshot
- `online-smoke.log`
- `failure_code` / `failure_stage` 断言结果
- 恢复前后 `SUPERVISOR_EVENT` / `SUPERVISOR_DIAG`
- `full-online-hmi-suite-summary.json`
- `full-online-hmi-suite-summary.md`

### 5.4 HMI runtime actions matrix

至少保留：

- `online-runtime-action-matrix-summary.json`
- `online-runtime-action-matrix-summary.md`
- 每个 profile 的 `online-smoke.log`
- 每个 profile 的 screenshot
- `operator_preview` case 的 `OPERATOR_CONTEXT` 阶段序列与显式 version 选择证据

### 5.4.1 HMI operator production 专项

至少保留：

- `hmi-production-operator-test.json`
- `hmi-production-operator-test.md`
- `job-status-history.json`
- `machine-status-history.json`
- `coord-status-history.json`
- `tcp_server.log`
- `gateway-stdout.log`
- `gateway-stderr.log`
- `hmi-stdout.log`
- `hmi-stderr.log`
- `hmi-screenshots/`

其中 `hmi-production-operator-test.json` 必须满足：

- `operator_execution.status` 与 `traceability_correspondence.status` 分开表达，不允许用单一 passed/failed 混写。
- 若 `coord-status-history.json` 为空，则 `traceability_correspondence.status` 只能是 `insufficient_evidence`，不得写成 `passed`。
- `control_script_capability` 必须说明该入口是否真的覆盖 `production-started` 与 `next-job-ready`。

### 5.5 controlled gate 类

至少保留：

- `offline-prereq/workspace-validation.json/md`
- `hardware-smoke/hardware-smoke-summary.json/md`
- `hil-closed-loop-summary.json/md`
- `hil-controlled-gate-summary.json/md`
- `hil-controlled-release-summary.md`
- optional `hil-case-matrix/case-matrix-summary.json/md`

### 5.6 DXF 生产模式真机专项

至少保留：

- `real-dxf-production-validation.json`
- `real-dxf-production-validation.md`
- `job-status-history.json`
- `machine-status-history.json`
- `coord-status-history.json`
- `tcp_server.log`
- `gateway-stdout.log`
- `gateway-stderr.log`

其中 `real-dxf-production-validation.json` 必须满足：

- 根字段 `validation_mode` 只能由 `dxf.plan.prepare` 真值解析，不允许文档预设
- `production_execution` 时必须给出 `timing_summary.execution_nominal_time_s / execution_budget_s / execution_budget_breakdown / observed_execution_time_s`
- `production_blocked` 时必须给出 post-block 观测证据，且 `timing_summary.execution_budget_s / execution_budget_breakdown / observed_execution_time_s = null`

### 5.7 canonical dry-run negative matrix

至少保留：

- `real-dxf-machine-dryrun-negative-matrix.json`
- `real-dxf-machine-dryrun-negative-matrix.md`
- 每个 negative case 的 `real-dxf-machine-dryrun-canonical.json/.md`
- 每个 negative case 的 `launcher.log`

### 5.8 multi-DXF dry-run suite

至少保留：

- `real-dxf-machine-dryrun-suite-summary.json`
- `real-dxf-machine-dryrun-suite-summary.md`
- 每个 case 的 `real-dxf-machine-dryrun-canonical.json/.md`
- 每个 case 的 `launcher.log`

### 5.9 soak profiles

至少保留：

- `online-soak-profiles-summary.json`
- `online-soak-profiles-summary.md`
- 每个 profile 的 `latest.json`
- 每个 profile 的 `latest.md`

### 5.10 分层 evidence bundle 约束

受限 HIL 与联机相关 evidence 还必须补充：

- `case-index.json`
- `validation-evidence-bundle.json`
- `evidence-links.md`
- `report-manifest.json`
- `report-index.json`
- `failure-details.json`（存在非通过结论时）

其中 `validation-evidence-bundle.json` 必须声明：

- `producer_lane_ref=limited-hil`
- 离线前置层（至少 `L0` / `L2`，必要时含 `L3`）
- `metadata.admission`
- `metadata.admission.safety_preflight_passed`
- `metadata.admission.operator_override_reason`（仅当 `operator_override_used=true` 时必填）
- `skip_justification`
- `abort_metadata`

## 6. 项目落地规则

1. 新的联机场景优先扩现有入口参数；若必须补聚合层，只允许在 owner-local 目录新增 suite runner，不允许新增 repo-root 总入口脚本。
2. 正式矩阵定义沉到 `docs/validation/`；脚本入口说明沉到各自 owner README。
3. 只有当场景输出已经形成稳定 `authority artifact` 时，才允许接入 `verify_hil_controlled_gate.py` 或发布流程。
4. `tests/contracts/` 与 `tests/integration/scenarios/first-layer/` 继续承担离线契约与负例验证；不得让联机脚本独自承担契约真值。
5. 任何需要真实设备动作的新增场景，先补观察点、证据和停机条件，再补脚本。

## 7. 推荐推进顺序

### 第一阶段

- 维持 `P0-01`、`P0-02`、`P0-04`、`P0-06`、`P0-07`、`P0-08`、`P0-09`、`P0-10` 的正式入口稳定。
- 把 `P0-03` 明确为项目内认可的补充矩阵入口，而不是孤立脚本。

### 第二阶段

- 已完成 `P0-05` 的 canonical dry-run 负例矩阵收敛。
- 已完成 `P1-04` 的 runtime actions 回归矩阵化。

### 第三阶段

- 已完成 `P1-03`、`P1-05` 与 `P2-01` 的 owner-local 套件化聚合。
- 当前 full-online 推荐执行顺序固定为：`preview suite -> dry-run suite -> HMI full-online suite -> soak profiles -> controlled HIL signed publish`。

## 8. 关联入口

- HIL 入口说明：`tests/e2e/hardware-in-loop/README.md`
- HMI smoke 退出码契约：`apps/hmi-app/docs/smoke-exit-codes.md`
- 现场验收口径：`docs/runtime/field-acceptance.md`
- 仓库级验证索引：`docs/validation/README.md`

根级 opt-in 命令：

- `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite e2e -IncludeHilCaseMatrix -ReportDir tests\reports\verify\hil-case-matrix -FailOnKnownFailure`
- `powershell -NoProfile -ExecutionPolicy Bypass -File .\ci.ps1 -Suite e2e -IncludeHilCaseMatrix -ReportDir tests\reports\ci-hil-case-matrix`
- `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\validation\run-local-validation-gate.ps1 -IncludeHilCaseMatrix`

默认门禁命令：

- `powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\e2e\hardware-in-loop\run_hil_controlled_test.ps1 -Profile Local`
- `.\release-check.ps1 -Version <x.y.z-rc.n or x.y.z>`

如需临时隔离矩阵影响，可显式传：

- `powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\e2e\hardware-in-loop\run_hil_controlled_test.ps1 -Profile Local -IncludeHilCaseMatrix:$false`
- `.\release-check.ps1 -Version <x.y.z-rc.n or x.y.z> -IncludeHilCaseMatrix:$false`
