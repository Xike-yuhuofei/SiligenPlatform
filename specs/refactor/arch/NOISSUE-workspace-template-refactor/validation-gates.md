# Validation Gates

## Purpose

本文件定义 `Wave 0-6` 的入口门禁、退出门禁、验证命令和证据路径。任何波次只有在上一波退出门禁满足后才允许进入；任何未满足退出门禁的波次都不得继续推进后续 owner cutover。

## Global Gate Rules

| Rule | 说明 |
|---|---|
| `Entry before work` | 每波开始前必须先确认前一波的退出门禁已满足，并且根级入口 `build.ps1`、`test.ps1`、`ci.ps1` 仍可调用。 |
| `Exit before next wave` | 任何一波未完成 closeout 证据补录时，不允许进入下一波。 |
| `Canonical first` | 新根必须先成为真实 owner 或正式承载面，legacy 路径随后才允许降级。 |
| `No implicit fallback` | 波次验收不得依赖未声明的旧路径 fallback；所有 bridge 必须显式记录。 |
| `Evidence in repo` | 门禁通过判定只接受仓库内文档、脚本、报告和验证输出作为证据。 |

## Wave Gate Matrix

| Wave | Entry gate | Exit gate | Required validation commands / assertions | Evidence paths |
|---|---|---|---|---|
| `Wave 0` | 正式事实源已固定为 `specs/refactor/arch/NOISSUE-workspace-template-refactor/`；`spec.md`、`plan.md`、`tasks.md` 已存在；冻结文档目录可访问 | `wave-mapping.md`、`module-cutover-checklist.md`、`validation-gates.md` 已创建并可支撑后续波次；统一术语和故事到波次映射已可追溯 | 文档存在性检查；`docs/architecture/dsp-e2e-spec/` 目录完整；任务编号连续 | `spec.md`、`plan.md`、`tasks.md`、本文件、`wave-mapping.md`、`module-cutover-checklist.md` |
| `Wave 1` | `Wave 0` 完成；shared/scripts/tests/samples/deploy 的终态角色已在计划中冻结；旧根仍允许存在但不得新增业务实现 | canonical root 文档更新完成；`cmake/workspace-layout.env` 和 `LoadWorkspaceLayout.cmake` 能解析 canonical roots；`scripts/` 成为正式脚本根；legacy script roots 降级为 wrapper；layout/freezerset validator 已具备 wave-aware 断言 | `.\build.ps1`；`python .\\tools\\migration\\validate_workspace_layout.py --wave "Wave 1"`；`python .\\tools\\migration\\validate_dsp_e2e_spec_docset.py --wave "Wave 1"`；根级脚本入口静态审查 | `docs/architecture/canonical-paths.md`、`docs/architecture/directory-responsibilities.md`、`cmake/workspace-layout.env`、`cmake/LoadWorkspaceLayout.cmake`、`scripts/`、`tools/` wrapper、`system-acceptance-report.md` |
| `Wave 2` | `Wave 1` 完成；`shared/`、`scripts/`、`tests/`、`samples/` 已可承接迁移；上游静态输入事实已完成盘点 | `M1-M3` 的 owner 路径和 contracts 已落位；相应样本已迁入 `samples/`；layout validator 可断言 `modules/job-ingest`、`modules/dxf-geometry`、`modules/topology-feature` | `python .\\tools\\migration\\validate_workspace_layout.py --wave "Wave 2"`，且 `m1_job_ingest_owner_path_complete`、`m2_dxf_geometry_owner_path_complete`、`m3_topology_feature_owner_path_complete` 全为 `true`；`.\test.ps1` 中上游链相关用例；`tests/integration/README.md` 的回链校验 | `modules/job-ingest/`、`modules/dxf-geometry/`、`modules/topology-feature/`、`samples/dxf/`、`samples/golden/`、`system-acceptance-report.md` |
| `Wave 3` | `Wave 2` 完成；`M1-M3` 不再依赖 legacy owner；规划链的输入事实已稳定 | `M0/M4-M8` 已拆出 `process-runtime-core`；新模块依赖方向符合 `apps -> modules -> shared` 或 module-contract-only；冻结文档中的对象边界与目录落位一致 | `python .\\tools\\migration\\validate_workspace_layout.py --wave "Wave 3"`，且 `m0_workflow_owner_path_complete`、`m4_process_planning_owner_path_complete`、`m5_coordinate_alignment_owner_path_complete`、`m6_process_path_owner_path_complete`、`m7_motion_planning_owner_path_complete`、`m8_dispense_packaging_owner_path_complete` 全为 `true`；`python .\\tools\\migration\\validate_dsp_e2e_spec_docset.py`；规划链构建入口静态审查 | `modules/workflow/`、`modules/process-planning/`、`modules/coordinate-alignment/`、`modules/process-path/`、`modules/motion-planning/`、`modules/dispense-packaging/`、`dsp-e2e-spec-s04/s05` |
| `Wave 4` | `Wave 3` 完成；规划链 owner 稳定；运行时切换范围与设备语义回归范围已隔离 | `modules/runtime-execution/` 成为真实 owner；`apps/runtime-service` 与 `apps/runtime-gateway` 成为正式宿主；设备契约与适配完成归位；运行态不回写上游规划事实 | `.\build.ps1`；`.\test.ps1`；`python .\\tools\\migration\\validate_workspace_layout.py --wave "Wave 4"` 且运行时链 owner/入口断言通过；运行时链专项回归；`scripts\\validation\\invoke-workspace-tests.ps1` 切向正式入口并确认不回写上游规划事实 | `modules/runtime-execution/`、`apps/runtime-service/`、`apps/runtime-gateway/`、`dsp-e2e-spec-s07`、`dsp-e2e-spec-s08`、`system-acceptance-report.md` |
| `Wave 5` | `Wave 4` 完成；运行时链、宿主壳和设备语义已稳定；trace/HMI/validation 的消费边界已盘点 | `M10-M11` 已落位；`tests/` 成为唯一仓库级验证承载面；`samples/` 成为唯一稳定样本承载面；`integration/`、`examples/` 只剩 redirect、wrapper 或 tombstone | `.\test.ps1`；`python .\\tools\\migration\\validate_workspace_layout.py`；`python -m test_kit.workspace_validation`；`docs/validation/README.md` 引用检查 | `modules/trace-diagnostics/`、`modules/hmi-application/`、`tests/`、`samples/`、`dsp-e2e-spec-s09`、`dsp-e2e-spec-s10` |
| `Wave 6` | `Wave 5` 完成；所有 owner 已真实迁入 canonical roots；legacy roots 只剩兼容壳或待移除骨架 | 根级构建图切换完成；`build.ps1`、`test.ps1`、`ci.ps1` 和 local validation gate 全部基于 canonical graph 通过；legacy exit 报告证明 `packages/`、`integration/`、`tools/`、`examples/` 不再承担终态 owner | `.\build.ps1`；`.\test.ps1`；`.\ci.ps1`；`.\tools\\scripts\\run-local-validation-gate.ps1`；`python .\\scripts\\migration\\legacy-exit-checks.py` 或兼容 wrapper；layout/freezerset validator 全量运行 | `CMakeLists.txt`、`apps/CMakeLists.txt`、`tests/CMakeLists.txt`、`cmake/workspace-layout.env`、`docs/architecture/removed-legacy-items-final.md`、`docs/architecture/system-acceptance-report.md` |

## Wave 1 Canonical Roots 与 Shared-Support 断言（T021）

- 命令：`python .\tools\migration\validate_workspace_layout.py --wave "Wave 1"`。
- 通过条件：`missing_key_count=0` 且 `missing_path_count=0`。
- canonical roots 骨架断言必须全部为 `true`：
  - `siligen_apps_root_skeleton_complete`
  - `siligen_modules_root_skeleton_complete`
  - `siligen_shared_root_skeleton_complete`
  - `siligen_docs_root_skeleton_complete`
  - `siligen_samples_root_skeleton_complete`
  - `siligen_tests_root_skeleton_complete`
  - `siligen_scripts_root_skeleton_complete`
  - `siligen_config_root_skeleton_complete`
  - `siligen_data_root_skeleton_complete`
  - `siligen_deploy_root_skeleton_complete`
- shared-support 断言必须全部为 `true`：
  - `shared_support_surface_directories_exist`
  - `directory_responsibilities_declares_shared_support_surface`
  - `directory_responsibilities_declares_target_roots`
  - `directory_responsibilities_declares_migration_source_roots`
- Wave 1 聚合断言必须为 `true`：
  - `wave1_canonical_roots_ready`
  - `wave1_shared_support_ready`

## Wave 2 上游静态工程链 Owner 路径断言（T031）

- 命令：`python .\tools\migration\validate_workspace_layout.py --wave "Wave 2"`。
- 通过条件：`missing_key_count=0` 且 `missing_path_count=0`。
- `M1-M3` owner 路径断言必须全部为 `true`：
  - `m1_job_ingest_owner_path_complete`
  - `m2_dxf_geometry_owner_path_complete`
  - `m3_topology_feature_owner_path_complete`

## Wave 3 工作流与规划链 Owner 路径断言（T039）

- 命令：`python .\tools\migration\validate_workspace_layout.py --wave "Wave 3"`。
- 通过条件：`missing_key_count=0` 且 `missing_path_count=0`。
- `M0/M4-M8` owner 路径断言必须全部为 `true`：
  - `m0_workflow_owner_path_complete`
  - `m4_process_planning_owner_path_complete`
  - `m5_coordinate_alignment_owner_path_complete`
  - `m6_process_path_owner_path_complete`
  - `m7_motion_planning_owner_path_complete`
  - `m8_dispense_packaging_owner_path_complete`

## Wave 4 运行时执行链检查点（T050）

- 命令：`python .\tools\migration\validate_workspace_layout.py --wave "Wave 4"`；`scripts\validation\invoke-workspace-tests.ps1`。
- 通过条件：`missing_key_count=0` 且 `missing_path_count=0`，并且运行时链 owner/入口/设备边界断言与专项回归同时通过。
- 必须同时满足以下检查点：
  - `US5-CP1 Runtime owner`：`modules/runtime-execution/` 的 owner surface 可审计（`CMakeLists.txt`、`README.md`、`contracts/README.md`、`adapters/`）。
  - `US5-CP2 Runtime hosts`：`apps/runtime-service/`、`apps/runtime-gateway/` 成为正式入口，`apps/control-runtime/` 与 `apps/control-tcp-server/` 仅保留兼容壳职责。
  - `US5-CP3 Device boundary`：设备契约与适配归位到 `shared/contracts/device/` 与 `modules/runtime-execution/adapters/`，不再由 legacy 路径承担终态 owner。
  - `US5-CP4 No upstream writeback`：运行态只消费上游规划结果，运行时专项回归与 `dsp-e2e-spec-s07/s08` 证据可回链证明不回写规划事实。

## Wave 5 追溯/HMI 与 Tests/Samples 检查点（T058）

- 命令：`python .\tools\migration\validate_workspace_layout.py --wave "Wave 5"`；`.\test.ps1`；`python -m test_kit.workspace_validation`。
- 通过条件：`missing_key_count=0` 且 `missing_path_count=0`，并且 `M10-M11` owner、仓库级 tests 承载面和 samples 稳定索引三类检查同时通过。
- 必须同时满足以下检查点：
  - `US6-CP1 Trace owner`：`modules/trace-diagnostics/` 的 owner surface 可审计（`CMakeLists.txt`、`README.md`、`contracts/README.md`），且 `packages/traceability-observability/` 不再承载终态 owner。
  - `US6-CP2 HMI owner`：`modules/hmi-application/` 成为业务 owner，`apps/hmi-app/` 与 `apps/control-cli/` 仅保留宿主或入口职责。
  - `US6-CP3 Tests surface`：`tests/` 成为唯一仓库级验证承载面，trace/HMI 回归可通过 `tests/` 与 `docs/validation/README.md` 回链。
  - `US6-CP4 Samples surface`：`samples/` 成为唯一稳定样本承载面，`dsp-e2e-spec-s09/s10` 与样本索引可回链证明 `examples/` 不再承担终态样本职责。

## Wave 6 根级构建图切换与 Legacy Exit 检查点（T066）

- 命令：`.\build.ps1`；`.\test.ps1`；`.\ci.ps1`；`.\tools\scripts\run-local-validation-gate.ps1`；`python .\scripts\migration\legacy-exit-checks.py`（或兼容 wrapper）。
- 通过条件：根级 `build/test/ci` 与 local validation gate 均在 canonical graph 上通过，且 legacy exit 报告确认 `packages/`、`integration/`、`tools/`、`examples/` 不再承担终态 owner。
- 必须同时满足以下检查点：
  - `US7-CP1 Root graph canonical`：根级 `CMakeLists.txt`、`apps/CMakeLists.txt`、`tests/CMakeLists.txt` 与 `cmake/workspace-layout.env` 默认解析指向 canonical roots，不再以 `packages/*` 为默认真实实现来源。
  - `US7-CP2 Root entry stability`：`build.ps1`、`test.ps1`、`ci.ps1` 外部调用接口保持稳定，内部实现切向 canonical graph 且与 local gate 一致。
  - `US7-CP3 Legacy exit evidence`：legacy roots 的退出检查可审计，`packages/`、`integration/`、`tools/`、`examples/` 仅剩 wrapper、tombstone 或说明性文档。
  - `US7-CP4 Closeout traceability`：`docs/architecture/removed-legacy-items-final.md` 与 `docs/architecture/system-acceptance-report.md` 提供可回链证据，且与 `wave-mapping.md` 的 `Wave 6` 映射一致。

## Evidence Ownership

| Evidence item | Owning file |
|---|---|
| 波次进入/退出状态 | `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\validation-gates.md` |
| 模块级 cutover 判定 | `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\module-cutover-checklist.md` |
| legacy root 与 canonical root 迁移盘点 | `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\wave-mapping.md` |
| 最终 build/test/ci/gate 证据 | `D:\Projects\SS-dispense-align\docs\architecture\system-acceptance-report.md` |

## Blocking Conditions

| Condition | Required action |
|---|---|
| 发现新逻辑继续写入 `packages/`、`integration/`、`tools/`、`examples/` | 停止当前波次，先补 bridge 和 owner 迁移策略，再继续。 |
| 根级 `build.ps1`、`test.ps1`、`ci.ps1` 任一入口失稳 | 当前波次不得 closeout，必须先恢复稳定入口。 |
| 冻结文档术语与仓库结构出现双事实源 | 回退到当前波次内修正，不允许带病进入下一波。 |
| 无法给出仓库内证据路径 | 视为门禁未通过，不得口头宣告完成。 |
