# Module Cutover Checklist

## Purpose

本文件定义 `M0-M11` 的模块级 cutover 判定列。每个模块只有在 owner 路径、契约边界、构建入口、验证入口、legacy 降级和 closeout 证据六类条件同时满足后，才允许宣告完成 cutover。

## Cutover Columns

| Column | 含义 |
|---|---|
| `Owner path ready` | 目标模块目录、`CMakeLists.txt`、`README.md`、`contracts/README.md` 已存在且表达终态 owner。 |
| `Boundary frozen` | 模块职责、输入输出对象、允许依赖方向已写入冻结文档或模块契约说明。 |
| `Build wired` | 根级或上层 `CMakeLists.txt` 已可解析到新模块入口，旧入口不再承载真实实现。 |
| `Validation wired` | 对应测试、样本、layout validator 或 freeze validator 已覆盖新 owner 路径。 |
| `Legacy downgraded` | 原承载路径已退化为 wrapper、forwarder、README 或 tombstone。 |
| `Evidence captured` | `wave-mapping.md`、`validation-gates.md`、`system-acceptance-report.md` 至少一处记录了该模块 closeout 证据。 |

## Module Matrix

| Module | Target owner path | Primary migration sources | Wave | Owner path ready | Boundary frozen | Build wired | Validation wired | Legacy downgraded | Evidence captured | Current status |
|---|---|---|---|---|---|---|---|---|---|---|
| `M0 workflow` | `modules/workflow/` | `packages/process-runtime-core/src/application` | `Wave 3` | `CMakeLists.txt`、`README.md`、`contracts/README.md` 齐备且只承载流程编排，且 `CMakeLists.txt` 已声明 `M0` owner 元数据 | `dsp-e2e-spec-s05` 定义其 owner 边界 | 上层构建图引用 `modules/workflow` | layout validator 与规划链冻结文档覆盖该路径 | `process-runtime-core` 不再拥有工作流事实 | `wave-mapping.md`、`module-cutover-checklist.md`、`system-acceptance-report.md` | `Completed` |
| `M1 job-ingest` | `modules/job-ingest/` | `apps/control-cli/`、`packages/application-contracts/` | `Wave 2` | 新模块承载任务接入与输入校验 owner，`CMakeLists.txt`/`README.md`/`contracts/README.md` 已齐备 | `dsp-e2e-spec-s04/s05` 明确任务输入对象边界 | 上游构建与 contracts 指向 `modules/job-ingest` | `tests/integration/README.md` 与 `validate_workspace_layout.py --wave "Wave 2"` 覆盖输入事实验证 | `control-cli` 只保留入口，不再拥有任务事实 | `wave-mapping.md`、`module-cutover-checklist.md`、`system-acceptance-report.md` | `Completed` |
| `M2 dxf-geometry` | `modules/dxf-geometry/` | `packages/engineering-data/` | `Wave 2` | 几何解析入口、contracts 和 README 已就位，且 `CMakeLists.txt` 已声明 `M2` owner 元数据 | 几何对象和输入引用规则已冻结 | 构建图直接包含 `modules/dxf-geometry` | `samples/dxf/` 与 `validate_workspace_layout.py --wave "Wave 2"` 覆盖新 owner | `engineering-data` 不再是真实几何 owner | `wave-mapping.md`、`module-cutover-checklist.md`、`system-acceptance-report.md` | `Completed` |
| `M3 topology-feature` | `modules/topology-feature/` | `packages/engineering-data/` | `Wave 2` | 特征提取 owner 入口已就位，且 `CMakeLists.txt` 已声明 `M3` owner 元数据 | 拓扑/特征对象边界已冻结 | 构建图直接包含 `modules/topology-feature` | `samples/golden/` 与 `validate_workspace_layout.py --wave "Wave 2"` 覆盖新 owner | `engineering-data` 不再是真实拓扑 owner | `wave-mapping.md`、`module-cutover-checklist.md`、`system-acceptance-report.md` | `Completed` |
| `M4 process-planning` | `modules/process-planning/` | `packages/process-runtime-core/planning`、`packages/process-runtime-core/src/domain` | `Wave 3` | 规划模块入口与 contracts 已就位，且 `CMakeLists.txt` 已声明 `M4` owner 元数据 | 规划对象、允许依赖方向已冻结 | 构建图直接引用 `modules/process-planning` | 规划链验证与 freeze 文档引用新路径 | `process-runtime-core` 不再拥有规划事实 | `wave-mapping.md`、`module-cutover-checklist.md`、`system-acceptance-report.md` | `Completed` |
| `M5 coordinate-alignment` | `modules/coordinate-alignment/` | `packages/process-runtime-core/planning`、`packages/process-runtime-core/machine` | `Wave 3` | 对齐模块入口与 contracts 已就位，且 `CMakeLists.txt` 已声明 `M5` owner 元数据 | 坐标对齐对象边界已冻结 | 构建图直接引用 `modules/coordinate-alignment` | 验证与门禁覆盖新路径 | 旧核心包不再拥有对齐事实 | `wave-mapping.md`、`module-cutover-checklist.md`、`system-acceptance-report.md` | `Completed` |
| `M6 process-path` | `modules/process-path/` | `packages/process-runtime-core/planning` | `Wave 3` | 工艺路径模块入口、README 与 contracts 已就位，且 `CMakeLists.txt` 已声明 `M6` owner 元数据 | 工艺路径对象和接口已冻结 | 构建图直接引用 `modules/process-path` | 门禁覆盖路径生成与验证 | 旧核心包不再拥有工艺路径事实 | `wave-mapping.md`、`module-cutover-checklist.md`、`system-acceptance-report.md` | `Completed` |
| `M7 motion-planning` | `modules/motion-planning/` | `packages/process-runtime-core/src/domain/motion` | `Wave 3` | 运动规划模块入口已就位，且 `CMakeLists.txt` 已声明 `M7` owner 元数据 | 运动对象和边界已冻结 | 构建图直接引用 `modules/motion-planning` | 验证入口覆盖规划结果 | 旧核心包不再拥有运动规划事实 | `wave-mapping.md`、`module-cutover-checklist.md`、`system-acceptance-report.md` | `Completed` |
| `M8 dispense-packaging` | `modules/dispense-packaging/` | `packages/process-runtime-core/src/domain/dispensing`、`packages/process-runtime-core/src/application/usecases/dispensing` | `Wave 3` | 执行包构建模块入口、README 与 contracts 已就位，且 `CMakeLists.txt` 已声明 `M8` owner 元数据 | 执行包对象和边界已冻结 | 构建图直接引用 `modules/dispense-packaging` | 门禁覆盖 `ExecutionPackageBuilt` 与 `ExecutionPackageValidated` 语义 | 旧核心包不再拥有执行包事实 | `wave-mapping.md`、`module-cutover-checklist.md`、`system-acceptance-report.md` | `Completed` |
| `M9 runtime-execution` | `modules/runtime-execution/` | `packages/runtime-host/`、`packages/device-adapters/`、`packages/transport-gateway/` | `Wave 4` | 运行时 owner、contracts、adapters 已归位 | 运行态控制语义和设备边界已冻结 | `apps/runtime-service`、`apps/runtime-gateway` 指向新 owner | 运行时链测试与 gate 覆盖 service/gateway/adapter | `runtime-host`、`transport-gateway` 仅剩兼容壳 | `wave-mapping.md`、`validation-gates.md`、`system-acceptance-report.md` | `Completed` |
| `M10 trace-diagnostics` | `modules/trace-diagnostics/` | `packages/traceability-observability/` | `Wave 5` | 追溯模块入口与 contracts 已就位 | 追溯对象、日志边界已冻结 | 构建图直接引用 `modules/trace-diagnostics` | `US6-CP1/US6-CP3/US6-CP4` 覆盖 trace owner、仓库级 tests 与样本索引回链 | legacy observability 包只剩 wrapper 或 README | `wave-mapping.md`、`validation-gates.md`、`system-acceptance-report.md` | `Completed` |
| `M11 hmi-application` | `modules/hmi-application/` | `apps/hmi-app/`、`apps/control-cli/` | `Wave 5` | HMI 业务 owner 与 contracts 已归位 | 展示对象、审批/查询边界已冻结 | `apps/hmi-app` 仅保留宿主和装配 | `US6-CP2/US6-CP3/US6-CP4` 覆盖 HMI owner、tests 承载面与 samples 基线 | `hmi-app/control-cli` 不再承载终态业务 owner | `wave-mapping.md`、`validation-gates.md`、`system-acceptance-report.md` | `Completed` |

## Wave Cutover Checkpoints

| Wave | Entry gate | Exit gate | Required validation | Evidence paths | Status |
|---|---|---|---|---|---|
| `Wave 1` | `Wave 0` 已完成；`shared/`、`scripts/`、`tests/`、`samples/`、`deploy/` 的终态角色已冻结；legacy roots 允许存在但不得新增业务实现 | canonical root 文档已更新；`cmake/workspace-layout.env` 与 `cmake/LoadWorkspaceLayout.cmake` 可解析 canonical roots；`scripts/` 成为正式脚本根；legacy script roots 已降级为 wrapper；layout/freezerset validator 已具备 wave-aware 断言 | `.\build.ps1`；`python .\\tools\\migration\\validate_workspace_layout.py`；`python .\\tools\\migration\\validate_dsp_e2e_spec_docset.py`；根级脚本入口静态审查 | `docs/architecture/canonical-paths.md`、`docs/architecture/directory-responsibilities.md`、`cmake/workspace-layout.env`、`cmake/LoadWorkspaceLayout.cmake`、`scripts/`、`tools/` wrappers、`docs/architecture/system-acceptance-report.md` | `Completed` |
| `Wave 2` | `Wave 1` 已完成；`shared/`、`scripts/`、`tests/`、`samples/` 已可承接迁移；上游静态输入事实已完成盘点 | `M1-M3` owner 路径与 contracts 已落位；对应样本入口已迁入 `samples/`；`packages/engineering-data` 与 `apps/control-cli` 不再承担 `M1-M3` 终态 owner | `python .\\tools\\migration\\validate_workspace_layout.py --wave \"Wave 2\"`；`.\\test.ps1`；`tests/integration/README.md` 的 `US3-CP1` 到 `US3-CP4` 回链检查 | `modules/job-ingest/`、`modules/dxf-geometry/`、`modules/topology-feature/`、`samples/dxf/`、`samples/golden/`、`tests/integration/README.md`、`wave-mapping.md`、`docs/architecture/system-acceptance-report.md` | `Completed` |
| `Wave 3` | `Wave 2` 已完成；`M1-M3` 不再依赖 legacy owner；规划链输入事实已稳定 | `M0/M4-M8` 已拆出 `process-runtime-core`；新模块依赖方向符合 `apps -> modules -> shared` 或 module-contract-only；冻结文档中的对象边界与目录落位一致；`process-runtime-core` 不再承担规划链终态 owner | `python .\\tools\\migration\\validate_workspace_layout.py --wave \"Wave 3\"`；`python .\\tools\\migration\\validate_dsp_e2e_spec_docset.py --wave \"Wave 3\"`；规划链构建入口静态审查 | `modules/workflow/`、`modules/process-planning/`、`modules/coordinate-alignment/`、`modules/process-path/`、`modules/motion-planning/`、`modules/dispense-packaging/`、`dsp-e2e-spec-s04/s05`、`wave-mapping.md`、`validation-gates.md`、`docs/architecture/system-acceptance-report.md` | `Completed` |
| `Wave 4` | `Wave 3` 已完成；`M0/M4-M8` owner 与依赖方向稳定；运行时切换范围与设备语义回归范围已隔离 | `M9` owner、运行时入口壳与设备适配已归位；`apps/runtime-service`、`apps/runtime-gateway` 成为正式入口；运行态只消费上游规划结果且不回写规划事实 | `python .\\tools\\migration\\validate_workspace_layout.py --wave \"Wave 4\"`；`.\\build.ps1`；`.\\test.ps1`；`scripts\\validation\\invoke-workspace-tests.ps1` 运行运行时链专项回归 | `modules/runtime-execution/`、`apps/runtime-service/`、`apps/runtime-gateway/`、`dsp-e2e-spec-s07`、`dsp-e2e-spec-s08`、`validation-gates.md`、`docs/architecture/system-acceptance-report.md` | `Completed` |
| `Wave 5` | `Wave 4` 已完成；运行时链、宿主壳和设备语义已稳定；trace/HMI/validation 的消费边界已盘点 | `M10-M11` owner 与 contracts 已落位；`tests/` 成为唯一仓库级验证承载面；`samples/` 成为唯一稳定样本承载面；`integration/`、`examples/` 仅保留 redirect、wrapper 或 tombstone | `python .\\tools\\migration\\validate_workspace_layout.py --wave \"Wave 5\"`；`.\\test.ps1`；`python -m test_kit.workspace_validation`；`docs/validation/README.md` 引用检查 | `modules/trace-diagnostics/`、`modules/hmi-application/`、`tests/`、`samples/`、`dsp-e2e-spec-s09`、`dsp-e2e-spec-s10`、`validation-gates.md`、`docs/architecture/system-acceptance-report.md` | `Completed` |
| `Wave 6` | `Wave 5` 已完成；所有 owner 已真实迁入 canonical roots；legacy roots 只剩兼容壳或待移除骨架 | 根级构建图切换到 canonical graph；`build.ps1`、`test.ps1`、`ci.ps1` 与 local validation gate 全量通过；legacy exit 报告确认旧根退出默认 owner | `.\\build.ps1`；`.\\test.ps1`；`.\\ci.ps1`；`.\\tools\\scripts\\run-local-validation-gate.ps1`；`python .\\scripts\\migration\\legacy-exit-checks.py`（或兼容 wrapper）；layout/freezerset validator 全量运行 | `CMakeLists.txt`、`apps/CMakeLists.txt`、`tests/CMakeLists.txt`、`cmake/workspace-layout.env`、`docs/architecture/removed-legacy-items-final.md`、`docs/architecture/system-acceptance-report.md`、`validation-gates.md` | `Completed` |

## Wave 4 运行时链检查点（T050）

| Checkpoint | 判定标准 | Evidence anchors |
|---|---|---|
| `US5-CP1 Runtime owner` | `modules/runtime-execution/` 的 `CMakeLists.txt`、`README.md`、`contracts/README.md` 和 `adapters/` 已形成可审计 owner 面。 | `modules/runtime-execution/` |
| `US5-CP2 Runtime hosts` | `apps/runtime-service/`、`apps/runtime-gateway/` 成为正式入口；`apps/control-runtime/`、`apps/control-tcp-server/` 仅保留兼容壳职责。 | `apps/runtime-service/`、`apps/runtime-gateway/`、`apps/control-runtime/`、`apps/control-tcp-server/` |
| `US5-CP3 Device boundary` | 设备契约与适配已经归位到 `shared/contracts/device/` 与 `modules/runtime-execution/adapters/`，运行时 owner 与设备边界一致。 | `shared/contracts/device/`、`modules/runtime-execution/adapters/` |
| `US5-CP4 No upstream writeback` | 运行时链专项验证与 `dsp-e2e-spec-s07/s08` 证据共同证明运行态只消费上游规划结果，不回写规划事实。 | `scripts/validation/invoke-workspace-tests.ps1`、`dsp-e2e-spec-s07`、`dsp-e2e-spec-s08` |

## Wave 4 Main Thread Review Evidence (T056)

- layout gate：`integration/reports/workspace-layout/us5-review-wave4-layout.txt`
- freeze docset gate：`integration/reports/dsp-e2e-spec-docset/us5-review-wave4.md`
- runtime gateway compatibility：`integration/reports/workspace-runtime/us5-review-wave4-transport-gateway-compat.txt`
- runtime host/gateway build replay：`integration/reports/workspace-runtime/us5-review-wave4-build.txt`
- phase end check layout gate：`integration/reports/workspace-layout/us5-review-wave4-phase-end-check.txt`
- phase end check freeze docset gate：`integration/reports/dsp-e2e-spec-docset/us5-review-wave4-phase-end-check.md`
- phase end check runtime gateway compatibility：`integration/reports/workspace-runtime/us5-review-wave4-phase-end-check-transport-gateway-compat.txt`
- phase end check runtime host/gateway build replay：`integration/reports/workspace-runtime/us5-review-wave4-phase-end-check-build.txt`
- closeout note：US5 closeout 证据已回写，且 Phase 7 End Check 已通过。

## Wave 5 追溯/HMI 与 Tests/Samples 检查点（T058）

| Checkpoint | 判定标准 | Evidence anchors |
|---|---|---|
| `US6-CP1 Trace owner` | `modules/trace-diagnostics/` 的 `CMakeLists.txt`、`README.md`、`contracts/README.md` 可审计，且 trace 事实 owner 不再由 legacy observability 路径承载。 | `modules/trace-diagnostics/`、`packages/traceability-observability/README.md` |
| `US6-CP2 HMI owner` | `modules/hmi-application/` 成为业务 owner，`apps/hmi-app/` 与 `apps/control-cli/` 仅保留宿主/入口职责，不承载终态业务实现。 | `modules/hmi-application/`、`apps/hmi-app/README.md`、`apps/control-cli/` |
| `US6-CP3 Tests surface` | 仓库级验证入口收敛到 `tests/`，且 trace/HMI 回归通过根级测试入口和 validation 套件可回链。 | `tests/CMakeLists.txt`、`tests/integration/README.md`、`tests/e2e/README.md`、`docs/validation/README.md` |
| `US6-CP4 Samples surface` | 稳定样本与验收基线索引收敛到 `samples/` 与 `dsp-e2e-spec-s09/s10`，不再依赖 `examples/` 承担终态样本职责。 | `samples/README.md`、`dsp-e2e-spec-s09`、`dsp-e2e-spec-s10`、`examples/README.md` |

## Wave 5 Main Thread Review Evidence (T064)

- layout gate：`integration/reports/workspace-layout/us6-review-wave5-layout.txt`
- freeze docset gate：`integration/reports/dsp-e2e-spec-docset/us6-review-wave5.md`
- workspace validation packages suite：`integration/reports/workspace-validation-us6/workspace-validation.md`
- phase end check layout gate：`integration/reports/workspace-layout/us6-review-wave5-phase-end-check.txt`
- phase end check freeze docset gate：`integration/reports/dsp-e2e-spec-docset/us6-review-wave5-phase-end-check.md`
- phase end check workspace validation packages suite：`integration/reports/workspace-validation-us6-phase-end-check/workspace-validation.md`
- trace viewer host alignment：`apps/README.md`、`apps/trace-viewer/README.md`
- closeout note：US6 closeout 证据已回写，且 Phase 8 End Check 已通过。

## Wave 6 根级构建图切换与 Legacy Exit 检查点（T066）

| Checkpoint | 判定标准 | Evidence anchors |
|---|---|---|
| `US7-CP1 Root graph canonical` | 根级 `CMakeLists.txt`、`apps/CMakeLists.txt`、`tests/CMakeLists.txt` 与 `cmake/workspace-layout.env` 默认解析指向 canonical roots，不再以 `packages/*` 作为默认真实实现来源。 | `CMakeLists.txt`、`apps/CMakeLists.txt`、`tests/CMakeLists.txt`、`cmake/workspace-layout.env` |
| `US7-CP2 Root entry stability` | 根级 `build.ps1`、`test.ps1`、`ci.ps1` 对外入口保持不变，内部执行链切向 canonical graph 且与 local validation gate 一致。 | `build.ps1`、`test.ps1`、`ci.ps1`、`tools/scripts/run-local-validation-gate.ps1` |
| `US7-CP3 Legacy root exit` | legacy exit 检查证明 `packages/`、`integration/`、`tools/`、`examples/` 不再承载终态 owner，仅保留 wrapper、tombstone 或说明文档。 | `scripts/migration/legacy-exit-checks.py`、`tools/scripts/legacy_exit_checks.py`、`packages/README.md`、`integration/README.md`、`tools/README.md`、`examples/README.md` |
| `US7-CP4 Closeout evidence` | `Wave 6` closeout 证据可从移除清单与总体验收报告回链，并与波次映射保持一致。 | `docs/architecture/removed-legacy-items-final.md`、`docs/architecture/system-acceptance-report.md`、`wave-mapping.md` |

## Shared Gate Notes

| Rule | 说明 |
|---|---|
| `No dual owner` | 任一模块一旦声明 `Owner path ready`，legacy 路径不得再接收新增业务实现。 |
| `Contracts first` | 需要跨模块暴露的接口必须先落入 `contracts/` 或 `shared/contracts/`，再允许依赖切换。 |
| `Tests move with owner` | 模块 owner 切换完成前，对应样本和验证入口必须同步切换到 `tests/` 或 `samples/` 正式根。 |
| `Closeout is evidence-based` | 没有证据路径的 cutover 视为未完成，不允许口头宣布收口。 |
