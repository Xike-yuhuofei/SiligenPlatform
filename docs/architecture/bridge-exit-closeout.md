# Bridge Exit Closeout

更新时间：`2026-04-07`

## 1. 目标

- 终态要求是零 bridge root、零 bridge metadata、零默认 fallback。
- `dsp-e2e-spec` 是唯一正式冻结事实源；本页只记录 closeout 责任分工与证据回链。

## 2. 责任分工

| 责任面 | 正式入口 | 必须失败的情况 |
|---|---|---|
| 冻结事实源 | `scripts/migration/validate_dsp_e2e_spec_docset.py`、`tests/contracts/test_freeze_docset_contract.py` | 缺文档、缺索引、缺根级引用、出现第二正式冻结入口 |
| owner 面与模块骨架 | `scripts/migration/validate_workspace_layout.py`、`tests/contracts/test_migration_alignment_matrix.py` | 缺模块 owner 面、矩阵缺列缺行、bridge metadata 或 bridge 根仍存在 |
| legacy 清除 | `scripts/migration/legacy-exit-checks.py`、`tests/contracts/test_bridge_exit_contract.py` | 旧顶层根、bridge 根、bridge 文本引用、默认 bridge 入口仍存在 |
| 根级门禁 | `build.ps1`、`test.ps1`、`ci.ps1`、`scripts/validation/run-local-validation-gate.ps1` | 未执行 layout/docset/legacy-exit/contracts gate，或任何 gate 非零退出 |

## 3. 已清除桥接面

| 资产族 | 已清除对象 |
|---|---|
| workflow | `modules/workflow/src`; `modules/workflow/process-runtime-core`; `modules/workflow/application/usecases-bridge` |
| dxf-geometry | `modules/dxf-geometry/src`; `modules/dxf-geometry/engineering-data`; `scripts/engineering-data-bridge` |
| runtime-execution | `modules/runtime-execution/device-contracts`; `modules/runtime-execution/device-adapters`; `modules/runtime-execution/runtime-host` |
| runtime contracts alias shell | `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/configuration/IConfigurationPort.h`; `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/dispensing/ITaskSchedulerPort.h`; `modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/system/IEventPublisherPort.h` |
| 其余模块 src bridge | `modules/job-ingest/src`; `modules/topology-feature/src`; `modules/process-planning/src`; `modules/coordinate-alignment/src`; `modules/process-path/src`; `modules/motion-planning/src`; `modules/dispense-packaging/src`; `modules/trace-diagnostics/src`; `modules/hmi-application/src` |
| bridge metadata | `module.yaml` 中 `bridge_roots` / `bridge_mode: active`; `CMakeLists.txt` 中 `SILIGEN_BRIDGE_ROOTS` / `SILIGEN_MIGRATION_SOURCE_ROOTS` |

## 4. 最新验证结果（2026-03-25）

| 证据 | 实际结论 |
|---|---|
| `tests/reports/workspace-validation.md` | `passed=15`、`failed=0`、`known_failure=0`、`skipped=0`，且包含 freeze/matrix/bridge-exit/application/engineering 合同测试通过 |
| `tests/reports/legacy-exit-current/legacy-exit-checks.md` | `finding_count=0` |
| `tests/reports/local-validation-gate/20260325-212745/local-validation-gate-summary.md` | `overall_status=passed`、`passed_steps=6/6` |
| `tests/reports/dsp-e2e-spec-docset/dsp-e2e-spec-docset.md` | `missing_doc_count=0` 且 `finding_count=0` |

## 5. Closeout 判定

- 任何 live 目录、脚本或文档只要仍把 bridge/fallback 写成默认入口，本次 closeout 即失败。
- `validate_workspace_layout.py` 与 `legacy-exit-checks.py` 不再接受“bridge shell-only 仍可通过”的旧语义。
- 本次收口完成后，所有后续变更都必须直接落在 canonical roots，不允许再恢复桥接层。
- 截至 `2026-04-07`，`runtime-execution/contracts/runtime` 已从 canonical required surface 中移除 `IConfigurationPort` / `ITaskSchedulerPort` / `IEventPublisherPort` alias shell，live include 也已清零；该问题不再构成运行链默认入口。
- 截至 `2026-04-07`，machine execution state 已收口为 `runtime/system/DispenserModelMachineExecutionStateBackend.*` 直接实现 `IMachineExecutionStatePort`，并通过 `modules/workflow/domain/include/domain/machine/aggregates/DispenserModel.h` 提供 `Aggregates::DispenserModel` / `Aggregates::DispensingTask` canonical alias；`runtime-execution` 已不再把 `Legacy::DispenserModel` / `Legacy::DispensingTask` 作为 live surface 暴露。
- 截至 `2026-04-07`，由于 `runtime-execution` / `runtime-service` 仍在进行 owner 收口、历史命名清理与其余文档同步，本次 bridge exit closeout 必须维持为 `NOT PASS`，不得继续对外宣称完成。