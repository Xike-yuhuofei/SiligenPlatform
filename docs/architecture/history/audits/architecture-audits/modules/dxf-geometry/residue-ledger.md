# dxf-geometry Residue Ledger

## 1. Executive Verdict
- trajectory / preview / parser / public-surface / simulation_input / placeholder skeleton closeout 已收口；当前无 active residue。

## 2. Scope
- 本轮只审查 `modules/dxf-geometry/**`。
- 为判断边界只引用少量关联对象：`shared/contracts/engineering/**`、`scripts/engineering-data/**`、`scripts/validation/verify-engineering-data-cli.ps1`、`apps/runtime-service/**`、`modules/job-ingest/**`、`modules/workflow/**`、`modules/process-planning/**`、仓库级 engineering-data 合同/集成/性能测试。

## 3. Baseline Freeze
- owner 基线
  - `README.md:3-5` 将当前 owner 冻结为 `M2 dxf-geometry`，正式事实来源冻结在 `modules/dxf-geometry/{application,adapters,contracts}/` 与 `shared/contracts/engineering/`。
  - `README.md:15` 明确 `modules/dxf-geometry/application/engineering_data/` 是当前 canonical Python owner 面。
  - `module.yaml:1-10` 将 owner artifact 冻结为 `CanonicalGeometry`，并声明“DXF 解析适配、几何转换与 engineering_data Python 包”都已归位到 canonical surface。
- 职责边界基线
  - `contracts/README.md:5-10` 把模块契约职责冻结为“DXF 几何解析相关的模块内契约与输入引用规则”。
  - `contracts/README.md:13-14` 明确模块不应长期承载“跨模块稳定公共契约”以及“运行时执行、设备适配或 HMI 展示层语义”。
- public surface 基线
  - `CMakeLists.txt:18-67` 冻结 C++ public surface 为 `siligen_dxf_geometry_application_headers`、`siligen_dxf_geometry_application_public`、`siligen_module_dxf_geometry`。
  - `CMakeLists.txt:77-78` 曾说明模块根 target 会向外暴露 `siligen_parsing_adapter`；当前 live truth 已切到 `siligen_dxf_geometry_pb_path_source_adapter`。
  - `scripts/engineering-data/dxf_to_pb.py:10-18` 与 `tests/contracts/test_engineering_contracts.py:10-18` 证明 Python public seam 通过 `engineering_data` 包与 `scripts/engineering-data/*` wrapper 被 live 消费。
- build 基线
  - `CMakeLists.txt:3,25-26,48,77-78` 证明模块 live build 依赖 `process-planning/contracts` 与 owner-scoped parser target `siligen_dxf_geometry_pb_path_source_adapter`。
  - `adapters/CMakeLists.txt:30,87-103` 证明 adapters target 依赖 `siligen_process_path_contracts_public`，并在同一 target 中承担 protobuf 代码生成与 DXF/PB adapter 编译。
  - `module.yaml:6-10` 只声明 `process-planning/contracts`、`process-path/contracts` 与 `shared` 为 allowed dependencies；这不是审查结论，而是当前已冻结的声明基线。
- skeleton 基线
  - `scripts/migration/validate_workspace_layout.py` 要求 `modules/dxf-geometry/{contracts,domain,services,application,adapters,tests,examples}` 与 `tests/{unit,contract,integration,regression}` 物理存在。
  - `CMakeLists.txt` 当前只把 `application/`、`adapters/`、`tests/` 计入 `SILIGEN_TARGET_IMPLEMENTATION_ROOTS`；`domain/`、`services/`、`examples/` 与 `tests/regression/` 不属于 live build roots。
- test 基线
  - `tests/README.md:7-19` 把模块 owner 级证明冻结在模块内 `unit + contracts + golden + integration`，且当前最小正式矩阵聚焦 `DxfPbPreparationService`。
  - `tests/CMakeLists.txt:4-62` 证明模块内 C++ 测试目标真实存在。
  - `scripts/validation/verify-engineering-data-cli.ps1:73-76`、`tests/contracts/test_engineering_contracts.py:18-22`、`tests/contracts/test_engineering_data_compatibility.py:19-23` 证明 Python owner 面当前依赖仓库级验证而不是模块内测试目标。

## 4. Top-Level Findings
- C++ live owner 面已经收口到 `DxfPbPreparationService` 与 `PbPathSourceAdapter`；legacy parser target、selector seam、legacy/infrastructure 兼容头已退出当前 live surface。
- `application/engineering_data/` 仍然是 live Python owner 面，但当前只剩 `dxf_to_pb` 预处理与 `simulation_input` stable contract；`residual/` 只隔离 simulation runtime concrete。
- `application/engineering_data/contracts/simulation_input.py` 现已退化为 stable compat shell：runtime defaults、trigger 投影、payload 组装与 CLI concrete 已迁到 `modules/runtime-execution/application/runtime_execution/`，`dxf-geometry` 只保留 geometry helper。
- `application/engineering_data/trajectory/offline_path_to_trajectory.py` 已退化为 compat wrapper；trajectory owner 已迁到 `modules/motion-planning/application/motion_planning/`。
- preview owner 已迁到 `apps/planner-cli/tools/planner_cli_preview/`；`dxf-geometry` 不再继续承载本地 HTML preview renderer。
- `module.yaml`、`CMakeLists.txt` 与 parser target 命名现已承认 `siligen_dxf_geometry_pb_path_source_adapter` 是唯一 live parser seam；当前 build truth 不再保留 `siligen_parsing_adapter`。
- 模块内 Python gate 已建立；preview 与 trajectory 的 owner gate 分别迁到 `apps/planner-cli/tests/python/` 与 `modules/motion-planning/tests/python/`，仓库级脚本继续作为跨 owner consumer 证据。
- `domain/`、`services/`、`examples/` 与 `tests/regression/` 已明确重分类为 workspace layout 要求的 canonical skeleton，而不是待删除的 live residue；`CMakeLists.txt` 当前不会把它们重新拉入 implementation roots。

## 5. Residue Ledger Summary
- active owner residue：0 项
- active naming residue：0 项
- active placeholder residue：0 项
- completed governance records：13 项（`DG-R001` ~ `DG-R013`）

## 6. Residue Ledger Table
| residue_id | severity | file_or_target | current_role | why_residue | violated_baseline | evidence | action_class | preferred_destination | confidence |
|---|---|---|---|---|---|---|---|---|---|
| DG-R001 | P1 | historical trajectory Python owner in `application/engineering_data/trajectory/offline_path_to_trajectory.py` | 已迁出的 trajectory compat shell；当前仅保留 module / workspace 兼容入口 | 原 `dxf-geometry` trajectory 语义越过 geometry owner 边界；当前实现 owner 已迁到 `modules/motion-planning/application/motion_planning/` | `README.md:15` 将 Python owner 面冻结在 geometry owner；`contracts/README.md:13-14` 禁止长期承载运行时/非 geometry 语义 | `scripts/engineering-data/path_to_trajectory.py`；`modules/motion-planning/application/motion_planning/trajectory_generation.py`；`modules/motion-planning/tests/python/test_trajectory_owner_lane.py`；`modules/dxf-geometry/application/engineering_data/trajectory/offline_path_to_trajectory.py` | migrated | `modules/motion-planning/application/motion_planning/` | high |
| DG-R002 | P1 | `application/engineering_data/contracts/simulation_input.py` | 已完成的 stable compat shell；runtime concrete owner 已迁出 | 过程控制与 runtime concrete 语义曾进入 geometry contract 包；现已拆为 shared schema authority + runtime concrete owner + geometry helper compat shell | `contracts/README.md:13-14`；`README.md:5` 把稳定契约 authority 冻结到 `shared/contracts/engineering/` | `modules/dxf-geometry/application/engineering_data/contracts/simulation_input.py`；`modules/runtime-execution/application/runtime_execution/simulation_input.py`；`modules/runtime-execution/application/runtime_execution/export_simulation_input.py`；`modules/dxf-geometry/application/engineering_data/processing/simulation_geometry.py` | migrated | `modules/runtime-execution/application/runtime_execution/` | high |
| DG-R003 | P1 | historical preview Python owner in `application/engineering_data/preview/html_preview.py` | 已迁出的 preview workspace shell；本地 preview owner 已切到 `apps/planner-cli` | geometry owner 不应长期承载 app-facing/HMI-facing HTML renderer concrete；当前 workspace 入口保留但实现已迁出 | `contracts/README.md:14` 禁止长期承载 HMI 展示层语义 | `scripts/engineering-data/generate_preview.py`；`apps/planner-cli/tools/planner_cli_preview/preview_renderer.py`；`apps/planner-cli/tests/python/test_preview_owner_lane.py`；`tests/contracts/test_engineering_data_compatibility.py` | migrated | `apps/planner-cli/tools/planner_cli_preview/` | high |
| DG-R004 | P1 | `application/services/dxf/DxfPbPreparationService.cpp` | 已拆薄的 `.dxf -> .pb` orchestration seam；launcher / env / process 细节已下沉 | 原单体 service 同时扮演 façade / launcher / compat gateway；当前已保留最小 orchestration 并把细节拆到 helper | `tests/README.md:7-14` 冻结的 owner 证明只聚焦 `EnsurePbReady`；当前 public seam 需保持最小稳定行为 | `modules/dxf-geometry/application/services/dxf/DxfPbPreparationService.cpp`；`modules/dxf-geometry/application/services/dxf/PbCommandResolver.cpp`；`modules/dxf-geometry/application/services/dxf/PbProcessRunner.cpp`；`modules/dxf-geometry/tests/unit/services/dxf/DxfPbPreparationServiceTest.cpp` | split | `application` 保留 orchestration；launch / command / process helper 已内聚到同模块内部实现 | high |
| DG-R005 | P1 | `module.yaml` + `CMakeLists.txt` + `adapters/CMakeLists.txt` + `tests/CMakeLists.txt` | 已对齐的依赖声明与构建元数据 | `allowed_dependencies` 与当前 live link/include graph 已收口；当前不再存在旧 parser target 或额外跨模块依赖漂移 | `module.yaml:6-10` | `modules/dxf-geometry/module.yaml`；`modules/dxf-geometry/CMakeLists.txt`；`modules/dxf-geometry/adapters/CMakeLists.txt`；`modules/dxf-geometry/tests/CMakeLists.txt` | aligned | `module.yaml` 与当前最小依赖集合一致 | high |
| DG-R006 | P1 | historical target `siligen_parsing_adapter` | 已删除的 generic parser target；live parser seam 已切到 `siligen_dxf_geometry_pb_path_source_adapter` | 原 target 命名与 owner 边界不一致；本批已删除旧 target 并完成 consumer 迁移，不再保留 compat target | `README.md:9` 冻结模块构建入口为 `siligen_module_dxf_geometry`；parser seam 现已由 owner-scoped target 收口 | `adapters/CMakeLists.txt`；`modules/dxf-geometry/CMakeLists.txt`；`apps/runtime-service/CMakeLists.txt`；`tests/contracts/test_dxf_geometry_parser_target_closeout.py` | deleted | `siligen_dxf_geometry_pb_path_source_adapter` | high |
| DG-R007 | P1 | historical `DXFAdapterFactory*` / `AutoPathSourceAdapter*` selector seam | 已删除的 selector / auto path source seam | 无 live runtime consumer，且 public surface 已收口到 `PbPathSourceAdapter`；相关 source / headers 已退出 `dxf-geometry` live tree | public surface 应与 live seam 对齐，不应继续公开无 live consumer 的 selector | `modules/dxf-geometry/adapters/CMakeLists.txt`；`modules/dxf-geometry/adapters/dxf/`；`modules/dxf-geometry/adapters/include/dxf_geometry/adapters/planning/dxf/` | deleted | retired | high |
| DG-R008 | P1 | historical legacy application header `application/include/application/services/dxf/DxfPbPreparationService.h` | 已删除的 legacy include wrapper；canonical header 仅保留 `dxf_geometry/...` | 旧命名与新命名不应长期并存；当前 live include 已统一到 module-prefixed 路径 | public surface 应只保留一条 canonical seam | `modules/dxf-geometry/README.md`；`modules/dxf-geometry/application/include/dxf_geometry/application/services/dxf/DxfPbPreparationService.h`；`modules/dxf-geometry/tests/unit/services/dxf/DxfPbPreparationServiceTest.cpp`；`apps/runtime-service/container/ApplicationContainer.Dispensing.cpp` | deleted | `dxf_geometry/application/services/dxf/DxfPbPreparationService.h` | high |
| DG-R009 | P2 | historical `adapters/include/infrastructure/adapters/planning/dxf/*` compat headers | 已删除的 infrastructure compat public headers | live include 已统一转向 `dxf_geometry/...`；旧命名 public surface 已退出当前 owner 产物 | public surface 应避免旧命名和新命名长期并存 | `modules/dxf-geometry/adapters/include/dxf_geometry/adapters/planning/dxf/PbPathSourceAdapter.h`；`apps/runtime-service/bootstrap/InfrastructureBindingsBuilder.cpp`；`tests/contracts/test_dxf_geometry_parser_target_closeout.py` | deleted | `dxf_geometry/adapters/planning/dxf/*` | high |
| DG-R010 | P1 | historical C++ mixed test gate layout | 已删除的 `unit_tests` 混编布局；当前已拆成 `unit`、`contract`、`golden`、`integration` 独立 target | 旧测试 gate 曾模糊 owner closeout；当前 C++ lane 已收口，但该 residue 需保留为已完成治理记录 | `tests/README.md:7-19` | `tests/CMakeLists.txt`；`tests/README.md` | deleted | `siligen_dxf_geometry_unit_tests`、`siligen_dxf_geometry_contract_tests`、`siligen_dxf_geometry_golden_tests`、`siligen_dxf_geometry_integration_tests` | high |
| DG-R011 | P1 | `modules/dxf-geometry/tests/**` 与跨 owner engineering-data gates 的组合 | 已建立的 owner 级测试证明 | `dxf-geometry` 已拥有模块内 Python lane；preview / trajectory owner gate 已分别迁往新 owner，不再由仓库级脚本单独补位 | `README.md:17-29`；`tests/README.md:22-35` | `modules/dxf-geometry/tests/python/test_engineering_data_residual_quarantine.py`；`modules/dxf-geometry/tests/python/test_engineering_data_cli_lane.py`；`apps/planner-cli/tests/python/test_preview_owner_lane.py`；`modules/motion-planning/tests/python/test_trajectory_owner_lane.py`；`scripts/validation/verify-engineering-data-cli.ps1` | migrated | `modules/dxf-geometry/tests/python/` + owner-specific downstream gates | high |
| DG-R012 | P2 | `contracts/README.md` + `application/engineering_data/contracts/simulation_input.py` + `shared/contracts/engineering/*` | 已冻结的 `simulation_input` authority 口径 | preview / trajectory 迁出后，`simulation_input` authority 曾存在 shared schema / geometry helper / runtime concrete 边界歧义；现已明确冻结 | `README.md:5`；`contracts/README.md:13-28` | `modules/dxf-geometry/contracts/README.md`；`modules/dxf-geometry/application/engineering_data/contracts/simulation_input.py`；`shared/contracts/engineering/README.md`；`shared/contracts/engineering/docs/versioning.md` | frozen | shared schema authority + runtime-execution concrete owner + dxf-geometry geometry helper | high |
| DG-R013 | P2 | `domain/README.md`、`services/README.md`、`examples/README.md`、`tests/regression/README.md` | 已冻结的 canonical skeleton 说明文件 | 这些目录此前只存在占位内容，是否应该删除存在歧义；现已依据 workspace layout gate 明确重分类为 shell-only canonical skeleton，而不是待删除 residue | `scripts/migration/validate_workspace_layout.py` 要求 canonical skeleton 物理存在；`README.md` 与 `tests/README.md` 已把 live roots 和 skeleton-only roots 区分冻结 | `modules/dxf-geometry/domain/README.md`；`modules/dxf-geometry/services/README.md`；`modules/dxf-geometry/examples/README.md`；`modules/dxf-geometry/tests/regression/README.md`；`scripts/migration/validate_workspace_layout.py` | reclassified | canonical skeleton（non-live root） | high |

## 7. Hotspots
- `application/engineering_data/contracts/simulation_input.py`
  - 现为 stable compat shell；后续若再引入 runtime defaults、trigger 投影或 payload 组装，即属 owner 回归。
- `modules/runtime-execution/application/runtime_execution/`
  - 当前 simulation-input runtime concrete owner；若与 shared schema 或 geometry helper 发生语义漂移，应以此处为首查点。
- `contracts/README.md`
  - 已写入 `simulation_input` authority 最终口径；后续只应跟随 owner 变化微调，不应回到 residual 叙述。
- `module.yaml`
  - build truth 已基本对齐；本轮未再触碰。
- `domain/README.md`、`services/README.md`、`examples/README.md`、`tests/regression/README.md`
  - 现已冻结为 canonical skeleton；只有实际 owner payload 落地时才允许把它们升格为 live roots 或 live regression suite。

## 8. False Friends
- `siligen_dxf_geometry_pb_path_source_adapter`
  - 是当前唯一 canonical parser target；若 live consumer 再次直连其他历史 target 名，即属回归。
- `application/engineering_data/contracts/simulation_input.py`
  - 看起来像完整 canonical contract root，实际仍同时承担 stable import surface 与 residual delegator。
- `application/engineering_data/residual/`
  - 看起来像普通内部目录，实际是当前 `simulation runtime concrete` 的 quarantine 真值位置。
- `scripts/engineering-data/path_to_trajectory.py`
  - 看起来像 `dxf-geometry` owner 脚本，实际只是稳定 workspace 入口，真实 owner 已迁到 `motion-planning`。
- `scripts/engineering-data/generate_preview.py`
  - 看起来像 `dxf-geometry` owner 脚本，实际只是稳定 workspace 入口，真实 owner 已迁到 `apps/planner-cli`。
- `modules/dxf-geometry/tests/python`
  - 看起来像普通补充测试目录，实际承担 `dxf-geometry` 现存 Python owner 面的 closeout gate，并证明 preview / trajectory 已迁出。

## 9. Closeout Blockers
- none

## 10. No-Change Zone
- `shared/contracts/engineering/**`
  - 本轮仅作为 authority artifact 用于冻结边界，不在本台账执行范围内扩散处理。
- `apps/runtime-service/**`、`modules/job-ingest/**`、`modules/workflow/**`、`modules/process-planning/**`
  - 本轮仅作为 live consumer / dependency evidence，不进入本轮 residue 处置。
- `adapters/dxf/PbPathSourceAdapter.cpp`
  - 当前是 live runtime parser seam；在 owner split 与 public surface freeze 完成前不应随意改动。
- `tests/golden/dxf_to_pb.args.summary.txt`
  - 当前用于冻结命令快照，只作为证据工件，不在本轮扩散。

## 11. Next-Phase Input
- skeleton freeze items
  - 保持 `domain/`、`services/`、`examples/` 与 `tests/regression/` 为 workspace layout 要求的 canonical skeleton；只有实际 owner payload 落地时才允许把它们重新升格为 live roots / live regression suite。
- contract freeze items
  - 冻结 `PathBundle` / PB parser 的唯一 authority seam。
  - 保持 `simulation_input` 的既有冻结口径：shared schema authority + runtime-execution concrete owner + dxf-geometry geometry helper。
- test realignment items
  - 保持 `export_simulation_input` 的 owner gate、workspace CLI smoke 与仓库级 consumer tests 分工，不要回退到 residual-backed 断言。

## 12. Suggested Execution Order
- 1. 保持已冻结 contract
  - 最小 acceptance criteria：`simulation_input` 继续维持 shared schema authority + runtime-execution concrete owner + dxf-geometry geometry helper 的口径，不回退到 residual-backed 叙述。
- 2. 保持 skeleton freeze
  - 最小 acceptance criteria：`domain/`、`services/`、`examples/` 维持 shell-only；`tests/regression/` 维持 canonical skeleton 且不注册 regression target。
- 3. 维护 test/doc realignment
  - 最小 acceptance criteria：owner gate、workspace CLI smoke、仓库级 consumer 证据分工持续清晰，且 `README` / `contracts/README` / 审计文档口径一致。
