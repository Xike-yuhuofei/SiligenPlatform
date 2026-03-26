# Wave Mapping

## Purpose

本文件定义本特性的正式迁移波次索引，用于把 `Wave 0-6`、`US1-US7`、`M0-M11`、canonical roots 与 legacy migration-source roots 统一映射到同一套收口口径。

正式事实源：

- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\spec.md`
- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\plan.md`
- `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\`

## Canonical Roots Baseline

| Root | 终态角色 | 说明 |
|---|---|---|
| `apps/` | 进程入口与装配面 | 只保留宿主、启动、发布和组合职责，不承担终态业务 owner。 |
| `modules/` | 业务 owner 面 | `M0-M11` 的唯一终态模块根。 |
| `shared/` | 稳定公共层 | 只承载低业务含义基础设施和跨模块稳定契约。 |
| `scripts/` | 自动化入口 | 承接原 `tools/` 中的正式脚本实现。 |
| `tests/` | 仓库级验证面 | 承接原 `integration/` 的正式验证入口。 |
| `samples/` | 稳定样本面 | 承接原 `examples/` 的正式样本与 golden 资产。 |
| `deploy/` | 部署承载面 | 预留正式部署与环境投放资产。 |
| `docs/` | 冻结文档与 closeout 证据 | 包含架构冻结、验收、迁移报告。 |

## Legacy Root to Canonical Root Inventory

| Legacy surface | Canonical destination | 过渡形态 | 退出波次 | 说明 |
|---|---|---|---|---|
| `packages/shared-kernel/` | `shared/kernel/`、`shared/ids/`、`shared/artifacts/`、`shared/messaging/`、`shared/logging/` | CMake wrapper、forwarding include | `Wave 1` | 公共层先变真，旧包只保留转发。 |
| `packages/application-contracts/` | `shared/contracts/application/`、`modules/job-ingest/contracts/`、`modules/hmi-application/contracts/` | README + wrapper | `Wave 1-5` | 稳定契约进 `shared/`，owner 特定契约回模块。 |
| `packages/engineering-contracts/` | `shared/contracts/engineering/` | README + thin bridge | `Wave 1` | 工程稳定 schema 不再留在 legacy 包。 |
| `packages/device-contracts/` | `shared/contracts/device/`、`modules/runtime-execution/contracts/` | README + wrapper | `Wave 1-4` | 跨模块稳定契约与运行时 owner 契约拆分。 |
| `packages/test-kit/` | `shared/testing/` | CMake wrapper、包级兼容导出 | `Wave 1` | 仓库级测试支撑统一下沉。 |
| `tools/` | `scripts/` | root wrapper、入口转发脚本 | `Wave 1`，`Wave 6` 收口 | 根级入口保持不变，正式实现迁入 `scripts/`。 |
| `packages/engineering-data/` | `modules/dxf-geometry/`、`modules/topology-feature/` | README + module shim | `Wave 2` | 上游静态工程链真实 owner 收敛。 |
| `packages/process-runtime-core/` | `modules/workflow/`、`modules/process-planning/`、`modules/coordinate-alignment/`、`modules/process-path/`、`modules/motion-planning/`、`modules/dispense-packaging/` | thin CMake bridge | `Wave 3` | 禁止整包平移，必须按 owner 拆分。 |
| `packages/runtime-host/` | `modules/runtime-execution/`、`apps/runtime-service/` | 宿主 wrapper | `Wave 4` | 运行态 owner 与宿主壳分离。 |
| `packages/transport-gateway/` | `modules/runtime-execution/adapters/`、`apps/runtime-gateway/` | gateway wrapper | `Wave 4` | 设备/协议接入与宿主网关拆分。 |
| `packages/device-adapters/` | `modules/runtime-execution/adapters/` | adapter bridge | `Wave 4` | 执行适配归位到运行时链。 |
| `packages/traceability-observability/` | `modules/trace-diagnostics/`、`shared/logging/` | README + forwarding shim | `Wave 5` | 追溯 owner 与通用日志基础设施拆开。 |
| `integration/` | `tests/integration/`、`tests/e2e/`、`tests/performance/` | README redirect、test harness forwarder | `Wave 5` | 仓库级验证面转正到 `tests/`。 |
| `examples/` | `samples/`、`samples/golden/`、`samples/dxf/` | README redirect、tombstone | `Wave 5-6` | 稳定样本不再留在 `examples/`。 |
| `apps/control-cli/` | `apps/planner-cli/`、`modules/job-ingest/`、`modules/hmi-application/` | host wrapper | `Wave 2-5` | CLI 仅保留入口，业务 owner 回模块。 |
| `apps/control-runtime/` | `apps/runtime-service/` | app wrapper | `Wave 4` | 老入口降级为兼容壳。 |
| `apps/control-tcp-server/` | `apps/runtime-gateway/` | app wrapper | `Wave 4` | 老 TCP 入口降级为兼容壳。 |

## Wave Overview

| Wave | 对应故事 | 主目标 | 主范围 | 关键交付件 | 波次退出信号 |
|---|---|---|---|---|---|
| `Wave 0` | `US1` 基线治理 | 冻结迁移术语、阶段链、映射索引和门禁矩阵 | 正式 spec/plan/tasks、冻结文档索引、迁移治理文件 | `wave-mapping.md`、`module-cutover-checklist.md`、`validation-gates.md` | 所有后续波次都可引用统一术语和统一 closeout 口径。 |
| `Wave 1` | `US2` canonical roots | 让 `shared/`、`scripts/`、`tests/`、`samples/`、`deploy/` 成为正式承载面 | `shared/`、`scripts/`、根级脚本、layout validators | canonical root 文档、shared/script/test support 入口 | 新根先变真，`tools/` 和公共 legacy 包只剩 wrapper。 |
| `Wave 2` | `US3` 上游静态链 | 收敛 `M1-M3`，稳定规划链输入事实 | `modules/job-ingest/`、`modules/dxf-geometry/`、`modules/topology-feature/`、对应样本与测试 | 上游模块入口、contracts、samples、tests | `M1-M3` owner 不再依赖 `packages/engineering-data` 为真实承载面。 |
| `Wave 3` | `US4` 工作流与规划链 | 从 `process-runtime-core` 中拆出 `M0/M4-M8` | `modules/workflow/`、`modules/process-planning/`、`modules/coordinate-alignment/`、`modules/process-path/`、`modules/motion-planning/`、`modules/dispense-packaging/` | 六个规划链模块入口与边界文档 | `process-runtime-core` 不再承担规划链终态 owner。 |
| `Wave 4` | `US5` 运行时执行链 | 隔离 `M9` 风险并完成运行时宿主切换 | `modules/runtime-execution/`、`apps/runtime-service/`、`apps/runtime-gateway/`、设备契约/适配 | 运行时 owner、service/gateway 入口、控制语义同步 | 运行时链只消费上游结果，不再越权回写。 |
| `Wave 5` | `US6` 追溯/HMI/验证面 | 收敛 `M10-M11` 与 `tests/`、`samples/` 终态承载面 | `modules/trace-diagnostics/`、`modules/hmi-application/`、`tests/`、`samples/` | trace/HMI 模块、测试入口、样本索引 | 展示、追溯、验证和样本正式落位。 |
| `Wave 6` | `US7` 根级 cutover | 切换 canonical build graph 并完成 legacy exit | 根级 `CMakeLists.txt`、`apps/CMakeLists.txt`、`tests/CMakeLists.txt`、`cmake/workspace-layout.env` | canonical graph、legacy exit 报告、closeout 证据 | `packages/`、`integration/`、`tools/`、`examples/` 不再承担终态 owner。 |

## Wave Closeout Ledger

| Wave | Story | Status | Evidence sink |
|---|---|---|---|
| `Wave 0` | `US1` | `completed` | `integration/reports/dsp-e2e-spec-docset/us1-review-wave0.md` |
| `Wave 1` | `US2` | `completed` | `integration/reports/dsp-e2e-spec-docset/us2-review-wave1.md`、`integration/reports/workspace-layout/us2-review-wave1-layout.txt` |
| `Wave 2` | `US3` | `completed` | `integration/reports/dsp-e2e-spec-docset/us3-review-wave2.md`、`integration/reports/workspace-layout/us3-review-wave2-layout.txt` |
| `Wave 3` | `US4` | `completed` | `integration/reports/dsp-e2e-spec-docset/us4-review-wave3.md`、`integration/reports/workspace-layout/us4-review-wave3-layout.txt` |
| `Wave 4` | `US5` | `completed` | `integration/reports/dsp-e2e-spec-docset/us5-review-wave4.md`、`integration/reports/workspace-layout/us5-review-wave4-layout.txt`、`integration/reports/workspace-runtime/us5-review-wave4-transport-gateway-compat.txt`、`integration/reports/workspace-runtime/us5-review-wave4-build.txt` |
| `Wave 5` | `US6` | `completed` | `integration/reports/dsp-e2e-spec-docset/us6-review-wave5.md`、`integration/reports/workspace-layout/us6-review-wave5-layout.txt`、`integration/reports/workspace-validation-us6/workspace-validation.md`、`integration/reports/dsp-e2e-spec-docset/us6-review-wave5-phase-end-check.md`、`integration/reports/workspace-layout/us6-review-wave5-phase-end-check.txt`、`integration/reports/workspace-validation-us6-phase-end-check/workspace-validation.md` |
| `Wave 6` | `US7` | `completed` | `integration/reports/workspace-layout/us7-review-wave6-layout.txt`、`integration/reports/dsp-e2e-spec-docset/us7-review-wave6.md`、`integration/reports/workspace-build/us7-review-wave6-final-build.txt`、`integration/reports/workspace-test-wave6-rerun/workspace-validation.md`、`integration/reports/workspace-ci-wave6/workspace-validation.md`、`integration/reports/legacy-exit-wave6/legacy-exit-checks.md`、`integration/reports/local-validation-gate-wave6/20260324-211140/local-validation-gate-summary.md` |

### Wave 0 / US1 Closeout Detail

- gate command: `python .\scripts\migration\validate_dsp_e2e_spec_docset.py --wave "Wave 0" --report-dir .\integration\reports\dsp-e2e-spec-docset --report-stem us1-review-wave0`
- gate result: `PASS` (`missing_doc_count=0`, `finding_count=0`)
- frozen axes in scope: `S01`、`S02`、`S03`、`S10`
- review entry and terminology check: `docs/architecture/build-and-test.md`
- reference-to-frozen index mapping anchors: `docs/architecture/dsp-e2e-spec/README.md`、`docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s10-frozen-directory-index.md`
- note: `Wave 0` closeout 已成立；该条为历史阶段说明，后续 `Wave 1-6` 已全部完成，整体 feature 已 terminal closeout。

### Wave 1 / US2 Closeout Detail

- layout gate command: `python .\tools\migration\validate_workspace_layout.py --wave "Wave 1"`
- layout gate result: `PASS` (`missing_key_count=0`, `missing_path_count=0`, `failed_skeleton_count=0`, `failed_wiring_count=0`, `failed_freeze_count=0`)
- layout gate evidence: `integration/reports/workspace-layout/us2-review-wave1-layout.txt`
- freeze gate command: `python .\scripts\migration\validate_dsp_e2e_spec_docset.py --wave "Wave 1" --report-dir .\integration\reports\dsp-e2e-spec-docset --report-stem us2-review-wave1`
- freeze gate result: `PASS` (`missing_doc_count=0`, `finding_count=0`)
- freeze gate evidence: `integration/reports/dsp-e2e-spec-docset/us2-review-wave1.md`
- shared root configure checks: `cmake -S .\shared\kernel -B .\build\_review_phase4_shared_kernel`、`cmake -S .\shared\testing -B .\build\_review_phase4_shared_testing`
- shared root configure evidence: `integration/reports/cmake-config/us2-review-wave1-shared-kernel.txt`、`integration/reports/cmake-config/us2-review-wave1-shared-testing.txt`
- note: `Wave 1` closeout 证据已回写，且 Phase 4 End Check 已通过；该条为历史阶段说明，后续 `Wave 2-6` 已全部完成。

### Wave 2 / US3 Closeout Detail

- layout gate command: `python .\tools\migration\validate_workspace_layout.py --wave "Wave 2"`
- layout gate result: `PASS` (`missing_key_count=0`, `missing_path_count=0`, `failed_skeleton_count=0`, `failed_wiring_count=0`, `failed_freeze_count=0`)
- layout gate evidence: `integration/reports/workspace-layout/us3-review-wave2-layout.txt`
- freeze gate command: `python .\scripts\migration\validate_dsp_e2e_spec_docset.py --wave "Wave 2" --report-dir .\integration\reports\dsp-e2e-spec-docset --report-stem us3-review-wave2`
- freeze gate result: `PASS` (`missing_doc_count=0`, `finding_count=0`)
- freeze gate evidence: `integration/reports/dsp-e2e-spec-docset/us3-review-wave2.md`
- owner entry anchors: `modules/job-ingest/CMakeLists.txt`、`modules/job-ingest/contracts/README.md`、`modules/dxf-geometry/CMakeLists.txt`、`modules/dxf-geometry/contracts/README.md`、`modules/topology-feature/CMakeLists.txt`、`modules/topology-feature/contracts/README.md`
- sample and validation anchors: `samples/dxf/README.md`、`samples/golden/README.md`、`examples/README.md`、`tests/integration/README.md`
- freeze boundary anchors: `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s04-stage-artifact-dictionary.md`、`docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s05-module-boundary-interface-contract.md`
- note: `Wave 2` closeout 证据已回写，且 Phase 5 End Check 已通过；该条为历史阶段说明，后续 `Wave 3-6` 已全部完成。

### Wave 3 / US4 Closeout Detail

- layout gate command: `python .\tools\migration\validate_workspace_layout.py --wave "Wave 3"`
- layout gate result: `PASS` (`missing_key_count=0`, `missing_path_count=0`, `failed_skeleton_count=0`, `failed_wiring_count=0`, `failed_freeze_count=0`)
- layout gate evidence: `integration/reports/workspace-layout/us4-review-wave3-layout.txt`
- freeze gate command: `python .\scripts\migration\validate_dsp_e2e_spec_docset.py --wave "Wave 3" --report-dir .\integration\reports\dsp-e2e-spec-docset --report-stem us4-review-wave3`
- freeze gate result: `PASS` (`missing_doc_count=0`, `finding_count=0`)
- freeze gate evidence: `integration/reports/dsp-e2e-spec-docset/us4-review-wave3.md`
- owner entry anchors: `modules/workflow/CMakeLists.txt`、`modules/process-planning/CMakeLists.txt`、`modules/coordinate-alignment/CMakeLists.txt`、`modules/process-path/CMakeLists.txt`、`modules/motion-planning/CMakeLists.txt`、`modules/dispense-packaging/CMakeLists.txt`
- freeze boundary anchors: `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s04-stage-artifact-dictionary.md`、`docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s05-module-boundary-interface-contract.md`
- note: `Wave 3` closeout 证据已回写，且 Phase 6 End Check 已通过；该条为历史阶段说明，后续 `Wave 4-6` 已全部完成。

### Wave 4 / US5 Closeout Detail

- layout gate command: `python .\tools\migration\validate_workspace_layout.py --wave "Wave 4"`
- layout gate result: `PASS` (`missing_key_count=0`, `missing_path_count=0`, `failed_skeleton_count=0`, `failed_wiring_count=0`, `failed_freeze_count=0`)
- layout gate evidence: `integration/reports/workspace-layout/us5-review-wave4-layout.txt`
- freeze gate command: `python .\scripts\migration\validate_dsp_e2e_spec_docset.py --wave "Wave 4" --report-dir .\integration\reports\dsp-e2e-spec-docset --report-stem us5-review-wave4`
- freeze gate result: `PASS` (`missing_doc_count=0`, `finding_count=0`)
- freeze gate evidence: `integration/reports/dsp-e2e-spec-docset/us5-review-wave4.md`
- runtime compatibility command: `python packages/transport-gateway/tests/test_transport_gateway_compatibility.py`
- runtime compatibility result: `PASS`
- runtime compatibility evidence: `integration/reports/workspace-runtime/us5-review-wave4-transport-gateway-compat.txt`
- runtime build commands: `cmake -S . -B .codex-tmp/phase7-review-cmake`、`cmake --build .codex-tmp/phase7-review-cmake --config Debug --target siligen_control_runtime siligen_tcp_server -j 1`
- runtime build result: `PASS`
- runtime build evidence: `integration/reports/workspace-runtime/us5-review-wave4-build.txt`
- phase end check layout evidence: `integration/reports/workspace-layout/us5-review-wave4-phase-end-check.txt`
- phase end check freeze evidence: `integration/reports/dsp-e2e-spec-docset/us5-review-wave4-phase-end-check.md`
- phase end check runtime compatibility evidence: `integration/reports/workspace-runtime/us5-review-wave4-phase-end-check-transport-gateway-compat.txt`
- phase end check runtime build evidence: `integration/reports/workspace-runtime/us5-review-wave4-phase-end-check-build.txt`
- owner entry anchors: `modules/runtime-execution/CMakeLists.txt`、`apps/runtime-service/CMakeLists.txt`、`apps/runtime-gateway/CMakeLists.txt`
- migration-source downgrade anchors: `apps/control-runtime/README.md`、`apps/control-tcp-server/README.md`、`packages/runtime-host/README.md`、`packages/transport-gateway/README.md`、`packages/device-contracts/README.md`、`packages/device-adapters/README.md`
- freeze boundary anchors: `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s07-state-machine-command-bus.md`、`docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s08-system-sequence-template.md`
- note: `Wave 4` closeout 证据已回写，且 Phase 7 End Check 已通过；该条为历史阶段说明，后续 `Wave 5-6` 已全部完成。

### Wave 5 / US6 Closeout Detail

- layout gate command: `python .\tools\migration\validate_workspace_layout.py --wave "Wave 5"`
- layout gate result: `PASS` (`missing_key_count=0`, `missing_path_count=0`, `failed_skeleton_count=0`, `failed_wiring_count=0`, `failed_freeze_count=0`)
- layout gate evidence: `integration/reports/workspace-layout/us6-review-wave5-layout.txt`
- freeze gate command: `python .\scripts\migration\validate_dsp_e2e_spec_docset.py --wave "Wave 5" --report-dir .\integration\reports\dsp-e2e-spec-docset --report-stem us6-review-wave5`
- freeze gate result: `PASS` (`missing_doc_count=0`, `finding_count=0`)
- freeze gate evidence: `integration/reports/dsp-e2e-spec-docset/us6-review-wave5.md`
- workspace validation command: `python -m test_kit.workspace_validation --profile ci --suite packages --report-dir integration/reports/workspace-validation-us6`
- workspace validation result: `PASS` (`passed=17`, `failed=0`, `known_failure=0`, `skipped=0`)
- workspace validation evidence: `integration/reports/workspace-validation-us6/workspace-validation.md`
- phase end check layout evidence: `integration/reports/workspace-layout/us6-review-wave5-phase-end-check.txt`
- phase end check freeze evidence: `integration/reports/dsp-e2e-spec-docset/us6-review-wave5-phase-end-check.md`
- phase end check workspace validation evidence: `integration/reports/workspace-validation-us6-phase-end-check/workspace-validation.md`
- owner entry anchors: `modules/trace-diagnostics/CMakeLists.txt`、`modules/hmi-application/CMakeLists.txt`、`tests/CMakeLists.txt`、`samples/README.md`
- host and migration-source downgrade anchors: `apps/hmi-app/README.md`、`apps/control-cli/README.md`、`apps/trace-viewer/README.md`、`apps/README.md`、`packages/traceability-observability/README.md`、`integration/README.md`、`examples/README.md`
- freeze boundary anchors: `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s09-test-matrix-acceptance-baseline.md`、`docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s10-frozen-directory-index.md`
- note: `Wave 5` closeout 证据已回写，且 Phase 8 End Check 已通过；该条为历史阶段说明，后续 `Wave 6` 已完成。

### Wave 6 / US7 Closeout Detail

- layout gate command: `python .\tools\migration\validate_workspace_layout.py`
- layout gate result: `PASS`
- layout gate evidence: `integration/reports/workspace-layout/us7-review-wave6-layout.txt`
- freeze gate command: `python .\scripts\migration\validate_dsp_e2e_spec_docset.py --report-dir integration/reports/dsp-e2e-spec-docset --report-stem us7-review-wave6`
- freeze gate result: `PASS` (`missing_doc_count=0`, `finding_count=0`)
- freeze gate evidence: `integration/reports/dsp-e2e-spec-docset/us7-review-wave6.md`
- root build command: `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile Local`
- root build result: `PASS`
- root build evidence: `integration/reports/workspace-build/us7-review-wave6-final-build.txt`
- workspace validation command: `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile Local -ReportDir integration\reports\workspace-test-wave6-rerun`
- workspace validation result: `PASS` (`passed=52`, `failed=0`, `known_failure=0`, `skipped=0`)
- workspace validation evidence: `integration/reports/workspace-test-wave6-rerun/workspace-validation.md`
- CI replay command: `powershell -NoProfile -ExecutionPolicy Bypass -File .\ci.ps1 -ReportDir integration\reports\workspace-ci-wave6`
- CI replay result: `PASS` (`passed=47`, `failed=0`, `known_failure=0`, `skipped=0`)
- CI replay evidence: `integration/reports/workspace-ci-wave6/workspace-validation.md`
- local validation gate command: `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\scripts\run-local-validation-gate.ps1 -ReportRoot integration\reports\local-validation-gate-wave6`
- local validation gate result: `PASS`
- local validation gate evidence: `integration/reports/local-validation-gate-wave6/20260324-211140/local-validation-gate-summary.md`
- dedicated legacy exit command: `python .\scripts\migration\legacy-exit-checks.py --profile local --report-dir integration\reports\legacy-exit-wave6`
- dedicated legacy exit result: `PASS` (`failed_rules=0`, `findings=0`)
- dedicated legacy exit evidence: `integration/reports/legacy-exit-wave6/legacy-exit-checks.md`
- root cutover anchors: `CMakeLists.txt`、`apps/CMakeLists.txt`、`tests/CMakeLists.txt`、`cmake/workspace-layout.env`
- migration-source downgrade anchors: `packages/README.md`、`integration/README.md`、`tools/README.md`、`examples/README.md`
- closeout evidence anchors: `docs/architecture/removed-legacy-items-final.md`、`docs/architecture/system-acceptance-report.md`、`docs/architecture/workspace-baseline.md`
- note: `Wave 6` closeout 证据已回写；根级 canonical build graph、legacy exit、Phase 9 End Check 与最终 closeout 均已成立，整项 feature 已进入 terminal closeout。

## Module to Wave Allocation

| Module | Target owner path | Primary legacy source | Wave | Closeout artifact |
|---|---|---|---|---|
| `M0 workflow` | `modules/workflow/` | `packages/process-runtime-core/` | `Wave 3` | `dsp-e2e-spec-s05`、`system-acceptance-report.md` |
| `M1 job-ingest` | `modules/job-ingest/` | `apps/control-cli/`、`packages/application-contracts/` | `Wave 2` | `dsp-e2e-spec-s04`、`tests/integration/` |
| `M2 dxf-geometry` | `modules/dxf-geometry/` | `packages/engineering-data/` | `Wave 2` | `dsp-e2e-spec-s04`、`samples/dxf/` |
| `M3 topology-feature` | `modules/topology-feature/` | `packages/engineering-data/` | `Wave 2` | `dsp-e2e-spec-s04`、`samples/golden/` |
| `M4 process-planning` | `modules/process-planning/` | `packages/process-runtime-core/` | `Wave 3` | `dsp-e2e-spec-s05` |
| `M5 coordinate-alignment` | `modules/coordinate-alignment/` | `packages/process-runtime-core/` | `Wave 3` | `dsp-e2e-spec-s05` |
| `M6 process-path` | `modules/process-path/` | `packages/process-runtime-core/` | `Wave 3` | `dsp-e2e-spec-s05` |
| `M7 motion-planning` | `modules/motion-planning/` | `packages/process-runtime-core/` | `Wave 3` | `dsp-e2e-spec-s05` |
| `M8 dispense-packaging` | `modules/dispense-packaging/` | `packages/process-runtime-core/` | `Wave 3` | `dsp-e2e-spec-s05` |
| `M9 runtime-execution` | `modules/runtime-execution/` | `packages/runtime-host/`、`packages/device-adapters/`、`packages/transport-gateway/` | `Wave 4` | `dsp-e2e-spec-s07`、`dsp-e2e-spec-s08` |
| `M10 trace-diagnostics` | `modules/trace-diagnostics/` | `packages/traceability-observability/` | `Wave 5` | `dsp-e2e-spec-s09` |
| `M11 hmi-application` | `modules/hmi-application/` | `apps/hmi-app/`、`apps/control-cli/` | `Wave 5` | `dsp-e2e-spec-s09`、`dsp-e2e-spec-s10` |

## Evidence Sink Baseline

| Evidence type | Primary sink |
|---|---|
| 波次盘点与 closeout 状态 | `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\wave-mapping.md` |
| 模块 cutover 判定 | `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\module-cutover-checklist.md` |
| 波次门禁与命令证据 | `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\validation-gates.md` |
| 冻结文档与架构事实 | `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\` |
| 最终验收与 closeout | `D:\Projects\SS-dispense-align\docs\architecture\system-acceptance-report.md` |
