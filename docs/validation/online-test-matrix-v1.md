# 联机测试矩阵 v1

更新时间：`2026-04-01`

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
| 在线预览证据 | `preview-verdict.json` + `plan-prepare.json` + `snapshot.json` | `tests/e2e/hardware-in-loop/` + `tests/contracts/` | HMI 展示、验证汇总 |
| HMI online 成功/失败/恢复 | `online-smoke.log` 中的 `SUPERVISOR_EVENT` / `SUPERVISOR_DIAG` | `apps/hmi-app/scripts/` + `apps/hmi-app/src/hmi_client/tools/` | `docs/validation/`、`docs/runtime/` |

约束：

- `online_ready` 的唯一真值仍来自 Supervisor 快照，不允许由 HMI 本地 UI 状态替代。
- 在线预览的唯一真值仍是 `planned_glue_snapshot` 系列工件，不允许把 `mock_synthetic` 或未知来源当作正式通过证据。
- controlled gate 只消费已冻结的 HIL 输出，不直接重解释设备行为语义。

## 3. 当前正式入口总览

| 场景簇 | 当前入口 | 最小输出 | 当前定位 |
| --- | --- | --- | --- |
| TCP 最小联机 smoke | `tests/e2e/hardware-in-loop/run_hardware_smoke.py` | `hardware-smoke` 报告、退出码 | P0 正式入口 |
| HIL 闭环 | `tests/e2e/hardware-in-loop/run_hil_closed_loop.py` | `hil-closed-loop-summary.json/md` | P0 正式入口 |
| HIL controlled gate | `tests/e2e/hardware-in-loop/run_hil_controlled_test.ps1` + `verify_hil_controlled_gate.py` | workspace / HIL / gate / release summary | P0 正式入口 |
| DXF canonical dry-run | `tests/e2e/hardware-in-loop/run_real_dxf_machine_dryrun.py` | dry-run 报告目录、状态观测工件 | P0 正式入口 |
| 在线预览证据 | `tests/e2e/hardware-in-loop/run_real_dxf_preview_snapshot.py` | `plan-prepare.json`、`snapshot.json`、`preview-verdict.json` 等 | P0 正式入口 |
| HMI online smoke | `apps/hmi-app/scripts/online-smoke.ps1` | `online-smoke.log`、qtest stdout/stderr、截图 | P0 正式入口 |
| HMI failure stage 注入 | `apps/hmi-app/scripts/verify-online-ready-timeout.ps1` | `SUPERVISOR_DIAG` / `SUPERVISOR_EVENT` 映射证据 | P0 正式入口 |
| HMI recovery | `apps/hmi-app/scripts/verify-online-recovery-loop.ps1` | 恢复前后事件与最终 `online_ready=true` | P0 正式入口 |
| 多轮 home/closed-loop matrix | `tests/e2e/hardware-in-loop/run_case_matrix.py`，或根级 `test.ps1 -IncludeHilCaseMatrix` | `case-matrix-summary.json/md` | P1 补充入口，已接 root validation opt-in，已接 controlled gate / release-check 默认门禁 |

## 4. 联机测试矩阵

### 4.1 P0：先做，直接决定联机门禁质量

| ID | 层级 | 场景 | 当前入口 | 必须证据 | 当前状态 |
| --- | --- | --- | --- | --- | --- |
| `P0-01` | L1 | TCP 最小联机 smoke | `run_hardware_smoke.py` | `hardware-smoke` 报告、退出码、stdout/stderr | `existing` |
| `P0-02` | L2 | 单轮 `start/pause/resume/stop` 闭环 | `run_hil_closed_loop.py` | `hil-closed-loop-summary.json/md`、`state_transition_checks`、`failure_context` | `existing` |
| `P0-03` | L2 | 多轮 `home + closed_loop` 稳定性 | `run_case_matrix.py`、`test.ps1 -IncludeHilCaseMatrix`、`run_hil_controlled_test.ps1` | `case-matrix-summary.json/md`、round 级 snapshots | `existing`：已接 root validation opt-in，已接 controlled gate / release-check 默认门禁 |
| `P0-04` | L3 | canonical DXF dry-run 主链 | `run_real_dxf_machine_dryrun.py` | dry-run 报告、`phase_timeline`、`verdict`、`evidence_contract` | `existing` |
| `P0-05` | L3 | 设备前提条件阻断 | `run_real_dxf_machine_dryrun.py` + `tests/e2e/first-layer/run_tcp_precondition_matrix.py` | 阻断原因、状态快照、失败上下文 | `partial`：first-layer 已有，canonical dry-run 负例矩阵未完全收敛 |
| `P0-06` | L4 | 在线预览证据主干 | `run_real_dxf_preview_snapshot.py` + `tests/contracts/test_online_preview_evidence_contract.py` | `plan-prepare.json`、`snapshot.json`、`glue_points.json`、`execution_polyline.json`、`preview-verdict.json`、`preview-evidence.md`、`hmi-preview.png`、`online-smoke.log` | `existing` |
| `P0-07` | L5 | HMI online 启动成功 | `apps/hmi-app/scripts/online-smoke.ps1` | `online-smoke.log`、截图、`SUPERVISOR_EVENT` 最小阶段序列 | `existing` |
| `P0-08` | L5 | HMI failure stage/code 识别 | `apps/hmi-app/scripts/verify-online-ready-timeout.ps1` | `failure_code`、`failure_stage`、阶段失败事件 | `existing` |
| `P0-09` | L5 | HMI recovery 闭环 | `apps/hmi-app/scripts/verify-online-recovery-loop.ps1` | `stage_failed`、`recovery_invoked`、`stage_succeeded@online_ready`、最终 `online_ready=true` | `existing` |
| `P0-10` | L6 | controlled gate 基本门禁 | `run_hil_controlled_test.ps1` + `verify_hil_controlled_gate.py` | `hil-controlled-gate-summary.json/md` | `existing` |

### 4.2 P1：随后补深度

| ID | 层级 | 场景 | 建议落点 | 当前状态 |
| --- | --- | --- | --- | --- |
| `P1-01` | L2 | 断连恢复 | 新增或扩展 HIL TCP recovery 脚本 | `planned` |
| `P1-02` | L3 | 门/急停/限位阻断专项 | 扩 `run_real_dxf_machine_dryrun.py` 的负例参数矩阵 | `planned` |
| `P1-03` | L4 | DXF 基线集批量在线预览 | 扩 `run_real_dxf_preview_snapshot.py` 批量模式 | `planned` |
| `P1-04` | L5 | HMI runtime actions 批量在线回归 | 扩 `online-smoke.ps1` / `ui_qtest.py` 参数矩阵 | `partial` |
| `P1-05` | L6 | 30 分钟以上 soak | `run_hil_closed_loop.py --duration-seconds` + gate 汇总 | `existing` 但未纳入统一矩阵文档前一直视为专项 |

### 4.3 P2：发布前与容量边界

| ID | 层级 | 场景 | 建议落点 | 当前状态 |
| --- | --- | --- | --- | --- |
| `P2-01` | L3/L4 | 多 DXF 回归套件 | `tests/e2e/hardware-in-loop/suites/` 或批量脚本参数化 | `planned` |
| `P2-02` | L5/L6 | HMI 故障恢复夜间回归 | nightly suite 脚本 + suite summary | `planned` |
| `P2-03` | 真机 E2E | 发布前最小真机主链 | 现场记录文档 + 受限执行 SOP | `planned` |

## 5. 必须证据标准

### 5.1 HIL 状态机类

至少保留：

- `hil-closed-loop-summary.json`
- `hil-closed-loop-summary.md`
- `state_transition_checks`
- `failure_context`
- `recent_status_snapshot`

### 5.2 在线预览类

至少保留：

- `plan-prepare.json`
- `snapshot.json`
- `glue_points.json`
- `execution_polyline.json`
- `preview-verdict.json`
- `preview-evidence.md`
- `hmi-preview.png`
- `online-smoke.log`

### 5.3 HMI 故障/恢复类

至少保留：

- qtest stdout/stderr
- screenshot
- `online-smoke.log`
- `failure_code` / `failure_stage` 断言结果
- 恢复前后 `SUPERVISOR_EVENT` / `SUPERVISOR_DIAG`

### 5.4 controlled gate 类

至少保留：

- `workspace-validation.json/md`
- `hil-closed-loop-summary.json/md`
- `hil-controlled-gate-summary.json/md`
- `hil-controlled-release-summary.md`

## 6. 项目落地规则

1. 新的联机场景优先扩现有入口参数，不优先新增新的总入口脚本。
2. 正式矩阵定义沉到 `docs/validation/`；脚本入口说明沉到各自 owner README。
3. 只有当场景输出已经形成稳定 `authority artifact` 时，才允许接入 `verify_hil_controlled_gate.py` 或发布流程。
4. `tests/contracts/` 与 `tests/e2e/first-layer/` 继续承担离线契约与负例验证；不得让联机脚本独自承担契约真值。
5. 任何需要真实设备动作的新增场景，先补观察点、证据和停机条件，再补脚本。

## 7. 推荐推进顺序

### 第一阶段

- 维持 `P0-01`、`P0-02`、`P0-04`、`P0-06`、`P0-07`、`P0-08`、`P0-09`、`P0-10` 的正式入口稳定。
- 把 `P0-03` 明确为项目内认可的补充矩阵入口，而不是孤立脚本。

### 第二阶段

- 收敛 `P0-05` 的 canonical dry-run 负例矩阵。
- 把 `P1-04` 的 runtime actions 回归从单点 smoke 扩成参数矩阵。

### 第三阶段

- 再推进 `P1-01`、`P1-03`、`P1-05` 与 `P2` 套件化建设。

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
